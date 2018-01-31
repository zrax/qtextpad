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

#ifndef _QTEXTPADWINDOW_H
#define _QTEXTPADWINDOW_H

#include <QMainWindow>

class SyntaxTextEdit;
class ActivationLabel;

class QToolButton;
class QMenu;
class QActionGroup;

namespace KSyntaxHighlighting
{
    class Definition;
    class Theme;
}

class QTextPadWindow : public QMainWindow
{
    Q_OBJECT

public:
    QTextPadWindow(QWidget *parent = 0);

    SyntaxTextEdit *editor() { return m_editor; }

    void setSyntax(const KSyntaxHighlighting::Definition &syntax);
    void setTheme(const KSyntaxHighlighting::Theme &theme);
    void setEncoding(const QString &codecName);

    void setOverwriteMode(bool overwrite);
    void setAutoIndent(bool ai);

    enum LineEndingMode
    {
        CROnly,
        LFOnly,
        CRLF,
    };
    void setLineEndingMode(LineEndingMode mode);

public slots:
    void updateCursorPosition();
    void modificationStatusChanged(bool modified);
    void nextInsertMode();
    void nextLineEndingMode();
    void updateIndentStatus();
    void chooseEditorFont();

protected:
    void closeEvent(QCloseEvent *e) Q_DECL_OVERRIDE;

private:
    SyntaxTextEdit *m_editor;

    QMenu *m_recentFiles;
    QMenu *m_themeMenu;
    QMenu *m_syntaxMenu;
    QMenu *m_encodingMenu;

    // QAction caches
    QAction *m_overwiteModeAction;
    QAction *m_autoIndentAction;
    QActionGroup *m_themeActions;
    QActionGroup *m_syntaxActions;
    QActionGroup *m_encodingActions;
    QActionGroup *m_lineEndingActions;
    QActionGroup *m_tabWidthActions;
    QActionGroup *m_indentWidthActions;
    QActionGroup *m_indentModeActions;

    ActivationLabel *m_positionLabel;
    ActivationLabel *m_crlfLabel;
    ActivationLabel *m_insertLabel;
    QToolButton *m_indentButton;
    QToolButton *m_encodingButton;
    QToolButton *m_syntaxButton;
    LineEndingMode m_lineEndingMode;

    QString m_documentTitle;
    bool m_modified;

    void updateTitle();
    void populateRecentFiles();
    void populateThemeMenu();
    void populateSyntaxMenu();
    void populateEncodingMenu();
    void populateIndentButtonMenu();
};

#endif // _QTEXTPADWINDOW_H
