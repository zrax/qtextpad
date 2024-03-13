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

#ifndef QTEXTPAD_SEARCHDIALOG_H
#define QTEXTPAD_SEARCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QTextCursor>
#include <QRegularExpressionMatch>

#include "syntaxtextedit.h"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QTextCursor;
class SyntaxTextEdit;
class QTextPadWindow;

class SearchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(QTextPadWindow *parent);

    QSize sizeHint() const override
    {
        // Make the default just a bit wider
        const QSize parentHint = QWidget::sizeHint();
        return { (parentHint.width() * 5) / 4, parentHint.height() };
    }

    void setSearchText(const QString &text);
    void activate();

public Q_SLOTS:
    void searchNext(bool reverse);

protected:
    void paintEvent(QPaintEvent *event) override;

private Q_SLOTS:
    void updateSettings();

private:
    QLineEdit *m_searchText;
    QAction *m_caseSensitive;
    QAction *m_wholeWord;
    QAction *m_regex;
    QAction *m_escapes;
    QAction *m_wrapSearch;

    SyntaxTextEdit *m_editor;
    SyntaxTextEdit::SearchParams m_searchParams;
};

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    static SearchDialog *create(QTextPadWindow *parent);

    ~SearchDialog() Q_DECL_OVERRIDE;

    static QString translateEscapes(const QString &text);
    static QString regexReplace(const QString &text,
                                const QRegularExpressionMatch &regexMatch);

public Q_SLOTS:
    QTextCursor searchNext(bool reverse);

private Q_SLOTS:
    void searchForward();
    void searchBackward();
    void replaceCurrent();

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
    QPushButton *m_replaceSelectionButton;
    QTextCursor m_replaceCursor;

    SyntaxTextEdit *m_editor;
    SyntaxTextEdit::SearchParams m_searchParams;
    QRegularExpressionMatch m_regexMatch;
};

#endif // QTEXTPAD_SEARCHDIALOG_H
