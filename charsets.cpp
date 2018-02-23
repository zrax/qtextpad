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

#include <QTextCodec>
#include <QCoreApplication>

#define trCharsets(text) QCoreApplication::translate("QTextPadCharsets", text)

// Greatly simplified version from KCharsets with no Latin-1 fallback
QTextCodec *QTextPadCharsets::codecForName(const QString &name)
{
    // KCharsets handles this one specially...  Not sure why
    if (name == QLatin1String("gb2312") || name == QLatin1String("gbk"))
        return QTextCodec::codecForName("gb18030");

    QByteArray nameLatin1 = name.toLatin1();
    auto codec = QTextCodec::codecForName(nameLatin1);
    if (codec)
        return codec;

    nameLatin1 = nameLatin1.toLower();
    if (nameLatin1.endsWith("_charset"))
        nameLatin1.chop(8);
    if (nameLatin1.startsWith("x-"))
        nameLatin1.remove(0, 2);

    if (name.isEmpty())
        return Q_NULLPTR;

    return QTextCodec::codecForName(nameLatin1);
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
        trCharsets("Arabic"),
        "ISO-8859-6",
        "windows-1256",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Baltic"),
        "ISO-8859-4",
        "ISO-8859-13",
        "windows-1257",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Central European"),
        "ISO-8859-2",
        "ISO-8859-3",
        "ISO-8859-10",
        "windows-1250",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Chinese Simplified"),
        "GB18030",
        "GBK",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Chinese Traditional"),
        "Big5",
        "Big5-HKSCS",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Cyrillic"),
        "IBM866",
        "ISO-8859-5",
        "KOI8-R",
        "KOI8-U",
        "windows-1251",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Eastern European"),
        "ISO-8859-16",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Greek"),
        "ISO-8859-7",
        "windows-1253",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Hebrew"),
        "ISO-8859-8",
        "windows-1255",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Indic"),
        "iscii-bng",
        "iscii-dev",
        "iscii-gjr",
        "iscii-knd",
        "iscii-mlm",
        "iscii-ori",
        "iscii-pnj",
        "iscii-tlg",
        "iscii-tml",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Japanese"),
        "EUC-JP",
        "ISO-2022-JP",
        "Shift_JIS",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Korean"),
        "windows-949",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Thai"),
        "TIS-620",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Turkish"),
        "ISO-8859-9",
        "windows-1254",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Western European"),
        "IBM850",
        "ISO-8859-1",
        "ISO-8859-14",
        "ISO-8859-15",
        "hp-roman8",
        "windows-1252",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Other"),
        "macintosh",
        "TSCII",
        "windows-1258",
    });
    m_encodingCache.append(QStringList {
        trCharsets("Unicode"),
        "UTF-7",
        "UTF-8",
        "UTF-16LE",
        "UTF-16BE",
        "UTF-32LE",
        "UTF-32BE",
    });

    // Prune encodings that aren't supported by Qt or the platform
    for (auto &encodingList : m_encodingCache) {
        int enc = 1;
        while (enc < encodingList.size()) {
            if (!QTextCodec::codecForName(encodingList.at(enc).toLatin1()))
                encodingList.removeAt(enc);
            else
                ++enc;
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
