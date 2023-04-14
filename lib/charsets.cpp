/* This file is part of QTextPad.
 *
 * QTextPad is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QTextPad is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QTextPad.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "charsets.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMap>

#ifdef QTEXTPAD_USE_WIN10_ICU
#include <icu.h>
#else
#include <unicode/ucnv.h>
#endif

Q_LOGGING_CATEGORY(CsLog, "qtextpad.charsets", QtInfoMsg)

struct TextCodecCache
{
    ~TextCodecCache()
    {
        for (TextCodec *codec : m_cache)
            delete codec;
    }

    QMap<QByteArray, TextCodec *> m_cache;
};
static TextCodecCache s_codecs;

TextCodec *TextCodec::create(const QByteArray &name)
{
    if (s_codecs.m_cache.contains(name))
        return s_codecs.m_cache[name];

    UErrorCode err = U_ZERO_ERROR;
    UConverter *converter = ucnv_open(name.constData(), &err);
    if (U_FAILURE(err)) {
        qCDebug(CsLog, "Failed to create UConverter for %s: %s",
                name.constData(), u_errorName(err));
    }
    if (!converter)
        return Q_NULLPTR;

    auto newCodec = new TextCodec(converter, name);
    s_codecs.m_cache[name] = newCodec;
    return newCodec;
}

QString TextCodec::icuVersion()
{
    UVersionInfo versionInfo;
    char versionString[U_MAX_VERSION_STRING_LENGTH];
    u_getVersion(versionInfo);
    u_versionToString(versionInfo, versionString);
    return QString::fromLatin1(versionString);
}

TextCodec::~TextCodec()
{
    ucnv_close(m_converter);
}

QByteArray TextCodec::icuName() const
{
    UErrorCode err = U_ZERO_ERROR;
    const char *name = ucnv_getName(m_converter, &err);
    if (U_FAILURE(err)) {
        qCDebug(CsLog, "Failed to get converter name: %s", u_errorName(err));
        return "Unknown";
    }
    return name;
}

QByteArray TextCodec::fromUnicode(const QString &text, bool addHeader)
{
    static_assert(sizeof(UChar) == sizeof(QChar),
                  "This code assumes UChar and QChar are both UTF-16 types.");
    std::vector<UChar> buffer;
    buffer.reserve(text.size() + (addHeader ? 1 : 0));
    buffer.assign((const UChar *)text.constData(), (const UChar *)text.constData() + text.size());
    if (addHeader && (buffer.empty() || buffer.front() != 0xFEFF))
        buffer.insert(buffer.begin(), 0xFEFF);

    ucnv_reset(m_converter);

    int maxLength = UCNV_GET_MAX_BYTES_FOR_STRING(text.length(), ucnv_getMaxCharSize(m_converter));
    QByteArray output(maxLength, Qt::Uninitialized);

    int convBytes = 0;
    const UChar *inptr = buffer.data();
    const UChar *inend = inptr + buffer.size();
    for ( ;; ) {
        char *outptr = output.data() + convBytes;
        UErrorCode err = U_ZERO_ERROR;
        ucnv_fromUnicode(m_converter, &outptr, output.data() + output.size(),
                         &inptr, inend, nullptr, false, &err);
        if (U_FAILURE(err)) {
            qCDebug(CsLog, "ucnv_fromUnicode failed: %s", u_errorName(err));
            return QByteArray();
        }

        convBytes = outptr - output.data();
        if (inptr >= inend)
            break;
        output.resize(output.length() * 2);
    }

    output.resize(convBytes);
    return output;
}

QString TextCodec::toUnicode(const QByteArray &text)
{
    static_assert(sizeof(UChar) == sizeof(QChar),
                  "This code assumes UChar and QChar are both UTF-16 types.");
    std::vector<UChar> buffer;
    buffer.resize(text.size());

    ucnv_reset(m_converter);

    int convChars = 0;
    const char *inptr = text.constData();
    const char *inend = inptr + text.length();
    UChar *outptr = buffer.data();
    for ( ;; ) {
        UErrorCode err = U_ZERO_ERROR;
        ucnv_toUnicode(m_converter, &outptr, buffer.data() + buffer.size(),
                       &inptr, inend, nullptr, false, &err);
        if (U_FAILURE(err) && err != U_BUFFER_OVERFLOW_ERROR) {
            qCDebug(CsLog, "ucnv_toUnicode failed: %s", u_errorName(err));
            return QString();
        }

        convChars = outptr - buffer.data();
        if (inptr >= inend)
            break;
        buffer.resize(buffer.size() * 2);
    }

    return QString((const QChar *)buffer.data(), convChars);
}

bool TextCodec::canDecode(const QByteArray &text)
{
    if (text.isEmpty())
        return true;

    const void *stopContext = Q_NULLPTR;
    const void *oldContext = Q_NULLPTR;
    UConverterToUCallback oldAction;
    UErrorCode err = U_ZERO_ERROR;
    ucnv_setToUCallBack(m_converter, UCNV_TO_U_CALLBACK_STOP, stopContext,
                        &oldAction, &oldContext, &err);
    if (U_FAILURE(err))
        qCDebug(CsLog, "Failed to set decode callback: %s", u_errorName(err));

    bool result = !toUnicode(text).isEmpty();
    ucnv_setToUCallBack(m_converter, oldAction, oldContext, Q_NULLPTR, Q_NULLPTR, &err);
    if (U_FAILURE(err))
        qCDebug(CsLog, "Failed to reset decode callback: %s", u_errorName(err));

    return result;
}

TextCodec *QTextPadCharsets::codecForName(const QByteArray &name)
{
    return TextCodec::create(name);
}

TextCodec *QTextPadCharsets::codecForLocale()
{
    QByteArray defaultName = ucnv_getDefaultName();
    QByteArray preferredName = QTextPadCharsets::getPreferredName(defaultName);
    if (preferredName.isEmpty())
        return TextCodec::create(defaultName);
    return TextCodec::create(preferredName);
}

// Data originally from KCharsets with a few additions.  However, KCharsets is
// arguably worse than pure QTextCodec since it is wildly inconsistent in the
// formats it uses for encoding names, and still manages to include several
// very confusingly named duplicates (see UTF-16, ucs2, ISO 10646-UCS-2
// for example, all of which are the same ambiguous UTF-16 codec)
QTextPadCharsets::QTextPadCharsets()
{
    m_encodingCache.reserve(17);
    m_encodingCache.append(QStringList {
        tr("Arabic"),
        QStringLiteral("ISO-8859-6"),
        QStringLiteral("windows-1256"),
    });
    m_encodingCache.append(QStringList {
        tr("Baltic"),
        QStringLiteral("ISO-8859-4"),
        QStringLiteral("ISO-8859-13"),
        QStringLiteral("windows-1257"),
    });
    m_encodingCache.append(QStringList {
        tr("Central European"),
        QStringLiteral("ISO-8859-2"),
        QStringLiteral("ISO-8859-3"),
        QStringLiteral("ISO-8859-10"),
        QStringLiteral("windows-1250"),
    });
    m_encodingCache.append(QStringList {
        tr("Chinese Simplified"),
        QStringLiteral("GB18030"),
        QStringLiteral("GBK"),
    });
    m_encodingCache.append(QStringList {
        tr("Chinese Traditional"),
        QStringLiteral("Big5"),
        QStringLiteral("Big5-HKSCS"),
    });
    m_encodingCache.append(QStringList {
        tr("Cyrillic"),
        QStringLiteral("IBM866"),
        QStringLiteral("ISO-8859-5"),
        QStringLiteral("KOI8-R"),
        QStringLiteral("KOI8-U"),
        QStringLiteral("windows-1251"),
    });
    m_encodingCache.append(QStringList {
        tr("Eastern European"),
        QStringLiteral("ISO-8859-16"),
    });
    m_encodingCache.append(QStringList {
        tr("Greek"),
        QStringLiteral("ISO-8859-7"),
        QStringLiteral("windows-1253"),
    });
    m_encodingCache.append(QStringList {
        tr("Hebrew"),
        QStringLiteral("ISO-8859-8"),
        QStringLiteral("windows-1255"),
    });
    m_encodingCache.append(QStringList {
        tr("Indic"),
        QStringLiteral("iscii-bng"),
        QStringLiteral("iscii-dev"),
        QStringLiteral("iscii-gjr"),
        QStringLiteral("iscii-gur"),
        QStringLiteral("iscii-guj"),
        QStringLiteral("iscii-knd"),
        QStringLiteral("iscii-mlm"),
        QStringLiteral("iscii-ori"),
        QStringLiteral("iscii-pnj"),
        QStringLiteral("iscii-tlg"),
        QStringLiteral("iscii-tml"),
    });
    m_encodingCache.append(QStringList {
        tr("Japanese"),
        QStringLiteral("EUC-JP"),
        QStringLiteral("ISO-2022-JP"),
        QStringLiteral("Shift-JIS"),
    });
    m_encodingCache.append(QStringList {
        tr("Korean"),
        QStringLiteral("EUC-KR"),
        QStringLiteral("windows-949"),
    });
    m_encodingCache.append(QStringList {
        tr("Other"),
        QStringLiteral("macintosh"),
        QStringLiteral("IBM437"),
        QStringLiteral("windows-1258"),
    });
    m_encodingCache.append(QStringList {
        tr("Thai"),
        QStringLiteral("IBM874"),
        QStringLiteral("TIS-620"),
    });
    m_encodingCache.append(QStringList {
        tr("Turkish"),
        QStringLiteral("ISO-8859-9"),
        QStringLiteral("windows-1254"),
    });
    m_encodingCache.append(QStringList {
        tr("Western European"),
        QStringLiteral("IBM850"),
        QStringLiteral("ISO-8859-1"),
        QStringLiteral("ISO-8859-14"),
        QStringLiteral("ISO-8859-15"),
        QStringLiteral("hp-roman8"),
        QStringLiteral("windows-1252"),
    });
    m_encodingCache.append(QStringList {
        tr("Unicode"),
        QStringLiteral("UTF-7"),
        QStringLiteral("UTF-8"),
        QStringLiteral("UTF-16LE"),
        QStringLiteral("UTF-16BE"),
        QStringLiteral("UTF-32LE"),
        QStringLiteral("UTF-32BE"),
    });

    QMap<QByteArray, QStringList> codecDupes;

    // Prune encodings that aren't supported by Qt or the platform
    for (auto &encodingList : m_encodingCache) {
        int enc = 1;
        while (enc < encodingList.size()) {
            const QString &name = encodingList.at(enc);
            TextCodec *codec = codecForName(name.toLatin1());
            if (!codec) {
                qCDebug(CsLog, "Removing unsupported codec %s", qPrintable(name));
                encodingList.removeAt(enc);
            } else {
                codecDupes[codec->icuName()].append(name);
                ++enc;
            }
        }
    }
    int script = 0;
    while (script < m_encodingCache.size()) {
        // One entry means only the script name is present with no encodings
        if (m_encodingCache.at(script).size() == 1)
            m_encodingCache.removeAt(script);
        else
            ++script;
    }

    for (auto dupe = codecDupes.constBegin(); dupe != codecDupes.constEnd(); ++dupe) {
        if (dupe.value().size() > 1) {
            qCDebug(CsLog, "Duplicate codecs for %s:", dupe.key().constData());
            for (const auto &name : dupe.value())
                qCDebug(CsLog, "  * %s", qPrintable(name));
        }
    }
}

QTextPadCharsets *QTextPadCharsets::instance()
{
    static QTextPadCharsets s_charsets;
    return &s_charsets;
}

QList<QStringList> QTextPadCharsets::encodingsByScript()
{
    return instance()->m_encodingCache;
}

QByteArray QTextPadCharsets::getPreferredName(const QByteArray &codecName)
{
    UErrorCode err = U_ZERO_ERROR;
    uint16_t count = ucnv_countAliases(codecName.constData(), &err);
    if (U_FAILURE(err)) {
        qCDebug(CsLog, "Failed to query aliases for %s: %s", codecName.constData(),
                u_errorName(err));
    }

    for (uint16_t i = 0; i < count; ++i) {
        QByteArray alias = ucnv_getAlias(codecName.constData(), i, &err);
        if (U_FAILURE(err)) {
            qCDebug(CsLog, "Failed to get alias %u for %s: %s", (unsigned)i,
                    codecName.constData(), u_errorName(err));
        }
        if (s_codecs.m_cache.contains(alias))
            return alias;
    }

    // No match, just return what we were given
    return codecName;
}
