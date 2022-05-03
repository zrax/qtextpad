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

#include "qtextpadwindow.h"

#include <QActionGroup>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QGridLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFontDialog>
#include <QApplication>
#include <QWidgetAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QClipboard>
#include <QUndoStack>
#include <QTextBlock>
#include <QTimer>
#include <QFileInfo>
#include <QPrinter>
#include <QDateTime>
#include <QProcess>
#include <QFileSystemWatcher>

#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>

#include "activationlabel.h"
#include "syntaxtextedit.h"
#include "settingspopup.h"
#include "searchdialog.h"
#include "definitiondownload.h"
#include "indentsettings.h"
#include "appsettings.h"
#include "undocommands.h"
#include "charsets.h"
#include "aboutdialog.h"

#include <memory>

#define LARGE_FILE_SIZE     (10*1024*1024)  // 10 MiB
#define DETECTION_SIZE      (      4*1024)

class EncodingPopupAction : public QWidgetAction
{
public:
    explicit EncodingPopupAction(QTextPadWindow *parent)
        : QWidgetAction(parent) { }

protected:
    QWidget *createWidget(QWidget *menuParent) Q_DECL_OVERRIDE
    {
        auto popup = new EncodingPopup(menuParent);
        connect(popup, &EncodingPopup::encodingSelected, this,
                [this](const QString &codecName) {
            auto window = qobject_cast<QTextPadWindow *>(parent());
            window->changeEncoding(codecName);

            // Don't close the popup right after clicking, so the user can
            // briefly see the visual feedback for the item they selected.
            QTimer::singleShot(100, []() {
                auto popupWidget = QApplication::activePopupWidget();
                if (popupWidget)
                    popupWidget->close();
            });
        });
        return popup;
    }
};

class SyntaxPopupAction : public QWidgetAction
{
public:
    explicit SyntaxPopupAction(QTextPadWindow *parent)
        : QWidgetAction(parent) { }

protected:
    QWidget *createWidget(QWidget *menuParent) Q_DECL_OVERRIDE
    {
        auto popup = new SyntaxPopup(menuParent);
        connect(popup, &SyntaxPopup::syntaxSelected, this,
                [this](const KSyntaxHighlighting::Definition &syntax) {
            auto window = qobject_cast<QTextPadWindow *>(parent());
            window->setSyntax(syntax);

            // Don't close the popup right after clicking, so the user can
            // briefly see the visual feedback for the item they selected.
            QTimer::singleShot(100, []() {
                auto popupWidget = QApplication::activePopupWidget();
                if (popupWidget)
                    popupWidget->close();
            });
        });
        return popup;
    }
};

