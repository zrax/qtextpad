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
#include <QTextCodec>

// Simplified version of KCharsets with more standard names and fewer duplicates
class QTextPadCharsets
{
    Q_DECLARE_TR_FUNCTIONS(QTextPadCharsets)

public:
    static QTextCodec *codecForName(const QString &name);

    static QList<QStringList> encodingsByScript();

private:
    QTextPadCharsets();
    static QTextPadCharsets *instance();

    QList<QStringList> m_encodingCache;
};

// CP 437 (OEM/DOS) codec
class Cp437Codec : public QTextCodec
{
public:
    QByteArray name() const override { return "OEM437"; }
    QList<QByteArray> aliases() const override
    {
        return QList<QByteArray>{ "CP437", "IBM437", "csPC8CodePage437" };
    }

    int mibEnum() const override { return 2011; }

protected:
    QString convertToUnicode(const char *in, int length, ConverterState *state) const override;
    QByteArray convertFromUnicode(const QChar *in, int length, ConverterState *state) const override;
};

#endif // _CHARSETS_H
