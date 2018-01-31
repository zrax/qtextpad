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

#ifndef _SEARCHDIALOG_H
#define _SEARCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QTextCursor>

class QComboBox;
class QCheckBox;
class QTextCursor;
class SyntaxTextEdit;
class QTextPadWindow;

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    static SearchDialog *create(QTextPadWindow *parent, bool replace);
    static SearchDialog *current();

    ~SearchDialog() Q_DECL_OVERRIDE;

    void showReplace(bool show);

    static QTextCursor searchNext(QTextPadWindow *parent, bool reverse);

private slots:
    void searchForward();
    void searchBackward();
    void replaceCurrent();
    void replaceAll();

private:
    explicit SearchDialog(QWidget *parent);

    void syncSearchSettings(bool saveRecent);
    static QTextCursor performSearch(const QTextCursor &start, bool reverse);

    SyntaxTextEdit *editor();

    QComboBox *m_searchText;
    QComboBox *m_replaceText;
    QCheckBox *m_caseSensitive;
    QCheckBox *m_wholeWord;
    QCheckBox *m_regex;
    QCheckBox *m_escapes;
    QCheckBox *m_wrapSearch;
    QCheckBox *m_inSelection;
    QList<QWidget *> m_replaceWidgets;
    QTextCursor m_replaceCursor;
};

#endif // _SEARCHDIALOG_H
