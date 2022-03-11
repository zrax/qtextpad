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

#ifndef QTEXTPAD_INDENTSETTINGS_H
#define QTEXTPAD_INDENTSETTINGS_H

#include <QDialog>

class QComboBox;
class QSpinBox;
class SyntaxTextEdit;

class IndentSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IndentSettingsDialog(QWidget *parent);

    void loadSettings(SyntaxTextEdit *editor);
    void applySettings(SyntaxTextEdit *editor);

private:
    QComboBox *m_indentMode;
    QSpinBox *m_tabWidth;
    QSpinBox *m_indentWidth;
};

#endif // QTEXTPAD_INDENTSETTINGS_H
