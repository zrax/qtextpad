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

#ifndef QTEXTPAD_CHARSETS_H
#define QTEXTPAD_CHARSETS_H

#include <QSet>
#include <QByteArray>
#include <QStringList>
#include <QCoreApplication>

typedef struct UConverter UConverter;

class TextCodec
{
public:
    QByteArray name() const { return m_name; }
    QByteArray icuName() const;

    QByteArray fromUnicode(const QString &text, bool addHeader);
    QString toUnicode(const QByteArray &text);
    bool canDecode(const QByteArray &text);

    static TextCodec *create(const QByteArray &name);

    static QString icuVersion();

private:
    UConverter *m_converter;
    QByteArray m_name;

    TextCodec(UConverter *converter, QByteArray name)
        : m_converter(converter), m_name(std::move(name)) { }
    ~TextCodec();

    friend struct TextCodecCache;
};

// Simplified version of KCharsets with more standard names and fewer duplicates
class QTextPadCharsets
{
    Q_DECLARE_TR_FUNCTIONS(QTextPadCharsets)

public:
    static TextCodec *codecForName(const QByteArray &name);
    static TextCodec *codecForLocale();

    static QList<QStringList> encodingsByScript();

    static QByteArray getPreferredName(const QByteArray &codecName);

private:
    QTextPadCharsets();
    static QTextPadCharsets *instance();

    QList<QStringList> m_encodingCache;
};

#endif // QTEXTPAD_CHARSETS_H
