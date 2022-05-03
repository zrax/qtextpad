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

#ifndef _CHARSETS_H
#define _CHARSETS_H

#include <QStringList>
#include <QCoreApplication>

class TextCodec
{
public:
    virtual QString name() const = 0;

    virtual QByteArray fromUnicode(const QString &text, bool addHeader) = 0;
    virtual QString toUnicode(const QByteArray &text) = 0;
    virtual bool canDecode(const QByteArray &text) = 0;

protected:
    TextCodec() { }
    virtual ~TextCodec() { }

    friend struct TextCodecCache;
};

// Simplified version of KCharsets with more standard names and fewer duplicates
class QTextPadCharsets
{
    Q_DECLARE_TR_FUNCTIONS(QTextPadCharsets)

public:
    static TextCodec *codecForName(const QString &name);
    static TextCodec *codecForMib(int mib);
    static TextCodec *codecForLocale();

    static QList<QStringList> encodingsByScript();

private:
    QTextPadCharsets();
    static QTextPadCharsets *instance();

    QList<QStringList> m_encodingCache;
};

#endif // _CHARSETS_H
