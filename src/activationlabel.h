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

#ifndef QTEXTPAD_ACTIVATIONLABEL_H
#define QTEXTPAD_ACTIVATIONLABEL_H

#include <QLabel>

class ActivationLabel : public QLabel
{
    Q_OBJECT

public:
    ActivationLabel(QWidget *parent = Q_NULLPTR)
        : QLabel(parent) { }

Q_SIGNALS:
    void activated();

protected:
    void mouseDoubleClickEvent(QMouseEvent *) Q_DECL_OVERRIDE
    {
        Q_EMIT activated();
    }
};

#endif // QTEXTPAD_ACTIVATIONLABEL_H