QTextPadWindow::QTextPadWindow(QWidget *parent)
    : QMainWindow(parent), m_fileState()
{
    m_editor = new SyntaxTextEdit(this);
    setCentralWidget(m_editor);
    m_editor->setFrameStyle(QFrame::NoFrame);

    m_searchWidget = new SearchWidget(this);
    showSearchBar(false);

    QTextPadSettings settings;
    m_editor->setShowLineNumbers(settings.lineNumbers());
    m_editor->setShowFolding(settings.showFolding());
    m_editor->setAutoIndent(settings.autoIndent());
    m_editor->setMatchBraces(settings.matchBraces());
    m_editor->setHighlightCurrentLine(settings.highlightCurLine());
    m_editor->setShowIndentGuides(settings.indentationGuides());
    m_editor->setShowLongLineEdge(settings.showLongLineMargin());
    m_editor->setShowWhitespace(settings.showWhitespace());
    m_editor->setTabWidth(settings.tabWidth());
    m_editor->setIndentWidth(settings.indentWidth());
    m_editor->setLongLineWidth(settings.longLineWidth());
    m_editor->setDefaultFont(settings.editorFont());
    m_editor->setWordWrap(settings.wordWrap());
    m_editor->setIndentationMode(settings.indentMode());
    m_editor->setScrollPastEndOfFile(settings.scrollPastEndOfFile());

    m_editor->setExternalUndoRedo(true);
    m_undoStack = new QUndoStack(this);
    connect(m_editor->document(), &QTextDocument::undoCommandAdded, this,
            [this] { m_undoStack->push(new TextEditorUndoCommand(m_editor)); });
    connect(m_editor, &SyntaxTextEdit::undoRequested, m_undoStack, &QUndoStack::undo);
    connect(m_editor, &SyntaxTextEdit::redoRequested, m_undoStack, &QUndoStack::redo);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    auto newAction = fileMenu->addAction(ICON("document-new"), tr("&New"));
    newAction->setShortcut(QKeySequence::New);
    auto newWindowAction = fileMenu->addAction(ICON("window-new"), tr("New &Window"));
    newWindowAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_N);
    (void) fileMenu->addSeparator();
    auto openAction = fileMenu->addAction(ICON("document-open"), tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    m_recentFiles = fileMenu->addMenu(tr("Open &Recent"));
    populateRecentFiles();
    m_reloadAction = fileMenu->addAction(ICON("view-refresh"), tr("Re&load"));
    m_reloadAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_R);
    (void) fileMenu->addSeparator();
    auto saveAction = fileMenu->addAction(ICON("document-save"), tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    auto saveAsAction = fileMenu->addAction(ICON("document-save-as"), tr("Save &As..."));
    saveAsAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
    auto saveCopyAction = fileMenu->addAction(ICON("document-save-as"), tr("Save &Copy..."));
    (void) fileMenu->addSeparator();
    auto printAction = fileMenu->addAction(ICON("document-print"), tr("&Print..."));
    printAction->setShortcut(QKeySequence::Print);
    auto printPreviewAction = fileMenu->addAction(ICON("document-preview"), tr("Print Pre&view"));
    (void) fileMenu->addSeparator();
    auto quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);

    connect(newAction, &QAction::triggered, this, &QTextPadWindow::newDocument);
    connect(newWindowAction, &QAction::triggered, [](bool) {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
    });
    connect(openAction, &QAction::triggered, this, &QTextPadWindow::loadDocument);
    connect(m_reloadAction, &QAction::triggered, this, &QTextPadWindow::reloadDocument);
    connect(saveAction, &QAction::triggered, this, &QTextPadWindow::saveDocument);
    connect(saveAsAction, &QAction::triggered, this, &QTextPadWindow::saveDocumentAs);
    connect(saveCopyAction, &QAction::triggered, this, &QTextPadWindow::saveDocumentCopy);
    connect(printAction, &QAction::triggered, this, &QTextPadWindow::printDocument);
    connect(printPreviewAction, &QAction::triggered, this, &QTextPadWindow::printPreviewDocument);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    auto undoAction = editMenu->addAction(ICON("edit-undo"), tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    m_editorContextActions << undoAction;
    auto redoAction = editMenu->addAction(ICON("edit-redo"), tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    m_editorContextActions << redoAction;
    m_editorContextActions << editMenu->addSeparator();
    auto cutAction = editMenu->addAction(ICON("edit-cut"), tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    m_editorContextActions << cutAction;
    auto copyAction = editMenu->addAction(ICON("edit-copy"), tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    m_editorContextActions << copyAction;
    auto pasteAction = editMenu->addAction(ICON("edit-paste"), tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    m_editorContextActions << pasteAction;
    auto clearAction = editMenu->addAction(ICON("edit-delete"), tr("&Delete"));
    clearAction->setShortcut(QKeySequence::Delete);
    m_editorContextActions << clearAction;
    auto deleteLinesAction = editMenu->addAction(tr("De&lete Line(s)"));
    deleteLinesAction->setShortcut(Qt::CTRL | Qt::Key_D);
    m_editorContextActions << editMenu->addSeparator();
    auto selectAllAction = editMenu->addAction(tr("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_editorContextActions << selectAllAction;
    (void) editMenu->addSeparator();
    m_overwriteModeAction = editMenu->addAction(tr("&Overwrite Mode"));
    m_overwriteModeAction->setShortcut(Qt::Key_Insert);
    m_overwriteModeAction->setCheckable(true);
    (void) editMenu->addSeparator();
    auto findAction = editMenu->addAction(ICON("edit-find"), tr("&Find..."));
    findAction->setShortcut(QKeySequence::Find);
    auto findNextAction = editMenu->addAction(tr("Find &Next"));
    findNextAction->setShortcut(Qt::Key_F3);
    auto findPrevAction = editMenu->addAction(tr("Find Pre&vious"));
    findPrevAction->setShortcut(Qt::SHIFT | Qt::Key_F3);
    auto replaceAction = editMenu->addAction(ICON("edit-find-replace"), tr("R&eplace..."));
    replaceAction->setShortcut(Qt::CTRL | Qt::Key_H);
    (void) editMenu->addSeparator();
    auto gotoAction = editMenu->addAction(ICON("go-jump"), tr("&Go to line..."));
    gotoAction->setShortcut(Qt::CTRL | Qt::Key_G);

    connect(undoAction, &QAction::triggered, m_undoStack, &QUndoStack::undo);
    connect(redoAction, &QAction::triggered, m_undoStack, &QUndoStack::redo);
    connect(cutAction, &QAction::triggered, m_editor, &SyntaxTextEdit::cutLines);
    connect(copyAction, &QAction::triggered, m_editor, &SyntaxTextEdit::copyLines);
    connect(pasteAction, &QAction::triggered, m_editor, &QPlainTextEdit::paste);
    connect(clearAction, &QAction::triggered, m_editor, &SyntaxTextEdit::deleteSelection);
    connect(deleteLinesAction, &QAction::triggered, m_editor, &SyntaxTextEdit::deleteLines);
    connect(selectAllAction, &QAction::triggered, m_editor, &QPlainTextEdit::selectAll);
    connect(m_overwriteModeAction, &QAction::toggled,
            this, &QTextPadWindow::setOverwriteMode);

    connect(findAction, &QAction::triggered, this, [this] { showSearchBar(true); });
    connect(findNextAction, &QAction::triggered, this, [this] { m_searchWidget->searchNext(false); });
    connect(findPrevAction, &QAction::triggered, this, [this] { m_searchWidget->searchNext(true); });
    connect(replaceAction, &QAction::triggered, this, [this] { SearchDialog::create(this); });
    connect(gotoAction, &QAction::triggered, this, &QTextPadWindow::navigateToLine);

    connect(m_undoStack, &QUndoStack::canUndoChanged, undoAction, &QAction::setEnabled);
    undoAction->setEnabled(false);
    connect(m_undoStack, &QUndoStack::canRedoChanged, redoAction, &QAction::setEnabled);
    redoAction->setEnabled(false);
    connect(m_editor, &QPlainTextEdit::copyAvailable, clearAction, &QAction::setEnabled);
    clearAction->setEnabled(false);

    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this, pasteAction] {
        pasteAction->setEnabled(m_editor->canPaste());
    });
    pasteAction->setEnabled(m_editor->canPaste());

    // The Editor's default context menu hooks directly into the QTextDocument's
    // undo stack, which won't see our custom actions.  We might as well just
    // use the app's actions for everything else while we're fixing that.
    m_editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_editor, &QPlainTextEdit::customContextMenuRequested,
            this, &QTextPadWindow::editorContextMenu);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_themeMenu = viewMenu->addMenu(tr("&Theme"));
    populateThemeMenu();
    (void) viewMenu->addSeparator();
    auto wordWrapAction = viewMenu->addAction(tr("&Word Wrap"));
    wordWrapAction->setShortcut(Qt::CTRL | Qt::Key_W);
    wordWrapAction->setCheckable(true);
    auto longLineAction = viewMenu->addAction(tr("Long Line &Margin"));
    longLineAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_M);
    longLineAction->setCheckable(true);
    auto longLineWidthAction = viewMenu->addAction(tr("Set Long Line Wi&dth..."));
    auto indentGuidesAction = viewMenu->addAction(tr("&Indentation Guides"));
    indentGuidesAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_T);
    indentGuidesAction->setCheckable(true);
    (void) viewMenu->addSeparator();
    auto showLineNumbersAction = viewMenu->addAction(tr("Line &Numbers"));
    showLineNumbersAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_L);
    showLineNumbersAction->setCheckable(true);
    auto showFoldingAction = viewMenu->addAction(tr("Show &Fold Margin"));
    showFoldingAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_H);
    showFoldingAction->setCheckable(true);
    auto showWhitespaceAction = viewMenu->addAction(tr("Show White&space"));
    showWhitespaceAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_W);
    showWhitespaceAction->setCheckable(true);
    (void) viewMenu->addSeparator();
    auto scrollPastEndOfFileAction = viewMenu->addAction(tr("Scroll &Past End of File"));
    scrollPastEndOfFileAction->setCheckable(true);
    (void) viewMenu->addSeparator();
    auto showCurrentLineAction = viewMenu->addAction(tr("Highlight &Current Line"));
    showCurrentLineAction->setCheckable(true);
    auto showMatchingBraces = viewMenu->addAction(tr("Match &Braces"));
    showMatchingBraces->setCheckable(true);
    (void) viewMenu->addSeparator();
    auto zoomInAction = viewMenu->addAction(ICON("zoom-in"), tr("Zoom &In"));
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    auto zoomOutAction = viewMenu->addAction(ICON("zoom-out"), tr("Zoom &Out"));
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    auto zoomResetAction = viewMenu->addAction(ICON("zoom-original"), tr("Reset &Zoom"));
    zoomResetAction->setShortcut(Qt::CTRL | Qt::Key_0);
    (void) viewMenu->addSeparator();
    m_fullScreenAction = viewMenu->addAction(ICON("view-fullscreen"), tr("&Full Screen"));
    m_fullScreenAction->setShortcut(Qt::Key_F11);
    m_fullScreenAction->setCheckable(true);

    connect(wordWrapAction, &QAction::toggled, this,
            [this](bool wrap) {
                m_editor->setWordWrap(wrap);
                QTextPadSettings().setWordWrap(wrap);
            });
    connect(longLineAction, &QAction::toggled, this,
            [this](bool show) {
                m_editor->setShowLongLineEdge(show);
                QTextPadSettings().setShowLongLineMargin(show);
            });
    connect(longLineWidthAction, &QAction::triggered,
            this, &QTextPadWindow::promptLongLineWidth);
    connect(indentGuidesAction, &QAction::toggled, this,
            [this](bool show) {
                m_editor->setShowIndentGuides(show);
                QTextPadSettings().setIndentationGuides(show);
            });
    connect(showLineNumbersAction, &QAction::toggled, this,
            [this](bool show) {
                m_editor->setShowLineNumbers(show);
                QTextPadSettings().setLineNumbers(show);
            });
    connect(showFoldingAction, &QAction::toggled, this,
            [this](bool show) {
                m_editor->setShowFolding(show);
                QTextPadSettings().setShowFolding(show);
            });
    connect(showWhitespaceAction, &QAction::toggled, this,
            [this](bool show) {
                m_editor->setShowWhitespace(show);
                QTextPadSettings().setShowWhitespace(show);
            });
    connect(scrollPastEndOfFileAction, &QAction::toggled, this,
            [this](bool scroll) {
                m_editor->setScrollPastEndOfFile(scroll);
                QTextPadSettings().setScrollPastEndOfFile(scroll);
            });
    connect(showCurrentLineAction, &QAction::toggled, this,
            [this](bool show) {
                m_editor->setHighlightCurrentLine(show);
                QTextPadSettings().setHighlightCurLine(show);
            });
    connect(showMatchingBraces, &QAction::toggled, this,
            [this](bool show) {
                m_editor->setMatchBraces(show);
                QTextPadSettings().setMatchBraces(show);
            });
    connect(zoomInAction, &QAction::triggered, m_editor, &SyntaxTextEdit::zoomIn);
    connect(zoomOutAction, &QAction::triggered, m_editor, &SyntaxTextEdit::zoomOut);
    connect(zoomResetAction, &QAction::triggered, m_editor, &SyntaxTextEdit::zoomReset);
    connect(m_fullScreenAction, &QAction::toggled, this, &QTextPadWindow::toggleFullScreen);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    auto insertDTL = toolsMenu->addAction(tr("Insert &Date/Time (Long)"));
    insertDTL->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_F5);
    auto insertDTS = toolsMenu->addAction(tr("Insert D&ate/Time (Short)"));
    insertDTS->setShortcut(Qt::CTRL | Qt::Key_F5);
    (void) toolsMenu->addSeparator();
    auto upcaseAction = toolsMenu->addAction(tr("&Uppercase"));
    upcaseAction->setShortcut(Qt::CTRL | Qt::Key_U);
    auto downcaseAction = toolsMenu->addAction(tr("&Lowercase"));
    downcaseAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_U);
    (void) toolsMenu->addSeparator();
    auto linesUpAction = toolsMenu->addAction(tr("Move Lines U&p"));
    linesUpAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Up);
    auto linesDownAction = toolsMenu->addAction(tr("Move Lines Do&wn"));
    linesDownAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Down);
    auto joinLinesAction = toolsMenu->addAction(tr("&Join Lines"));
    joinLinesAction->setShortcut(Qt::CTRL | Qt::Key_J);
    (void) toolsMenu->addSeparator();
    QMenu *foldMenu = toolsMenu->addMenu(tr("Code &Folding"));
    auto foldAction = foldMenu->addAction(tr("&Collapse"));
    foldAction->setShortcut(Qt::CTRL | Qt::Key_BracketLeft);
    auto unfoldAction = foldMenu->addAction(tr("&Expand"));
    unfoldAction->setShortcut(Qt::CTRL | Qt::Key_BracketRight);
    (void) foldMenu->addSeparator();
    auto foldAllAction = foldMenu->addAction(tr("Collapse &All"));
    foldAllAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Minus);
    auto unfoldAllAction = foldMenu->addAction(tr("E&xpand All"));
    unfoldAllAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus);

    connect(insertDTL, &QAction::triggered, this, [this](bool) {
        insertDateTime(QLocale::LongFormat);
    });
    connect(insertDTS, &QAction::triggered, this, [this](bool) {
        insertDateTime(QLocale::ShortFormat);
    });
    connect(upcaseAction, &QAction::triggered, this, &QTextPadWindow::upcaseSelection);
    connect(downcaseAction, &QAction::triggered, this, &QTextPadWindow::downcaseSelection);
    connect(linesUpAction, &QAction::triggered, this, [this](bool) {
        m_editor->moveLines(QTextCursor::PreviousBlock);
    });
    connect(linesDownAction, &QAction::triggered, this, [this](bool) {
        m_editor->moveLines(QTextCursor::NextBlock);
    });
    connect(joinLinesAction, &QAction::triggered, this, &QTextPadWindow::joinLines);
    connect(foldAction, &QAction::triggered, m_editor, &SyntaxTextEdit::foldCurrentLine);
    connect(unfoldAction, &QAction::triggered, m_editor, &SyntaxTextEdit::unfoldCurrentLine);
    connect(foldAllAction, &QAction::triggered, m_editor, &SyntaxTextEdit::foldAll);
    connect(unfoldAllAction, &QAction::triggered, m_editor, &SyntaxTextEdit::unfoldAll);

    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    auto fontAction = settingsMenu->addAction(tr("Editor &Font..."));
    (void) settingsMenu->addSeparator();
    m_syntaxMenu = settingsMenu->addMenu(tr("&Syntax"));
    populateSyntaxMenu();
    m_setEncodingMenu = settingsMenu->addMenu(tr("&Encoding"));
    populateEncodingMenu();
    auto lineEndingMenu = settingsMenu->addMenu(tr("&Line Endings"));
    m_lineEndingActions = new QActionGroup(this);
    auto crOnlyAction = lineEndingMenu->addAction(tr("Classic Mac (CR)"));
    crOnlyAction->setCheckable(true);
    crOnlyAction->setActionGroup(m_lineEndingActions);
    crOnlyAction->setData(static_cast<int>(FileTypeInfo::CROnly));
    auto lfOnlyAction = lineEndingMenu->addAction(tr("UNIX (LF)"));
    lfOnlyAction->setCheckable(true);
    lfOnlyAction->setActionGroup(m_lineEndingActions);
    lfOnlyAction->setData(static_cast<int>(FileTypeInfo::LFOnly));
    auto crlfAction = lineEndingMenu->addAction(tr("Windows/DOS (CRLF)"));
    crlfAction->setCheckable(true);
    crlfAction->setActionGroup(m_lineEndingActions);
    crlfAction->setData(static_cast<int>(FileTypeInfo::CRLF));
    (void) settingsMenu->addSeparator();
    auto indentSettingsAction = settingsMenu->addAction(tr("&Indentation Settings..."));
    m_autoIndentAction = settingsMenu->addAction(tr("&Auto Indent"));
    m_autoIndentAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_I);
    m_autoIndentAction->setCheckable(true);
    (void) settingsMenu->addSeparator();
    auto showToolBarAction = settingsMenu->addAction(tr("Show Tool &Bar"));
    showToolBarAction->setCheckable(true);
    showToolBarAction->setChecked(settings.showToolBar());
    auto showStatusBarAction = settingsMenu->addAction(tr("Show Stat&us Bar"));
    showStatusBarAction->setCheckable(true);
    showStatusBarAction->setChecked(settings.showStatusBar());
    auto showFilePathAction = settingsMenu->addAction(tr("Show &Path in Title Bar"));
    showFilePathAction->setCheckable(true);
    m_showFilePath = settings.showFilePath();
    showFilePathAction->setChecked(m_showFilePath);

    connect(fontAction, &QAction::triggered, this, &QTextPadWindow::chooseEditorFont);
    connect(crOnlyAction, &QAction::triggered, this, [this] { changeLineEndingMode(FileTypeInfo::CROnly); });
    connect(lfOnlyAction, &QAction::triggered, this, [this] { changeLineEndingMode(FileTypeInfo::LFOnly); });
    connect(crlfAction, &QAction::triggered, this, [this] { changeLineEndingMode(FileTypeInfo::CRLF); });
    connect(indentSettingsAction, &QAction::triggered, this, &QTextPadWindow::promptIndentSettings);
    connect(m_autoIndentAction, &QAction::toggled, this, &QTextPadWindow::setAutoIndent);
    connect(showFilePathAction, &QAction::toggled, this, &QTextPadWindow::toggleFilePath);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto aboutAction = helpMenu->addAction(ICON("help-about"), tr("&About..."));
    aboutAction->setShortcut(QKeySequence::HelpContents);

    connect(aboutAction, &QAction::triggered, this, &QTextPadWindow::showAbout);

    m_toolBar = addToolBar(tr("Toolbar"));
    m_toolBar->setIconSize(QSize(22, 22));
    m_toolBar->setMovable(false);
    m_toolBar->addAction(newAction);
    m_toolBar->addAction(openAction);
    m_toolBar->addAction(saveAction);
    (void) m_toolBar->addSeparator();
    m_toolBar->addAction(undoAction);
    m_toolBar->addAction(redoAction);
    (void) m_toolBar->addSeparator();
    m_toolBar->addAction(cutAction);
    m_toolBar->addAction(copyAction);
    m_toolBar->addAction(pasteAction);
    (void) m_toolBar->addSeparator();
    m_toolBar->addAction(findAction);
    m_toolBar->addAction(replaceAction);

    if (!settings.showToolBar())
        m_toolBar->setVisible(false);
    connect(showToolBarAction, &QAction::toggled, m_toolBar, &QToolBar::setVisible);
    connect(m_toolBar->toggleViewAction(), &QAction::toggled,
            showToolBarAction, &QAction::setChecked);

    m_positionLabel = new ActivationLabel(this);
    statusBar()->addWidget(m_positionLabel, 1);
    m_insertLabel = new ActivationLabel(this);
    statusBar()->addPermanentWidget(m_insertLabel);
    m_crlfLabel = new ActivationLabel(this);
    statusBar()->addPermanentWidget(m_crlfLabel);
    m_indentButton = new QToolButton(this);
    m_indentButton->setAutoRaise(true);;
    m_indentButton->setPopupMode(QToolButton::InstantPopup);
    populateIndentButtonMenu();
    statusBar()->addPermanentWidget(m_indentButton);
    m_encodingButton = new QToolButton(this);
    m_encodingButton->setAutoRaise(true);
    m_encodingButton->setPopupMode(QToolButton::InstantPopup);
    m_encodingButton->addAction(new EncodingPopupAction(this));
    statusBar()->addPermanentWidget(m_encodingButton);
    m_syntaxButton = new QToolButton(this);
    m_syntaxButton->setAutoRaise(true);
    m_syntaxButton->setPopupMode(QToolButton::InstantPopup);
    m_syntaxButton->addAction(new SyntaxPopupAction(this));
    statusBar()->addPermanentWidget(m_syntaxButton);
    updateCursorPosition();

    if (!settings.showStatusBar())
        statusBar()->setVisible(false);
    connect(showStatusBarAction, &QAction::toggled, statusBar(), &QStatusBar::setVisible);

    connect(m_positionLabel, &ActivationLabel::activated,
            this, &QTextPadWindow::navigateToLine);
    connect(m_insertLabel, &ActivationLabel::activated,
            this, &QTextPadWindow::nextInsertMode);
    connect(m_crlfLabel, &ActivationLabel::activated,
            this, &QTextPadWindow::nextLineEndingMode);

    wordWrapAction->setChecked(m_editor->wordWrap());
    longLineAction->setChecked(m_editor->showLongLineEdge());
    indentGuidesAction->setChecked(m_editor->showIndentGuides());
    showLineNumbersAction->setChecked(m_editor->showLineNumbers());
    showFoldingAction->setChecked(m_editor->showFolding());
    showWhitespaceAction->setChecked(m_editor->showWhitespace());
    scrollPastEndOfFileAction->setChecked(m_editor->scrollPastEndOfFile());
    showCurrentLineAction->setChecked(m_editor->highlightCurrentLine());
    showMatchingBraces->setChecked(m_editor->matchBraces());
    m_autoIndentAction->setChecked(m_editor->autoIndent());

    KSyntaxHighlighting::Theme theme;
    QString themeName = settings.editorTheme();
    if (!themeName.isEmpty())
        theme = SyntaxTextEdit::syntaxRepo()->theme(themeName);
    if (!theme.isValid())
        theme = SyntaxTextEdit::syntaxRepo()->theme(m_editor->themeName());
    if (theme.isValid())
        setTheme(theme);

    m_insertLabel->setMinimumWidth(QFontMetrics(m_insertLabel->font()).boundingRect(tr("OVR")).width() + 4);
    m_crlfLabel->setMinimumWidth(QFontMetrics(m_crlfLabel->font()).boundingRect(tr("CRLF")).width() + 4);
    setOverwriteMode(false);

    connect(m_editor, &SyntaxTextEdit::cursorPositionChanged,
            this, &QTextPadWindow::updateCursorPosition);
    connect(m_editor, &SyntaxTextEdit::selectionChanged,
            this, &QTextPadWindow::updateCursorPosition);
    connect(m_undoStack, &QUndoStack::cleanChanged, this,
            [this](bool) { updateTitle(); });

    connect(m_editor, &SyntaxTextEdit::textChanged, [this] {
        if (m_searchWidget->isVisible())
            showSearchBar(false);
    });
    connect(qApp, &QApplication::focusChanged, [this](QWidget *, QWidget *focus) {
        if (focus == m_editor && m_searchWidget->isVisible())
            showSearchBar(false);
    });

    // Set up the editor and status for a clean, empty document
    newDocument();

    resize(settings.windowSize());

    // Only check for modifications when the application is focused.  This
    // prevents us from unexpectedly stealing focus from other applications.
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) {
        if (QApplication::applicationState() == Qt::ApplicationActive)
            checkForModifications();
    });
    connect(qApp, &QApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            checkForModifications();
    });

    installEventFilter(this);
}

