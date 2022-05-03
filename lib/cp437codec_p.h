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

#ifndef CP437CODEC_H
#define CP437CODEC_H

#include <QTextCodec>

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

#endif // CP437CODEC_H
