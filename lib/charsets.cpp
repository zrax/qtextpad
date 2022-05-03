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

#include <map>
#include <unicode/ucnv.h>

Q_LOGGING_CATEGORY(CsLog, "qtextpad.charsets", QtInfoMsg)

struct TextCodecCache
{
    ~TextCodecCache()
    {
        for (TextCodec *codec : m_cache)
            delete codec;
    }

    QSet<TextCodec *> m_cache;
};
static TextCodecCache s_codecs;

class Icu_TextCodec : public TextCodec
{
public:
    Icu_TextCodec(UConverter *converter) : m_converter(converter) { }

    static TextCodec *create(const QByteArray &name)
    {
        for (TextCodec *cacheCodec : s_codecs.m_cache) {
            auto checkCodec = dynamic_cast<Icu_TextCodec *>(cacheCodec);
            if (checkCodec && checkCodec->name() == name)
                return cacheCodec;
        }

        UErrorCode err = U_ZERO_ERROR;
        UConverter *converter = ucnv_open(name.constData(), &err);
        if (converter) {
            auto newCodec = new Icu_TextCodec(converter);
            s_codecs.m_cache.insert(newCodec);
            return newCodec;
        }
        return Q_NULLPTR;
    }

    ~Icu_TextCodec() override
    {
        ucnv_close(m_converter);
    }

    QByteArray name() const override
    {
        UErrorCode err = U_ZERO_ERROR;
        return ucnv_getName(m_converter, &err);
    }

    QByteArray fromUnicode(const QString &text, bool addHeader) override
    {
        static_assert(sizeof(UChar) == sizeof(QChar),
                      "This code assumes UChar and QChar are both UTF-16 types.");
        std::vector<UChar> buffer;
        buffer.reserve(text.size() + (addHeader ? 1 : 0));
        buffer.assign((const UChar *)text.constData(), (const UChar *)text.constData() + text.size());
        if (addHeader && (buffer.empty() || buffer.front() != 0xFEFF))
            buffer.insert(buffer.begin(), 0xFEFF);

        int maxLength = UCNV_GET_MAX_BYTES_FOR_STRING(text.length(), ucnv_getMaxCharSize(m_converter));
        QByteArray output(maxLength, Qt::Uninitialized);

        int convBytes = 0;
        const UChar *inptr = buffer.data();
        char *outptr = output.data();
        for ( ;; ) {
            UErrorCode err = U_ZERO_ERROR;
            ucnv_fromUnicode(m_converter, &outptr, output.data() + output.size(),
                             &inptr, buffer.data() + buffer.size(), nullptr, false, &err);
            if (!U_SUCCESS(err)) {
                qCDebug(CsLog, "ucnv_fromUnicode failed: %s", u_errorName(err));
                return QByteArray();
            }
            convBytes = outptr - output.data();
            if (inptr >= buffer.data() + buffer.size())
                break;
            output.resize(output.length() * 2);
        }

        output.resize(convBytes);
        return output;
    }

    QString toUnicode(const QByteArray &text) override
    {
        static_assert(sizeof(UChar) == sizeof(QChar),
                      "This code assumes UChar and QChar are both UTF-16 types.");
        std::vector<UChar> buffer;
        buffer.resize(text.size());

        int convChars = 0;
        const char *inptr = text.constData();
        UChar *outptr = buffer.data();
        for ( ;; ) {
            UErrorCode err = U_ZERO_ERROR;
            ucnv_toUnicode(m_converter, &outptr, buffer.data() + buffer.size(),
                           &inptr, text.constData() + text.length(), nullptr, false, &err);
            if (!U_SUCCESS(err) && err != U_BUFFER_OVERFLOW_ERROR) {
                qCDebug(CsLog, "ucnv_toUnicode failed: %s", u_errorName(err));
                return QString();
            }

            convChars = outptr - buffer.data();
            if (inptr >= text.constData() + text.length())
                break;
            buffer.resize(buffer.size() * 2);
        }

        return QString((const QChar *)buffer.data(), convChars);
    }

    bool canDecode(const QByteArray &text) override
    {
        if (text.isEmpty())
            return true;
        return !toUnicode(text).isEmpty();
    }

private:
    UConverter *m_converter;
};

TextCodec *QTextPadCharsets::codecForName(const QByteArray &name)
{
    return Icu_TextCodec::create(name);
}

TextCodec *QTextPadCharsets::codecForLocale()
{
    return Icu_TextCodec::create(ucnv_getDefaultName());
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
        QStringLiteral("cp437"),
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

    std::map<TextCodec *, QStringList> codecDupes;

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
                codecDupes[codec].append(name);
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

    for (const auto &dupe : codecDupes) {
        if (dupe.second.size() > 1) {
            qCDebug(CsLog, "Duplicate codecs for %s:", dupe.first->name().constData());
            for (const auto &name : dupe.second)
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