void QTextPadWindow::setOpenFilename(const QString &filename)
{
    if (!m_openFilename.isEmpty())
        m_fileWatcher->removePath(m_openFilename);
    m_openFilename = filename;
    if (!m_openFilename.isEmpty()) {
        if (!m_fileWatcher->addPath(m_openFilename))
            qWarning("Could not add file system watch for %s", qPrintable(m_openFilename));
    }
}

void QTextPadWindow::showAbout()
{
    AboutDialog about(this);
    about.setModal(true);
    about.exec();
}

void QTextPadWindow::toggleFullScreen(bool fullScreen)
{
    if (fullScreen) {
        m_fullScreenAction->setIcon(ICON("view-restore"));
        showFullScreen();
    } else {
        m_fullScreenAction->setIcon(ICON("view-fullscreen"));
        showNormal();
    }
}

void QTextPadWindow::showSearchBar(bool show)
{
    m_searchWidget->setVisible(show);
    m_searchWidget->setEnabled(show);
    if (show) {
        const QTextCursor cursor = m_editor->textCursor();
        if (cursor.hasSelection())
            m_searchWidget->setSearchText(cursor.selectedText());
        m_searchWidget->activate();
    } else {
        m_editor->clearLiveSearch();
        m_editor->setFocus(Qt::OtherFocusReason);
    }
}

