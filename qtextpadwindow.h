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
class QUndoStack;
class QUndoCommand;

namespace KSyntaxHighlighting
{
    class Definition;
    class Theme;
}

class QTextPadWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit QTextPadWindow(QWidget *parent = Q_NULLPTR);

    SyntaxTextEdit *editor() { return m_editor; }

    void setSyntax(const KSyntaxHighlighting::Definition &syntax);
    void setTheme(const KSyntaxHighlighting::Theme &theme);
    void setEncoding(const QString &codecName);

    QString textEncoding() const { return m_textEncoding; }

    bool utfBOM() const;
    void setUtfBOM(bool bom);

    void setOverwriteMode(bool overwrite);
    void setAutoIndent(bool ai);

    enum LineEndingMode
    {
        CROnly,
        LFOnly,
        CRLF,
    };
    void setLineEndingMode(LineEndingMode mode);
    LineEndingMode lineEndingMode() const { return m_lineEndingMode; }

    bool saveDocumentTo(const QString &filename);
    bool loadDocumentFrom(const QString &filename,
                          const QString &textEncoding = QString());
    bool isDocumentModified() const;

    void gotoLine(int line, int column = 0);

public slots:
    bool promptForSave();
    bool promptForDiscard();
    void newDocument();
    bool saveDocument();
    bool saveDocumentAs();
    bool saveDocumentCopy();
    bool loadDocument();
    bool reloadDocument();
    void reloadDocumentEncoding(const QString &textEncoding);

    void editorContextMenu(const QPoint& pos);
    void updateCursorPosition();
    void nextInsertMode();
    void nextLineEndingMode();
    void updateIndentStatus();
    void chooseEditorFont();
    void promptTabSettings();
    void promptLongLineWidth();
    void navigateToLine();
    void toggleFilePath(bool show);

    // User-triggered actions that store commands in the Undo stack
    void changeEncoding(const QString &encoding);
    void changeLineEndingMode(LineEndingMode mode);
    void changeUtfBOM();

protected:
    void closeEvent(QCloseEvent *e) Q_DECL_OVERRIDE;

private:
    SyntaxTextEdit *m_editor;
    QString m_openFilename;
    QString m_textEncoding;

    QToolBar *m_toolBar;
    QMenu *m_recentFiles;
    QMenu *m_loadEncodingMenu;
    QMenu *m_themeMenu;
    QMenu *m_syntaxMenu;
    QMenu *m_setEncodingMenu;

    // QAction caches
    QAction *m_reloadAction;
    QAction *m_overwriteModeAction;
    QAction *m_utfBOMAction;
    QAction *m_autoIndentAction;
    QActionGroup *m_loadEncodingActions;
    QActionGroup *m_themeActions;
    QActionGroup *m_syntaxActions;
    QActionGroup *m_setEncodingActions;
    QActionGroup *m_lineEndingActions;
    QActionGroup *m_tabWidthActions;
    QActionGroup *m_indentWidthActions;
    QActionGroup *m_indentModeActions;
    QList<QAction *> m_editorContextActions;

    ActivationLabel *m_positionLabel;
    ActivationLabel *m_crlfLabel;
    ActivationLabel *m_insertLabel;
    QToolButton *m_indentButton;
    QToolButton *m_encodingButton;
    QToolButton *m_syntaxButton;
    LineEndingMode m_lineEndingMode;

    // Custom Undo Stack for adding non-editor undo items
    QUndoStack *m_undoStack;
    void addUndoCommand(QUndoCommand *command);

    QString documentTitle();
    void updateTitle();
    void populateRecentFiles();
    void populateThemeMenu();
    void populateSyntaxMenu();
    void populateEncodingMenu();
    void populateIndentButtonMenu();
};

#endif // _QTEXTPADWINDOW_H
