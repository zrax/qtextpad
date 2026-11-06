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

#ifndef QTEXTPAD_WINDOW_H
#define QTEXTPAD_WINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QLocale>

#include "filetypeinfo.h"

class SyntaxTextEdit;
class SearchWidget;
class ActivationLabel;

class QTabWidget;
class QToolButton;
class QMenu;
class QActionGroup;
class QUndoStack;
class QUndoCommand;
class QFileSystemWatcher;

namespace KSyntaxHighlighting
{
    class Definition;
    class Theme;
}

class DocumentTab : public QWidget
{
public:
    explicit DocumentTab(QWidget *parent = Q_NULLPTR);

    SyntaxTextEdit *editor() const { return m_editor; }
    QUndoStack *undoStack() const { return m_undoStack; }

    QString filename;
    unsigned int fileState = 0;
    QDateTime cachedModTime;
    QString textEncoding;
    FileTypeInfo::LineEndingType lineEndingMode = FileTypeInfo::LFOnly;
    bool utfBOM = false;

private:
    SyntaxTextEdit *m_editor;
    QUndoStack *m_undoStack;
};

class QTextPadWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit QTextPadWindow(QWidget *parent = Q_NULLPTR);

    // Editor of the currently active tab
    SyntaxTextEdit *editor() const;

    void setSyntax(const KSyntaxHighlighting::Definition &syntax);
    void setEditorTheme(const KSyntaxHighlighting::Theme &theme);
    void setDefaultEditorTheme();
    void setEncoding(const QString &codecName);

    QString textEncoding() const;

    bool utfBOM() const;
    void setUtfBOM(bool bom);

    void setOverwriteMode(bool overwrite);
    void setAutoIndent(bool ai);

    void setLineEndingMode(FileTypeInfo::LineEndingType mode);
    FileTypeInfo::LineEndingType lineEndingMode() const;

    bool saveDocumentTo(const QString &filename);
    bool loadDocumentFrom(const QString &filename,
                          const QString &textEncoding = QString());
    bool openDocument(const QString &filename,
                      const QString &textEncoding = QString());
    bool isDocumentModified() const;
    bool documentExists() const;

    void gotoLine(int line, int column = 0);

public Q_SLOTS:
    void checkForModifications();
    bool promptForSave();
    bool promptForDiscard();
    void newDocument();
    void closeTab(int index);
    void currentTabChanged(int index);
    void resetEditor();
    bool saveDocument();
    bool saveDocumentAs();
    bool saveDocumentCopy();
    bool loadDocument();
    bool reloadDocument();
    void reloadDocumentEncoding(const QString &textEncoding);
    void printDocument();
    void printPreviewDocument();

    void editorContextMenu(const QPoint& pos);
    void updateCursorPosition();
    void nextInsertMode();
    void nextLineEndingMode();
    void updateIndentStatus();
    void chooseEditorFont();
    void promptIndentSettings();
    void promptLongLineWidth();
    void navigateToLine();
    void toggleFilePath(bool show);

    void insertDateTime(QLocale::FormatType type);
    void upcaseSelection();
    void downcaseSelection();
    void joinLines();

    void showAbout();
    void toggleFullScreen(bool fullScreen);
    void showSearchBar(bool show);

    // User-triggered actions that store commands in the Undo stack
    void changeEncoding(const QString &encoding);
    void changeLineEndingMode(FileTypeInfo::LineEndingType mode);
    void changeUtfBOM();

protected:
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

    bool eventFilter(QObject *, QEvent *event) Q_DECL_OVERRIDE;

private:
    enum FileState
    {
        FS_New = 0x01,
        FS_OutOfDate = 0x02,
    };

    QTabWidget *m_tabs;
    SearchWidget *m_searchWidget = Q_NULLPTR;
    bool m_closing = false;

    QFileSystemWatcher *m_fileWatcher;
    void setOpenFilename(const QString &filename);

    QToolBar *m_toolBar;
    QMenu *m_recentFiles;
    QMenu *m_themeMenu;
    QMenu *m_syntaxMenu;
    QMenu *m_setEncodingMenu;
    bool m_showFilePath;

    // QAction caches
    QAction *m_reloadAction;
    QAction *m_undoAction = Q_NULLPTR;
    QAction *m_redoAction = Q_NULLPTR;
    QAction *m_deleteAction = Q_NULLPTR;
    QAction *m_overwriteModeAction;
    QAction *m_utfBOMAction;
    QAction *m_autoIndentAction;
    QAction *m_fullScreenAction;
    QActionGroup *m_themeActions;
    QAction *m_defaultThemeAction;
    QActionGroup *m_syntaxActions;
    QActionGroup *m_setEncodingActions;
    QActionGroup *m_lineEndingActions;
    QActionGroup *m_tabWidthActions;
    QActionGroup *m_indentWidthActions;
    QActionGroup *m_indentModeActions;
    QList<QAction *> m_editorContextActions;

    ActivationLabel *m_positionLabel = Q_NULLPTR;
    ActivationLabel *m_crlfLabel;
    ActivationLabel *m_insertLabel;
    QToolButton *m_indentButton;
    QToolButton *m_encodingButton;
    QToolButton *m_syntaxButton;

    // Active tab helpers
    DocumentTab *currentTab() const;
    QUndoStack *undoStack() const;
    QList<SyntaxTextEdit *> editors() const;
    DocumentTab *findTab(const QString &filename,
                         const DocumentTab *exclude = Q_NULLPTR) const;
    DocumentTab *addDocumentTab();
    void updateTabText(DocumentTab *tab);
    void updateSyntaxStatus(const KSyntaxHighlighting::Definition &syntax);
    void syncIndentSettings();

    QString documentTitle();
    void updateTitle();
    void populateRecentFiles();
    void populateThemeMenu();
    void populateSyntaxMenu();
    void populateEncodingMenu();
    void populateIndentButtonMenu();
};

#endif // QTEXTPAD_WINDOW_H