void QTextPadWindow::setSyntax(const KSyntaxHighlighting::Definition &syntax)
{
    m_editor->setSyntax(syntax);
    if (syntax.isValid())
        m_syntaxButton->setText(syntax.translatedName());
    else
        m_syntaxButton->setText(tr("Plain Text"));

    // Update the menus when this is triggered via other callers
    for (const auto &action : m_syntaxActions->actions()) {
        const auto actionDef = action->data().value<KSyntaxHighlighting::Definition>();
        if (actionDef == syntax) {
            action->setChecked(true);
            break;
        }
    }
}

void QTextPadWindow::setTheme(const KSyntaxHighlighting::Theme &theme)
{
    m_editor->setTheme(theme);

    // Update the menus when this is triggered via other callers
    for (const auto &action : m_themeActions->actions()) {
        const auto actionTheme = action->data().value<KSyntaxHighlighting::Theme>();
        if (actionTheme.filePath() == theme.filePath()) {
            action->setChecked(true);
            break;
        }
    }

    QTextPadSettings().setEditorTheme(theme.name());
}

void QTextPadWindow::setEncoding(const QString &codecName)
{
    m_textEncoding = codecName;

    // We may not directly match the passed encoding, so don't show a
    // radio check at all if we can't find the encoding.
    if (m_setEncodingActions->checkedAction())
        m_setEncodingActions->checkedAction()->setChecked(false);

    if (!QTextPadCharsets::codecForName(codecName)) {
        qWarning("Invalid codec selected");
        m_encodingButton->setText(tr("Invalid (%1)").arg(codecName));
    } else {
        // Use the passed name for UI consistency
        m_encodingButton->setText(codecName);
    }

    // Update the menus when this is triggered via other callers
    for (const auto &action : m_setEncodingActions->actions()) {
        const auto actionCodec = action->data().toString();
        if (actionCodec == codecName) {
            action->setChecked(true);
            break;
        }
    }
}

bool QTextPadWindow::utfBOM() const
{
    return m_utfBOMAction->isChecked();
}

void QTextPadWindow::setUtfBOM(bool bom)
{
    m_utfBOMAction->setChecked(bom);
}

void QTextPadWindow::setOverwriteMode(bool overwrite)
{
    m_editor->setOverwriteMode(overwrite);
    m_overwriteModeAction->setChecked(overwrite);
    m_insertLabel->setText(overwrite ? tr("OVR") : tr("INS"));
}

void QTextPadWindow::setAutoIndent(bool ai)
{
    m_editor->setAutoIndent(ai);
    m_autoIndentAction->setChecked(ai);
    QTextPadSettings().setAutoIndent(ai);
    updateIndentStatus();
}

