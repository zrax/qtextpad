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

class QLabel;
class QComboBox;
class QCheckBox;
class QPushButton;
class QTextCursor;
class SyntaxTextEdit;
class QTextPadWindow;

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    static SearchDialog *create(QTextPadWindow *parent, bool replace);

    ~SearchDialog() Q_DECL_OVERRIDE;

    static QString translateEscapes(const QString &text);
    static QString regexReplace(const QString &text,
                                const QRegularExpressionMatch &regexMatch);

    void showReplace(bool show);

    static QTextCursor searchNext(QTextPadWindow *parent, bool reverse);

private slots:
    void searchForward();
    void searchBackward();
    void replaceCurrent();
    void replaceAll();
    void replaceInSelection();

private:
    explicit SearchDialog(QWidget *parent);

    void syncSearchSettings(bool saveRecent);

    enum ReplaceAllMode { WholeDocument, InSelection };
    void performReplaceAll(ReplaceAllMode mode);

    QComboBox *m_searchText;
    QComboBox *m_replaceText;
    QCheckBox *m_caseSensitive;
    QCheckBox *m_wholeWord;
    QCheckBox *m_regex;
    QCheckBox *m_escapes;
    QCheckBox *m_wrapSearch;
    QLabel *m_toggleReplace;
    QPushButton *m_replaceSelectionButton;
    QList<QWidget *> m_replaceWidgets;
    QTextCursor m_replaceCursor;
};

#endif // _SEARCHDIALOG_H