void QTextPadWindow::setLineEndingMode(FileTypeInfo::LineEndingType mode)
{
    m_lineEndingMode = mode;

    switch (mode) {
    case FileTypeInfo::CROnly:
        m_crlfLabel->setText(tr("CR"));
        break;
    case FileTypeInfo::LFOnly:
        m_crlfLabel->setText(tr("LF"));
        break;
    case FileTypeInfo::CRLF:
        m_crlfLabel->setText(tr("CRLF"));
        break;
    }

    // Update the menus when this is triggered via other callers
    for (const auto &action : m_lineEndingActions->actions()) {
        if (action->data().toInt() == static_cast<int>(mode)) {
            action->setChecked(true);
            break;
        }
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
static QString convertRawText(const QString &text)
{
    QString result = text;
    for (auto dp = result.begin(); dp != result.end(); ++dp) {
        switch (dp->unicode()) {
        case 0xfdd0:    // Used internally by QTextDocument
        case 0xfdd1:    // Used internally by QTextDocument
        case QChar::ParagraphSeparator:
        case QChar::LineSeparator:
            *dp = QLatin1Char('\n');
            break;
        default:
            break;
        }
    }
    return result;
}
#endif

bool QTextPadWindow::saveDocumentTo(const QString &filename)
{
    auto codec = QTextPadCharsets::codecForName(m_textEncoding);
    if (!codec) {
        QMessageBox::critical(this, QString(),
            tr("The selected encoding (%1) is invalid.  Please select a valid "
               "encoding before attempting to save.").arg(m_textEncoding));
        return false;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, QString(),
                              tr("Cannot open file %1 for writing").arg(filename));
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    auto document = convertRawText(m_editor->document()->toRawText());
#else
    // NOTE: toPlainText() does not preserve NBSP characters
    auto document = m_editor->toPlainText();
#endif

    switch (m_lineEndingMode) {
    case FileTypeInfo::CROnly:
        document.replace(QLatin1Char('\n'), QLatin1Char('\r'));
        break;
    case FileTypeInfo::LFOnly:
        // No action
        break;
    case FileTypeInfo::CRLF:
        document.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
        break;
    }

    const auto buffer = codec->fromUnicode(document, utfBOM());
    qint64 count = file.write(buffer);
    if (count < 0) {
        QMessageBox::critical(this, QString(),
                              tr("Error writing to file: %1").arg(file.errorString()));
        return false;
    } else if (count != buffer.size()) {
        QMessageBox::critical(this, QString(), tr("Error: File truncated while writing"));
        return false;
    }

    const QTextCursor cursor = m_editor->textCursor();
    QTextPadSettings::setFileModes(filename, m_textEncoding, m_editor->syntaxName(),
                                   cursor.blockNumber() + 1);
    QTextPadSettings().addRecentFile(filename);
    populateRecentFiles();

    return true;
}

bool QTextPadWindow::loadDocumentFrom(const QString &filename, const QString &textEncoding)
{
    QFile file(filename);
    if (!file.exists()) {
        // Creating a new file
        resetEditor();

        KSyntaxHighlighting::Definition definition =
                SyntaxTextEdit::syntaxRepo()->definitionForFileName(filename);
        if (definition.isValid())
            setSyntax(definition);

        setOpenFilename(filename);
        m_fileState = FS_New;
        updateTitle();

        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QString(),
                              tr("Cannot open file %1 for reading").arg(filename));
        return false;
    }

    if (file.size() > LARGE_FILE_SIZE) {
        int response = QMessageBox::question(this, QString(),
                            tr("Warning: Are you sure you want to open this large file?"),
                            QMessageBox::Yes | QMessageBox::No);
        if (response == QMessageBox::No)
            return false;
    }

    const auto fileModes = QTextPadSettings::fileModes(filename);
    const QString codecName = textEncoding.isEmpty() ? fileModes.encoding : textEncoding;

    auto buffer = file.read(DETECTION_SIZE);
    auto detect = FileTypeInfo::detect(buffer);
    setLineEndingMode(detect.lineEndings());

    TextCodec *codec = Q_NULLPTR;
    if (!codecName.isEmpty()) {
        codec = QTextPadCharsets::codecForName(codecName);
        if (!codec) {
            qDebug("Invalid manually-specified encoding: %s",
                   codecName.toLocal8Bit().constData());
        }
    }
    if (!codec)
        codec = detect.textCodec();
    setEncoding(codec->name());

    buffer.append(file.readAll());
    QString document = codec->toUnicode(buffer);
    if (!document.isEmpty() && document[0] == QChar(0xFEFF))
        document = document.mid(1);

    // Don't search while we're in the middle of loading a new file
    showSearchBar(false);

    // Don't let the syntax highlighter hinder us while setting the new content
    m_editor->clear();
    setSyntax(SyntaxTextEdit::nullSyntax());
    m_editor->setPlainText(document);
    m_editor->document()->clearUndoRedoStacks();

    KSyntaxHighlighting::Definition definition;
    if (!fileModes.syntax.isEmpty())
        definition = SyntaxTextEdit::syntaxRepo()->definitionForName(fileModes.syntax);
    if (!definition.isValid())
        definition = SyntaxTextEdit::syntaxRepo()->definitionForFileName(filename);
    if (!definition.isValid())
        definition = FileTypeInfo::definitionForFileMagic(filename);

    if (definition.isValid())
        setSyntax(definition);

    if (fileModes.lineNum > 0)
        gotoLine(fileModes.lineNum);

    setOpenFilename(filename);
    QTextPadSettings::setFileModes(filename, m_textEncoding, definition.name(), fileModes.lineNum);
    QTextPadSettings().addRecentFile(filename);
    populateRecentFiles();

    m_fileState = 0;
    m_cachedModTime = QFileInfo(file).lastModified();

    m_undoStack->clear();
    m_undoStack->setClean();
    m_reloadAction->setEnabled(true);
    m_utfBOMAction->setChecked(detect.bomOffset() != 0);
    updateTitle();
    return true;
}

bool QTextPadWindow::isDocumentModified() const
{
    return !m_undoStack->isClean();
}

bool QTextPadWindow::documentExists() const
{
    // Checking m_fileState is faster than asking the file system...
    return !m_openFilename.isEmpty() && !(m_fileState & FS_New);
}

void QTextPadWindow::gotoLine(int line, int column)
{
    m_editor->moveCursorTo(line, column);
}

void QTextPadWindow::checkForModifications()
{
    if (m_openFilename.isEmpty() || (m_fileState & FS_OutOfDate) != 0)
        return;

    QFileInfo info(m_openFilename);
    if (!info.exists()) {
        if (!(m_fileState & FS_New)) {
            auto result = QMessageBox::warning(this, tr("File Deleted"),
                    tr("File %1 was deleted by another program.").arg(m_openFilename),
                    QMessageBox::Ignore | QMessageBox::Close);
            if (result == QMessageBox::Close) {
                close();
            } else if (result == QMessageBox::Ignore) {
                m_fileState = FS_New;
                m_cachedModTime = QDateTime();
                updateTitle();
            }
        }
    } else if ((m_fileState & FS_New) != 0 || info.lastModified() != m_cachedModTime) {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setWindowTitle(tr("File Modified"));
        msg.setText(tr("File %1 was modified by another program.").arg(m_openFilename));
        auto reloadButton = msg.addButton(tr("&Reload"), QMessageBox::AcceptRole);
        auto ignoreButton = msg.addButton(QMessageBox::Ignore);
        msg.setDefaultButton(reloadButton);

        msg.exec();
        if (msg.clickedButton() == reloadButton) {
            if (!loadDocumentFrom(m_openFilename))
                close();
        } else if (msg.clickedButton() == ignoreButton) {
            m_fileState = FS_OutOfDate;
            updateTitle();
        }
    }
}

bool QTextPadWindow::promptForSave()
{
    if (documentExists()) {
        const QTextCursor cursor = m_editor->textCursor();
        QTextPadSettings::setFileModes(m_openFilename, m_textEncoding,
                                       m_editor->syntaxName(), cursor.blockNumber() + 1);
    }

    if (isDocumentModified()) {
        int response = QMessageBox::question(this, QString(),
                            tr("%1 has been modified.  Would you like to save your changes first?")
                            .arg(documentTitle()), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (response == QMessageBox::Cancel)
            return false;
        else if (response == QMessageBox::Yes)
            return saveDocument();
    }
    return true;
}

bool QTextPadWindow::promptForDiscard()
{
    if (documentExists()) {
        const QTextCursor cursor = m_editor->textCursor();
        QTextPadSettings::setFileModes(m_openFilename, m_textEncoding,
                                       m_editor->syntaxName(), cursor.blockNumber() + 1);
    }

    if (isDocumentModified()) {
        int response = QMessageBox::question(this, QString(),
                            tr("%1 has been modified.  Are you sure you want to discard your changes?")
                            .arg(documentTitle()), QMessageBox::Yes | QMessageBox::No);
        if (response == QMessageBox::No)
            return false;
    }
    return true;
}

void QTextPadWindow::newDocument()
{
    if (!promptForSave())
        return;
    resetEditor();
    updateTitle();
}

void QTextPadWindow::resetEditor()
{
    m_editor->clear();
    m_editor->document()->clearUndoRedoStacks();

    setSyntax(SyntaxTextEdit::nullSyntax());
    setEncoding(QStringLiteral("UTF-8"));
#ifdef _WIN32
    setLineEndingMode(FileTypeInfo::CRLF);
#else
    // OSX uses LF as well, and we don't support building for classic MacOS
    setLineEndingMode(FileTypeInfo::LFOnly);
#endif

    setOpenFilename(QString());
    m_cachedModTime = QDateTime();
    m_undoStack->clear();
    m_undoStack->setClean();
    m_reloadAction->setEnabled(false);
    m_utfBOMAction->setChecked(false);
}

bool QTextPadWindow::saveDocument()
{
    if (m_openFilename.isEmpty())
        return saveDocumentAs();
    if (!saveDocumentTo(m_openFilename))
        return false;

    m_fileState = 0;
    m_cachedModTime = QFileInfo(m_openFilename).lastModified();
    m_undoStack->setClean();
    updateTitle();
    return true;
}

bool QTextPadWindow::saveDocumentAs()
{
    QString startPath;
    if (!m_openFilename.isEmpty()) {
        QFileInfo fi(m_openFilename);
        startPath = fi.absoluteFilePath();
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Save File As"), startPath);
    if (path.isEmpty())
        return false;
    if (!saveDocumentTo(path))
        return false;

    setOpenFilename(path);
    m_fileState = 0;
    m_cachedModTime = QFileInfo(path).lastModified();
    m_undoStack->setClean();
    updateTitle();
    return true;
}

bool QTextPadWindow::saveDocumentCopy()
{
    QString startPath;
    if (!m_openFilename.isEmpty()) {
        QFileInfo fi(m_openFilename);
        startPath = fi.absolutePath();
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Save Copy As"), startPath);
    if (path.isEmpty())
        return false;
    return saveDocumentTo(path);
}

bool QTextPadWindow::loadDocument()
{
    if (!promptForSave())
        return false;

    QString startPath;
    if (!m_openFilename.isEmpty()) {
        QFileInfo fi(m_openFilename);
        startPath = fi.absolutePath();
    }
    QString path = QFileDialog::getOpenFileName(this, tr("Open File"), startPath);
    if (path.isEmpty())
        return false;

    return loadDocumentFrom(path);
}

bool QTextPadWindow::reloadDocument()
{
    if (!documentExists())
        return true;
    if (!promptForDiscard())
        return false;
    return loadDocumentFrom(m_openFilename);
}

void QTextPadWindow::reloadDocumentEncoding(const QString &textEncoding)
{
    Q_ASSERT(documentExists());

    const QString oldEncoding = m_textEncoding;
    if (!promptForDiscard() || !loadDocumentFrom(m_openFilename, textEncoding))
        setEncoding(oldEncoding);
}

void QTextPadWindow::printDocument()
{
    QPrinter printer;
    printer.setDocName(documentTitle());

    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted)
        m_editor->printDocument(&printer);
}

void QTextPadWindow::printPreviewDocument()
{
    QPrinter printer;
    printer.setDocName(documentTitle());

    QPrintPreviewDialog previewDialog(&printer, this);
    connect(&previewDialog, &QPrintPreviewDialog::paintRequested,
            m_editor, &SyntaxTextEdit::printDocument);
    previewDialog.exec();
}

void QTextPadWindow::editorContextMenu(const QPoint &pos)
{
    std::unique_ptr<QMenu> menu(new QMenu(m_editor));
    for (auto action : m_editorContextActions)
        menu->addAction(action);

#if 0
    // TODO: Qt provides a control code menu in RTL text mode, but
    // does not provide any public API way to use it without instantiating
    // QPlainTextEdit's default context menu :(
    if (QGuiApplication::styleHints()->useRtlExtensions()) {
        menu->addSeparator();
        // Private API
        auto ctrlCharMenu = new QUnicodeControlCharacterMenu(m_editor, menu);
        menu->addMenu(ctrlCharMenu);
    }
#endif

    menu->exec(m_editor->viewport()->mapToGlobal(pos));
}

void QTextPadWindow::updateCursorPosition()
{
    const QTextCursor cursor = m_editor->textCursor();
    const int column = m_editor->textColumn(cursor.block().text(), cursor.positionInBlock());
    const int selectedChars = std::abs(cursor.selectionEnd() - cursor.selectionStart());
    QString positionText = tr("Line %1, Col %2")
                                .arg(cursor.blockNumber() + 1)
                                .arg(column + 1);
    if (selectedChars)
        positionText += tr(" (Selected: %1)").arg(selectedChars);
    m_positionLabel->setText(positionText);
}

QString QTextPadWindow::documentTitle()
{
    if (m_openFilename.isEmpty())
        return tr("Untitled");

    QFileInfo fi(m_openFilename);
    return fi.fileName();
}

void QTextPadWindow::updateTitle()
{
    QString title = documentTitle();
    if (m_showFilePath && !m_openFilename.isEmpty()) {
        QFileInfo fi(m_openFilename);
        title += QStringLiteral(" [%1]").arg(fi.absolutePath());
    }
    if ((m_fileState & FS_OutOfDate) != 0)
        title += tr(" (Not Current)");
    else if ((m_fileState & FS_New) != 0)
        title += tr(" (New File)");
    title += QStringLiteral(u" \u2013 qtextpad");  // n-dash
    if (isDocumentModified())
        title = QStringLiteral("* ") + title;

    setWindowTitle(title);
}

void QTextPadWindow::nextInsertMode()
{
    setOverwriteMode(!m_editor->overwriteMode());
}

void QTextPadWindow::nextLineEndingMode()
{
    switch (m_lineEndingMode) {
    case FileTypeInfo::CROnly:
        changeLineEndingMode(FileTypeInfo::LFOnly);
        break;
    case FileTypeInfo::LFOnly:
        changeLineEndingMode(FileTypeInfo::CRLF);
        break;
    case FileTypeInfo::CRLF:
        changeLineEndingMode(FileTypeInfo::CROnly);
        break;
    }
}

void QTextPadWindow::updateIndentStatus()
{
    const int tabWidth = m_editor->tabWidth();
    const int indentWidth = m_editor->indentWidth();
    const auto indentMode = m_editor->indentationMode();

    QString description;
    switch (indentMode) {
    case SyntaxTextEdit::IndentSpaces:
        description = tr("Soft Tabs: %1").arg(indentWidth);
        if (tabWidth != indentWidth)
            description += tr(" (%1)").arg(tabWidth);
        break;
    case SyntaxTextEdit::IndentTabs:
        description = tr("Tab Size: %1").arg(tabWidth);
        break;
    case SyntaxTextEdit::IndentMixed:
        description = tr("Mixed Indent: %1").arg(indentWidth);
        if (tabWidth != indentWidth)
            description += tr(" (%1)").arg(tabWidth);
        break;
    default:
        description = tr("INVALID");
        break;
    }

    m_indentButton->setText(description);

    QAction *otherAction = Q_NULLPTR;
    bool haveMatch = false;
    for (auto action : m_tabWidthActions->actions()) {
        if (!action->data().isValid()) {
            otherAction = action;
        } else if (tabWidth == action->data().toInt()) {
            action->setChecked(true);
            haveMatch = true;
        }
    }
    if (!haveMatch && otherAction)
        otherAction->setChecked(true);

    otherAction = Q_NULLPTR;
    haveMatch = false;
    for (auto action : m_indentWidthActions->actions()) {
        if (!action->data().isValid()) {
            otherAction = action;
        } else if (indentWidth == action->data().toInt()) {
            action->setChecked(true);
            haveMatch = true;
        }
    }
    if (!haveMatch && otherAction)
        otherAction->setChecked(true);

    for (auto action : m_indentModeActions->actions()) {
        const auto actionMode = static_cast<SyntaxTextEdit::IndentationMode>(action->data().toInt());
        if (indentMode == actionMode)
            action->setChecked(true);
    }
}

void QTextPadWindow::chooseEditorFont()
{
    bool ok = false;
    QFont newFont = QFontDialog::getFont(&ok, m_editor->defaultFont(), this,
                                         tr("Set Editor Font"));
    if (ok) {
        m_editor->setDefaultFont(newFont);
        QTextPadSettings().setEditorFont(newFont);
    }
}

void QTextPadWindow::changeEncoding(const QString &encoding)
{
    if (encoding == m_textEncoding)
        return;

    if (!documentExists()) {
        // Don't save changes in the undo stack if we are creating a new file
        setEncoding(encoding);
    } else {
        QMessageBox mbQuestion(QMessageBox::Question, tr("Change Document Encoding"),
               tr("The current document encoding is '%1'.  Would you like to:<ul>"
                  "<li><b>Reload</b> the existing file in the '%2' encoding, or</li>"
                  "<li><b>Convert</b> the current document's encoding to '%2'?</li></ul>")
                  .arg(m_textEncoding).arg(encoding),
               QMessageBox::Cancel, this);
        auto reloadButton = mbQuestion.addButton(tr("&Reload"), QMessageBox::AcceptRole);
        auto convertButton = mbQuestion.addButton(tr("&Convert"), QMessageBox::AcceptRole);

        (void)mbQuestion.exec();
        if (mbQuestion.clickedButton() == convertButton) {
            auto command = new ChangeEncodingCommand(this, encoding);
            m_undoStack->push(command);
        } else if (mbQuestion.clickedButton() == reloadButton) {
            reloadDocumentEncoding(encoding);
        }
        // No action if the user hit Cancel or closed the dialog.
    }
}

void QTextPadWindow::changeLineEndingMode(FileTypeInfo::LineEndingType mode)
{
    if (!documentExists()) {
        // Don't save changes in the undo stack if we are creating a new file
        setLineEndingMode(mode);
    } else {
        auto command = new ChangeLineEndingCommand(this, mode);
        m_undoStack->push(command);
    }
}

void QTextPadWindow::changeUtfBOM()
{
    if (documentExists()) {
        // Don't save changes in the undo stack if we are creating a new file
        auto command = new ChangeUtfBOMCommand(this);
        m_undoStack->push(command);
    }
}

void QTextPadWindow::promptIndentSettings()
{
    auto dialog = new IndentSettingsDialog(this);
    dialog->loadSettings(m_editor);
    if (dialog->exec() == QDialog::Accepted) {
        dialog->applySettings(m_editor);
        updateIndentStatus();
    }
}

void QTextPadWindow::promptLongLineWidth()
{
    bool ok;
    auto width = QInputDialog::getInt(this, tr("Long Line Width"),
            tr("Set Long Line Margin Position (characters)"),
            m_editor->longLineWidth(), 0, std::numeric_limits<int>::max(), 1, &ok);
    if (ok) {
        m_editor->setLongLineWidth(width);
        QTextPadSettings().setLongLineWidth(width);
    }
}

void QTextPadWindow::navigateToLine()
{
    const QTextCursor cursor = m_editor->textCursor();
    const QString curLine = QString::number(cursor.block().blockNumber() + 1);
    QInputDialog dialog(this);
    dialog.setWindowTitle(tr("Go to Line"));
    dialog.setWindowIcon(ICON("go-jump"));
    dialog.setLabelText(tr("Enter line number or line:column"));
    dialog.setTextValue(curLine);
    if (dialog.exec() == QDialog::Rejected)
        return;

    QStringList parts = dialog.textValue().split(QLatin1Char(':'));
    bool ok;
    int line = parts.at(0).toInt(&ok);
    int column = 0;
    if (ok && parts.size() > 1)
        column = parts.at(1).toInt(&ok);

    if (!ok || line < 0 || column < 0) {
        QMessageBox::critical(this, QString(), tr("Invalid line number specified"));
        return;
    }

    gotoLine(line, column);
}

void QTextPadWindow::toggleFilePath(bool show)
{
    m_showFilePath = show;
    QTextPadSettings().setShowFilePath(show);
    updateTitle();
}

void QTextPadWindow::insertDateTime(QLocale::FormatType type)
{
    auto cursor = m_editor->textCursor();
    const auto now = QDateTime::currentDateTime();
    cursor.insertText(now.toString(QLocale().dateTimeFormat(type)));
}

template <typename Modify>
void modifySelection(SyntaxTextEdit *editor, Modify &&modify)
{
    auto cursor = editor->textCursor();
    cursor.beginEditBlock();

    int selectStart = -1, selectEnd = -1;
    if (cursor.hasSelection()) {
        selectStart = cursor.position();
        selectEnd = cursor.anchor();
    } else {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    }

    QString newText = modify(cursor.selectedText());
    cursor.removeSelectedText();
    cursor.insertText(newText);
    cursor.endEditBlock();

    if (selectEnd >= 0) {
        cursor.setPosition(selectEnd);
        cursor.setPosition(selectStart, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
    }
}

void QTextPadWindow::upcaseSelection()
{
    modifySelection(m_editor, [](const QString &selectedText) {
        return QLocale().toUpper(selectedText);
    });
}

void QTextPadWindow::downcaseSelection()
{
    modifySelection(m_editor, [](const QString &selectedText) {
        return QLocale().toLower(selectedText);
    });
}

// Why doesn't Qt provide these?
static inline QString trimLeft(const QString &text)
{
    const QChar *begin = text.constData();
    const QChar *end = begin + text.size();
    while (begin < end && begin->isSpace())
        ++begin;
    return QString(begin, static_cast<int>(end - begin));
}

static inline QString trimRight(const QString &text)
{
    const QChar *begin = text.constData();
    const QChar *end = begin + text.size();
    while (begin < end && end[-1].isSpace())
        --end;
    return QString(begin, static_cast<int>(end - begin));
}

static void joinText(QString &out, const QString &line)
{
    if (line.isEmpty())
        return;

    if (!out.isEmpty())
        out.append(QLatin1Char(' '));
    out.append(line);
}

void QTextPadWindow::joinLines()
{
    auto cursor = m_editor->textCursor();

    const int startPos = cursor.position();
    const int endPos = cursor.anchor();
    cursor.setPosition(qMin(startPos, endPos));
    cursor.movePosition(QTextCursor::StartOfBlock);
    auto startBlock = cursor.block();
    cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    auto endBlock = cursor.block();

    // Join requires at least two blocks
    if (startBlock == endBlock) {
        if (!cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor))
            return;
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        endBlock = cursor.block();
    }

    QString joined;
    const int selectionSize = cursor.selectionEnd() - cursor.selectionStart();
    joined.reserve(selectionSize);
    joined.append(trimRight(startBlock.text()));
    for (auto block = startBlock.next(); block < endBlock; block = block.next())
        joinText(joined, block.text().trimmed());
    joinText(joined, trimLeft(endBlock.text()));

    cursor.beginEditBlock();
    cursor.removeSelectedText();
    cursor.insertText(joined);
    cursor.endEditBlock();

    // TODO: Not perfect, but easier than adjusting the cursor based on
    // reformatted line content...
    if (startPos == endPos) {
        cursor.setPosition(startPos);
    } else if (startPos > endPos) {
        cursor.setPosition(endPos);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    } else {
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.setPosition(startPos, QTextCursor::KeepAnchor);
    }
    m_editor->setTextCursor(cursor);
}

void QTextPadWindow::resizeEvent(QResizeEvent *event)
{
    if (event)
        QMainWindow::resizeEvent(event);

    // Move the search widget to the upper-right corner
    const QPoint editorPos = m_editor->pos();
    QSize searchSize = m_searchWidget->sizeHint();
    m_searchWidget->resize(searchSize);
    m_searchWidget->move(editorPos.x() + m_editor->viewport()->width() - searchSize.width() - 16,
                         editorPos.y());
}

void QTextPadWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    resizeEvent(nullptr);
}

void QTextPadWindow::closeEvent(QCloseEvent *event)
{
    if (!promptForSave()) {
        event->ignore();
        return;
    }

    if ((windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen)) == 0) {
        QTextPadSettings settings;
        settings.setWindowSize(size());
        settings.setShowToolBar(m_toolBar->isVisible());
        settings.setShowStatusBar(statusBar()->isVisible());
    }
    event->accept();
}

bool QTextPadWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_searchWidget->isVisible() && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            showSearchBar(false);
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void QTextPadWindow::populateRecentFiles()
{
    m_recentFiles->clear();

    auto recentFiles = QTextPadSettings().recentFiles();
    for (const auto &recent : recentFiles) {
        QFileInfo info(recent);
        const QString label = QStringLiteral("%1 [%2]").arg(info.fileName(), info.absolutePath());
        auto recentFileAction = m_recentFiles->addAction(label);
        connect(recentFileAction, &QAction::triggered, this, [this, recent] {
            if (!promptForSave())
                return;
            loadDocumentFrom(recent);
        });
    }

    (void) m_recentFiles->addSeparator();
    auto clearListAction = m_recentFiles->addAction(tr("Clear List"));
    connect(clearListAction, &QAction::triggered, this, [this] {
        QTextPadSettings settings;
        settings.clearRecentFiles();
        populateRecentFiles();
    });
}

void QTextPadWindow::populateSyntaxMenu()
{
    m_syntaxActions = new QActionGroup(this);

    auto plainText = m_syntaxMenu->addAction(tr("Plain Text"));
    plainText->setCheckable(true);
    plainText->setActionGroup(m_syntaxActions);
    plainText->setData(QVariant::fromValue(SyntaxTextEdit::nullSyntax()));
    connect(plainText, &QAction::triggered, this, [this] {
        setSyntax(SyntaxTextEdit::nullSyntax());
    });

    KSyntaxHighlighting::Repository *syntaxRepo = SyntaxTextEdit::syntaxRepo();
    const auto syntaxDefs = syntaxRepo->definitions();
    QMap<QString, QMenu *> groupMenus;
    for (const auto &def : syntaxDefs) {
        if (def.isHidden() || def == SyntaxTextEdit::nullSyntax())
            continue;

        QMenu *parentMenu = groupMenus.value(def.translatedSection(), Q_NULLPTR);
        if (!parentMenu) {
            parentMenu = m_syntaxMenu->addMenu(def.translatedSection());
            groupMenus[def.translatedSection()] = parentMenu;
        }
        auto item = parentMenu->addAction(def.translatedName());
        item->setCheckable(true);
        item->setActionGroup(m_syntaxActions);
        item->setData(QVariant::fromValue(def));
        connect(item, &QAction::triggered, this, [this, def] { setSyntax(def); });
    }

#if (SyntaxHighlighting_VERSION >= ((5<<16)|(56<<8)|(0)))
    (void) m_syntaxMenu->addSeparator();
    auto updateAction = m_syntaxMenu->addAction(tr("Update Definitions"));
    connect(updateAction, &QAction::triggered, this, [this] {
        auto downloadDialog = new DefinitionDownloadDialog(SyntaxTextEdit::syntaxRepo(), this);
        downloadDialog->setAttribute(Qt::WA_DeleteOnClose);
        downloadDialog->show();
        downloadDialog->raise();
        downloadDialog->activateWindow();
    });
#endif
}

void QTextPadWindow::populateThemeMenu()
{
    m_themeActions = new QActionGroup(this);

    KSyntaxHighlighting::Repository *syntaxRepo = SyntaxTextEdit::syntaxRepo();
    const auto themeDefs = syntaxRepo->themes();
    for (const auto &theme : themeDefs) {
        auto item = m_themeMenu->addAction(theme.translatedName());
        item->setCheckable(true);
        item->setActionGroup(m_themeActions);
        item->setData(QVariant::fromValue(theme));
        connect(item, &QAction::triggered, this, [this, theme] { setTheme(theme); });
    }
}

void QTextPadWindow::populateEncodingMenu()
{
    m_setEncodingActions = new QActionGroup(this);
    auto encodingScripts = QTextPadCharsets::encodingsByScript();

    m_utfBOMAction = m_setEncodingMenu->addAction(tr("Write Unicode BOM"));
    m_utfBOMAction->setCheckable(true);
    (void) m_setEncodingMenu->addSeparator();
    connect(m_utfBOMAction, &QAction::triggered, this, &QTextPadWindow::changeUtfBOM);

    // Sort the lists by script/region name
    std::sort(encodingScripts.begin(), encodingScripts.end(),
              [](const QStringList &left, const QStringList &right)
    {
        return left.first() < right.first();
    });

    for (const auto &encodingList : encodingScripts) {
        QMenu *setEncodingParentMenu = m_setEncodingMenu->addMenu(encodingList.first());
        for (int i = 1; i < encodingList.size(); ++i) {
            const QString &codecName = encodingList.at(i);
            auto seItem = setEncodingParentMenu->addAction(codecName);
            seItem->setCheckable(true);
            seItem->setActionGroup(m_setEncodingActions);
            seItem->setData(codecName);
            connect(seItem, &QAction::triggered, this, [this, codecName] {
                changeEncoding(codecName);
            });
        }
    }
}

void QTextPadWindow::populateIndentButtonMenu()
{
    auto indentMenu = new QMenu(this);

    // Qt 5.1 has QMenu::addSection, but that results in a style hint that
    // is completely ignored by some platforms, including both GTK and Windows
    auto headerLabel = new QLabel(tr("Tab Width"), this);
    headerLabel->setAlignment(Qt::AlignCenter);
    headerLabel->setContentsMargins(4, 4, 4, 0);
    auto headerAction = new QWidgetAction(this);
    headerAction->setDefaultWidget(headerLabel);
    indentMenu->addAction(headerAction);
    (void) indentMenu->addSeparator();

    m_tabWidthActions = new QActionGroup(this);
    auto tabWidth8Action = indentMenu->addAction(QStringLiteral("8"));
    tabWidth8Action->setCheckable(true);
    tabWidth8Action->setActionGroup(m_tabWidthActions);
    tabWidth8Action->setData(8);
    auto tabWidth4Action = indentMenu->addAction(QStringLiteral("4"));
    tabWidth4Action->setCheckable(true);
    tabWidth4Action->setActionGroup(m_tabWidthActions);
    tabWidth4Action->setData(4);
    auto tabWidth2Action = indentMenu->addAction(QStringLiteral("2"));
    tabWidth2Action->setCheckable(true);
    tabWidth2Action->setActionGroup(m_tabWidthActions);
    tabWidth2Action->setData(2);
    auto tabWidthOtherAction = indentMenu->addAction(tr("Other..."));
    tabWidthOtherAction->setCheckable(true);
    tabWidthOtherAction->setActionGroup(m_tabWidthActions);

    for (auto action : m_tabWidthActions->actions()) {
        if (action == tabWidthOtherAction)
            continue;
        connect(action, &QAction::triggered, this, [this, action] {
            const int width = action->data().toInt();
            m_editor->setTabWidth(width);
            QTextPadSettings().setTabWidth(width);
            updateIndentStatus();
        });
    }
    connect(tabWidthOtherAction, &QAction::triggered,
            this, &QTextPadWindow::promptIndentSettings);

    headerLabel = new QLabel(tr("Indentation Width"), this);
    headerLabel->setAlignment(Qt::AlignCenter);
    headerLabel->setContentsMargins(4, 4, 4, 0);
    headerAction = new QWidgetAction(this);
    headerAction->setDefaultWidget(headerLabel);
    indentMenu->addAction(headerAction);
    (void) indentMenu->addSeparator();

    m_indentWidthActions = new QActionGroup(this);
    auto indentWidth8Action = indentMenu->addAction(QStringLiteral("8"));
    indentWidth8Action->setCheckable(true);
    indentWidth8Action->setActionGroup(m_indentWidthActions);
    indentWidth8Action->setData(8);
    auto indentWidth4Action = indentMenu->addAction(QStringLiteral("4"));
    indentWidth4Action->setCheckable(true);
    indentWidth4Action->setActionGroup(m_indentWidthActions);
    indentWidth4Action->setData(4);
    auto indentWidth2Action = indentMenu->addAction(QStringLiteral("2"));
    indentWidth2Action->setCheckable(true);
    indentWidth2Action->setActionGroup(m_indentWidthActions);
    indentWidth2Action->setData(2);
    auto indentWidthOtherAction = indentMenu->addAction(tr("Other..."));
    indentWidthOtherAction->setCheckable(true);
    indentWidthOtherAction->setActionGroup(m_indentWidthActions);

    for (auto action : m_indentWidthActions->actions()) {
        if (action == indentWidthOtherAction)
            continue;
        connect(action, &QAction::triggered, this, [this, action] {
            const int width = action->data().toInt();
            m_editor->setIndentWidth(width);
            QTextPadSettings().setIndentWidth(width);
            updateIndentStatus();
        });
    }
    connect(indentWidthOtherAction, &QAction::triggered,
            this, &QTextPadWindow::promptIndentSettings);

    headerLabel = new QLabel(tr("Indentation Mode"), this);
    headerLabel->setAlignment(Qt::AlignCenter);
    headerLabel->setContentsMargins(4, 4, 4, 0);
    headerAction = new QWidgetAction(this);
    headerAction->setDefaultWidget(headerLabel);
    indentMenu->addAction(headerAction);
    (void) indentMenu->addSeparator();

    m_indentModeActions = new QActionGroup(this);
    auto indentSpacesAction = indentMenu->addAction(tr("&Spaces"));
    indentSpacesAction->setCheckable(true);
    indentSpacesAction->setActionGroup(m_indentModeActions);
    indentSpacesAction->setData(static_cast<int>(SyntaxTextEdit::IndentSpaces));
    auto indentTabsAction = indentMenu->addAction(tr("&Tabs"));
    indentTabsAction->setCheckable(true);
    indentTabsAction->setActionGroup(m_indentModeActions);
    indentTabsAction->setData(static_cast<int>(SyntaxTextEdit::IndentTabs));
    auto indentMixedAction = indentMenu->addAction(tr("&Mixed"));
    indentMixedAction->setCheckable(true);
    indentMixedAction->setActionGroup(m_indentModeActions);
    indentMixedAction->setData(static_cast<int>(SyntaxTextEdit::IndentMixed));

    for (auto action : m_indentModeActions->actions()) {
        connect(action, &QAction::triggered, this, [this, action] {
            const int mode = action->data().toInt();
            m_editor->setIndentationMode(mode);
            QTextPadSettings().setIndentMode(mode);
            updateIndentStatus();
        });
    }

    (void) indentMenu->addSeparator();
    // Make a copy since we don't want to show the key shortcut here
    auto autoIndentAction = indentMenu->addAction(m_autoIndentAction->text());
    autoIndentAction->setCheckable(true);
    autoIndentAction->setChecked(m_autoIndentAction->isChecked());
    connect(m_autoIndentAction, &QAction::toggled,
            autoIndentAction, &QAction::setChecked);
    connect(autoIndentAction, &QAction::triggered,
            m_autoIndentAction, &QAction::trigger);

    m_indentButton->setMenu(indentMenu);
    updateIndentStatus();
}
