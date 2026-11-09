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
#include <QTextBlock>
#include <QTimer>
#include <QFileInfo>
#include <QPrinter>
#include <QDateTime>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QTabWidget>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QGuiApplication>
#include <QStyleHints>
#endif

#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>

#include "documenttabs.h"
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

#ifdef Q_OS_WIN
#define FILE_COMPARE_CS Qt::CaseInsensitive
#else
#define FILE_COMPARE_CS Qt::CaseSensitive
#endif

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
            QTimer::singleShot(100, nullptr, [] {
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
            QTimer::singleShot(100, nullptr, [] {
                auto popupWidget = QApplication::activePopupWidget();
                if (popupWidget)
                    popupWidget->close();
            });
        });
        return popup;
    }
};

QTextPadWindow::QTextPadWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QTextPadSettings settings;

    m_tabs = new DocumentTabWidget(this);
    setCentralWidget(m_tabs);
    addDocumentTab();

    m_searchWidget = new SearchWidget(this);
    showSearchBar(false);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    auto newTabAction = fileMenu->addAction(ICON("document-new"), tr("New &Tab"));
    newTabAction->setShortcuts({ QKeySequence::AddTab, QKeySequence::New });
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
    auto closeTabAction = fileMenu->addAction(ICON("tab-close"), tr("Clos&e Tab"));
    closeTabAction->setShortcut(Qt::CTRL | Qt::Key_W);
    auto quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);

    connect(newTabAction, &QAction::triggered, this, &QTextPadWindow::newDocument);
    connect(closeTabAction, &QAction::triggered, this,
            [this] { closeTab(m_tabs->currentIndex()); });
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
    m_undoAction = editMenu->addAction(ICON("edit-undo"), tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_editorContextActions << m_undoAction;
    m_redoAction = editMenu->addAction(ICON("edit-redo"), tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_editorContextActions << m_redoAction;
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
    m_deleteAction = editMenu->addAction(ICON("edit-delete"), tr("&Delete"));
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_editorContextActions << m_deleteAction;
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

    connect(m_undoAction, &QAction::triggered, this, [this] { undoStack()->undo(); });
    connect(m_redoAction, &QAction::triggered, this, [this] { undoStack()->redo(); });
    connect(cutAction, &QAction::triggered, this, [this] { editor()->cutLines(); });
    connect(copyAction, &QAction::triggered, this, [this] { editor()->copyLines(); });
    connect(pasteAction, &QAction::triggered, this, [this] { editor()->paste(); });
    connect(m_deleteAction, &QAction::triggered, this, [this] { editor()->deleteSelection(); });
    connect(deleteLinesAction, &QAction::triggered, this, [this] { editor()->deleteLines(); });
    connect(selectAllAction, &QAction::triggered, this, [this] { editor()->selectAll(); });
    connect(m_overwriteModeAction, &QAction::toggled,
            this, &QTextPadWindow::setOverwriteMode);

    connect(findAction, &QAction::triggered, this, [this] { showSearchBar(true); });
    connect(findNextAction, &QAction::triggered, this, [this] { m_searchWidget->searchNext(false); });
    connect(findPrevAction, &QAction::triggered, this, [this] { m_searchWidget->searchNext(true); });
    connect(replaceAction, &QAction::triggered, this, [this] { SearchDialog::create(this); });
    connect(gotoAction, &QAction::triggered, this, &QTextPadWindow::navigateToLine);

    // per-tab undo stack and editor signals are hooked up in addDocumentTab()
    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);
    m_deleteAction->setEnabled(false);

    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this, pasteAction] {
        pasteAction->setEnabled(editor()->canPaste());
    });
    pasteAction->setEnabled(editor()->canPaste());

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_themeMenu = viewMenu->addMenu(tr("&Theme"));
    populateThemeMenu();
    (void) viewMenu->addSeparator();
    auto wordWrapAction = viewMenu->addAction(tr("&Word Wrap"));
    wordWrapAction->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_W);
    wordWrapAction->setCheckable(true);
    auto longLineAction = viewMenu->addAction(tr("Long Line &Margin"));
    longLineAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_M);
    longLineAction->setCheckable(true);
    auto longLineWidthAction = viewMenu->addAction(tr("Set Long Line Wi&dth..."));
    auto indentGuidesAction = viewMenu->addAction(tr("Indentation &Guides"));
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
                for (auto ed : editors())
                    ed->setWordWrap(wrap);
                QTextPadSettings().setWordWrap(wrap);
            });
    connect(longLineAction, &QAction::toggled, this,
            [this](bool show) {
                for (auto ed : editors())
                    ed->setShowLongLineEdge(show);
                QTextPadSettings().setShowLongLineMargin(show);
            });
    connect(longLineWidthAction, &QAction::triggered,
            this, &QTextPadWindow::promptLongLineWidth);
    connect(indentGuidesAction, &QAction::toggled, this,
            [this](bool show) {
                for (auto ed : editors())
                    ed->setShowIndentGuides(show);
                QTextPadSettings().setIndentationGuides(show);
            });
    connect(showLineNumbersAction, &QAction::toggled, this,
            [this](bool show) {
                for (auto ed : editors())
                    ed->setShowLineNumbers(show);
                QTextPadSettings().setLineNumbers(show);
            });
    connect(showFoldingAction, &QAction::toggled, this,
            [this](bool show) {
                for (auto ed : editors())
                    ed->setShowFolding(show);
                QTextPadSettings().setShowFolding(show);
            });
    connect(showWhitespaceAction, &QAction::toggled, this,
            [this](bool show) {
                for (auto ed : editors())
                    ed->setShowWhitespace(show);
                QTextPadSettings().setShowWhitespace(show);
            });
    connect(scrollPastEndOfFileAction, &QAction::toggled, this,
            [this](bool scroll) {
                for (auto ed : editors())
                    ed->setScrollPastEndOfFile(scroll);
                QTextPadSettings().setScrollPastEndOfFile(scroll);
            });
    connect(showCurrentLineAction, &QAction::toggled, this,
            [this](bool show) {
                for (auto ed : editors())
                    ed->setHighlightCurrentLine(show);
                QTextPadSettings().setHighlightCurLine(show);
            });
    connect(showMatchingBraces, &QAction::toggled, this,
            [this](bool show) {
                for (auto ed : editors())
                    ed->setMatchBraces(show);
                QTextPadSettings().setMatchBraces(show);
            });
    connect(zoomInAction, &QAction::triggered, this, [this] { editor()->zoomIn(); });
    connect(zoomOutAction, &QAction::triggered, this, [this] { editor()->zoomOut(); });
    connect(zoomResetAction, &QAction::triggered, this, [this] { editor()->zoomReset(); });
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
        editor()->moveLines(QTextCursor::PreviousBlock);
    });
    connect(linesDownAction, &QAction::triggered, this, [this](bool) {
        editor()->moveLines(QTextCursor::NextBlock);
    });
    connect(joinLinesAction, &QAction::triggered, this, &QTextPadWindow::joinLines);
    connect(foldAction, &QAction::triggered, this, [this] { editor()->foldCurrentLine(); });
    connect(unfoldAction, &QAction::triggered, this, [this] { editor()->unfoldCurrentLine(); });
    connect(foldAllAction, &QAction::triggered, this, [this] { editor()->foldAll(); });
    connect(unfoldAllAction, &QAction::triggered, this, [this] { editor()->unfoldAll(); });

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
    (void) settingsMenu->addSeparator();
    auto hideSingleTabAction = settingsMenu->addAction(tr("&Hide Tab Bar with a Single Tab"));
    hideSingleTabAction->setCheckable(true);
    hideSingleTabAction->setChecked(settings.hideSingleTab());
    auto closeWindowAction = settingsMenu->addAction(tr("Close &Window with Last Tab"));
    closeWindowAction->setCheckable(true);
    closeWindowAction->setChecked(settings.closeWindowOnLastTab());

    connect(fontAction, &QAction::triggered, this, &QTextPadWindow::chooseEditorFont);
    connect(crOnlyAction, &QAction::triggered, this, [this] { changeLineEndingMode(FileTypeInfo::CROnly); });
    connect(lfOnlyAction, &QAction::triggered, this, [this] { changeLineEndingMode(FileTypeInfo::LFOnly); });
    connect(crlfAction, &QAction::triggered, this, [this] { changeLineEndingMode(FileTypeInfo::CRLF); });
    connect(indentSettingsAction, &QAction::triggered, this, &QTextPadWindow::promptIndentSettings);
    connect(m_autoIndentAction, &QAction::toggled, this, &QTextPadWindow::setAutoIndent);
    connect(showFilePathAction, &QAction::toggled, this, &QTextPadWindow::toggleFilePath);
    connect(hideSingleTabAction, &QAction::toggled, this, [this](bool hide) {
        m_tabs->setTabBarAutoHide(hide);
        QTextPadSettings().setHideSingleTab(hide);
    });
    connect(closeWindowAction, &QAction::toggled, this, [](bool closeWindow) {
        QTextPadSettings().setCloseWindowOnLastTab(closeWindow);
    });

    m_documentsMenu = menuBar()->addMenu(tr("&Documents"));
    auto saveAllAction = m_documentsMenu->addAction(ICON("document-save-all"), tr("&Save All"));
    saveAllAction->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_S);
    auto closeAllAction = m_documentsMenu->addAction(ICON("tab-close"), tr("Close All &Tabs"));
    auto closeOtherAction = m_documentsMenu->addAction(ICON("tab-close-other"), tr("Close &Other Tabs"));
    (void) m_documentsMenu->addSeparator();
    auto prevTabAction = m_documentsMenu->addAction(tr("Pre&vious Tab"));
    prevTabAction->setShortcut(Qt::CTRL | Qt::Key_PageUp);
    auto nextTabAction = m_documentsMenu->addAction(tr("Ne&xt Tab"));
    nextTabAction->setShortcut(Qt::CTRL | Qt::Key_PageDown);
    (void) m_documentsMenu->addSeparator();

    connect(saveAllAction, &QAction::triggered, this, &QTextPadWindow::promptSaveAll);
    connect(closeAllAction, &QAction::triggered, this, &QTextPadWindow::closeAllTabs);
    connect(closeOtherAction, &QAction::triggered, this, &QTextPadWindow::closeOtherTabs);
    connect(prevTabAction, &QAction::triggered, this, [this] {
        m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count());
    });
    connect(nextTabAction, &QAction::triggered, this, [this] {
        m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count());
    });

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto aboutAction = helpMenu->addAction(ICON("help-about"), tr("&About..."));
    aboutAction->setShortcut(QKeySequence::HelpContents);

    connect(aboutAction, &QAction::triggered, this, &QTextPadWindow::showAbout);

    m_toolBar = addToolBar(tr("Toolbar"));
    m_toolBar->setIconSize(QSize(22, 22));
    m_toolBar->setMovable(false);
    m_toolBar->addAction(newTabAction);
    m_toolBar->addAction(openAction);
    m_toolBar->addAction(saveAction);
    (void) m_toolBar->addSeparator();
    m_toolBar->addAction(m_undoAction);
    m_toolBar->addAction(m_redoAction);
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
    m_indentButton->setAutoRaise(true);
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

    wordWrapAction->setChecked(editor()->wordWrap());
    longLineAction->setChecked(editor()->showLongLineEdge());
    indentGuidesAction->setChecked(editor()->showIndentGuides());
    showLineNumbersAction->setChecked(editor()->showLineNumbers());
    showFoldingAction->setChecked(editor()->showFolding());
    showWhitespaceAction->setChecked(editor()->showWhitespace());
    scrollPastEndOfFileAction->setChecked(editor()->scrollPastEndOfFile());
    showCurrentLineAction->setChecked(editor()->highlightCurrentLine());
    showMatchingBraces->setChecked(editor()->matchBraces());
    m_autoIndentAction->setChecked(editor()->autoIndent());

    KSyntaxHighlighting::Theme theme;
    QString themeName = settings.editorTheme();
    if (!themeName.isEmpty())
        theme = SyntaxTextEdit::syntaxRepo()->theme(themeName);
    if (theme.isValid())
        setEditorTheme(theme);
    else
        setDefaultEditorTheme();

    m_insertLabel->setMinimumWidth(QFontMetrics(m_insertLabel->font()).boundingRect(tr("OVR")).width() + 4);
    m_crlfLabel->setMinimumWidth(QFontMetrics(m_crlfLabel->font()).boundingRect(tr("CRLF")).width() + 4);
    setOverwriteMode(false);

    connect(m_tabs, &QTabWidget::currentChanged, this, &QTextPadWindow::currentTabChanged);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &QTextPadWindow::closeTab);
    connect(m_tabs, &DocumentTabWidget::newTabRequested, this, &QTextPadWindow::newDocument);

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *focus) {
        if (focus == editor() && m_searchWidget->isVisible())
            showSearchBar(false);
    });

    // Set up the editor and status for a clean, empty document
    resetEditor();
    updateTitle();

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

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) {
        if (m_defaultThemeAction->isChecked())
            setDefaultEditorTheme();
    });
#endif

    installEventFilter(this);
}

QTextPadWindow::~QTextPadWindow()
{
    // Prevent the stacks and editors from calling back into window state on shutdown
    m_closing = true;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto tab = static_cast<DocumentTab *>(m_tabs->widget(i));
        tab->undoStack()->disconnect(this);
        tab->editor()->disconnect(this);
    }
}

void QTextPadWindow::setOpenFilename(DocumentTab *tab, const QString &filename)
{
    const QString oldFilename = tab->filename;
    tab->filename = filename;

    // Another tab may still be showing the same file, so keep watching it
    if (!oldFilename.isEmpty() && !findTab(oldFilename, tab))
        m_fileWatcher->removePath(oldFilename);
    if (!filename.isEmpty() && !m_fileWatcher->files().contains(filename)) {
        if (!m_fileWatcher->addPath(filename))
            qWarning("Could not add file system watch for %s", qPrintable(filename));
    }
}

void QTextPadWindow::setOpenFilename(const QString &filename)
{
    setOpenFilename(currentTab(), filename);
}

DocumentTab *QTextPadWindow::currentTab() const
{
    return m_tabs ? static_cast<DocumentTab *>(m_tabs->currentWidget()) : Q_NULLPTR;
}

SyntaxTextEdit *QTextPadWindow::editor() const
{
    return currentTab()->editor();
}

QUndoStack *QTextPadWindow::undoStack() const
{
    return currentTab()->undoStack();
}

QString QTextPadWindow::textEncoding() const
{
    return currentTab()->textEncoding;
}

FileTypeInfo::LineEndingType QTextPadWindow::lineEndingMode() const
{
    return currentTab()->lineEndingMode;
}

QList<SyntaxTextEdit *> QTextPadWindow::editors() const
{
    QList<SyntaxTextEdit *> result;
    for (int i = 0; i < m_tabs->count(); ++i) {
        result.append(static_cast<DocumentTab *>(m_tabs->widget(i))->editor());
    }
    return result;
}

// Resolves symlinks and redundant segments when the file exists
// falls back to the absolute path for files that don't exist (yet).
static QString normalizedPath(const QString &path)
{
    const QFileInfo fi(path);
    const QString canon = fi.canonicalFilePath();
    return canon.isEmpty() ? fi.absoluteFilePath() : canon;
}

DocumentTab *QTextPadWindow::findTab(const QString &filename, const DocumentTab *exclude) const
{
    if (filename.isEmpty())
        return Q_NULLPTR;

    const QString normName = normalizedPath(filename);
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto tab = static_cast<DocumentTab *>(m_tabs->widget(i));
        if (tab == exclude || tab->filename.isEmpty())
            continue;
        if (normalizedPath(tab->filename).compare(normName, FILE_COMPARE_CS) == 0)
            return tab;
    }
    return Q_NULLPTR;
}

DocumentTab *QTextPadWindow::addDocumentTab()
{
    auto tab = new DocumentTab;
    SyntaxTextEdit *textEdit = tab->editor();
    QUndoStack *stack = tab->undoStack();

    QTextPadSettings settings;
    textEdit->setShowLineNumbers(settings.lineNumbers());
    textEdit->setShowFolding(settings.showFolding());
    textEdit->setAutoIndent(settings.autoIndent());
    textEdit->setMatchBraces(settings.matchBraces());
    textEdit->setHighlightCurrentLine(settings.highlightCurLine());
    textEdit->setShowIndentGuides(settings.indentationGuides());
    textEdit->setShowLongLineEdge(settings.showLongLineMargin());
    textEdit->setShowWhitespace(settings.showWhitespace());
    textEdit->setTabWidth(settings.tabWidth());
    textEdit->setIndentWidth(settings.indentWidth());
    textEdit->setLongLineWidth(settings.longLineWidth());
    textEdit->setDefaultFont(settings.editorFont());
    textEdit->setWordWrap(settings.wordWrap());
    textEdit->setIndentationMode(settings.indentMode());
    textEdit->setScrollPastEndOfFile(settings.scrollPastEndOfFile());
    textEdit->setOverwriteMode(m_overwriteModeAction && m_overwriteModeAction->isChecked());

    KSyntaxHighlighting::Theme theme;
    const QString themeName = settings.editorTheme();
    if (!themeName.isEmpty())
        theme = SyntaxTextEdit::syntaxRepo()->theme(themeName);
    if (theme.isValid())
        textEdit->setTheme(theme);
    else
        textEdit->setDefaultTheme();

    // Every tab gets its own undo stack
    textEdit->setExternalUndoRedo(true);
    connect(textEdit->document(), &QTextDocument::undoCommandAdded, tab,
            [tab] { tab->undoStack()->push(new TextEditorUndoCommand(tab->editor())); });
    connect(textEdit, &SyntaxTextEdit::undoRequested, stack, &QUndoStack::undo);
    connect(textEdit, &SyntaxTextEdit::redoRequested, stack, &QUndoStack::redo);

    textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(textEdit, &QPlainTextEdit::customContextMenuRequested,
            this, &QTextPadWindow::editorContextMenu);

    // Window-level actions only follow the stack/editor of the active tab
    connect(stack, &QUndoStack::canUndoChanged, this, [this, tab](bool enabled) {
        if (m_closing)
            return;
        if (tab == currentTab() && m_undoAction)
            m_undoAction->setEnabled(enabled);
    });
    connect(stack, &QUndoStack::canRedoChanged, this, [this, tab](bool enabled) {
        if (m_closing)
            return;
        if (tab == currentTab() && m_redoAction)
            m_redoAction->setEnabled(enabled);
    });
    connect(stack, &QUndoStack::cleanChanged, this, [this, tab](bool) {
        if (m_closing)
            return;
        if (tab == currentTab())
            updateTitle();
        else
            updateTabText(tab);
    });
    connect(textEdit, &QPlainTextEdit::copyAvailable, this, [this, tab](bool available) {
        if (m_closing)
            return;
        if (tab == currentTab() && m_deleteAction)
            m_deleteAction->setEnabled(available);
    });
    connect(textEdit, &SyntaxTextEdit::cursorPositionChanged,
            this, &QTextPadWindow::updateCursorPosition);
    connect(textEdit, &SyntaxTextEdit::selectionChanged,
            this, &QTextPadWindow::updateCursorPosition);
    connect(textEdit, &SyntaxTextEdit::textChanged, this, [this] {
        if (m_searchWidget && m_searchWidget->isVisible())
            showSearchBar(false);
    });

    m_tabs->addTab(tab, tr("Untitled"));
    return tab;
}

void QTextPadWindow::updateTabText(DocumentTab *tab)
{
    const int index = m_tabs->indexOf(tab);
    if (index < 0)
        return;

    QString name = tab->documentName();
    name.replace(QLatin1Char('&'), QLatin1String("&&"));
    if (!tab->undoStack()->isClean())
        name = QStringLiteral("* ") + name;

    m_tabs->setTabText(index, name);
    m_tabs->setTabToolTip(index, tab->absolutePath());
}

void QTextPadWindow::currentTabChanged(int index)
{
    if (index < 0 || !m_searchWidget)
        return;

    DocumentTab *tab = currentTab();
    SyntaxTextEdit *textEdit = tab->editor();

    m_searchWidget->setEditor(textEdit);
    SearchDialog::setActiveEditor(textEdit);
    showSearchBar(false);

    // Refresh the shared actions and the status bar from the active document
    m_undoAction->setEnabled(tab->undoStack()->canUndo());
    m_redoAction->setEnabled(tab->undoStack()->canRedo());
    m_deleteAction->setEnabled(textEdit->textCursor().hasSelection());
    textEdit->setOverwriteMode(m_overwriteModeAction->isChecked());
    m_reloadAction->setEnabled(documentExists());
    m_utfBOMAction->setChecked(tab->utfBOM);
    setEncoding(tab->textEncoding);
    setLineEndingMode(tab->lineEndingMode);

    auto syntax = SyntaxTextEdit::syntaxRepo()->definitionForName(textEdit->syntaxName());
    if (!syntax.isValid())
        syntax = SyntaxTextEdit::nullSyntax();
    updateSyntaxStatus(syntax);

    updateCursorPosition();
    updateIndentStatus();
    updateTitle();
    textEdit->setFocus(Qt::OtherFocusReason);

    if (!m_closing && QApplication::applicationState() == Qt::ApplicationActive)
        checkForModifications();
}

void QTextPadWindow::closeTab(int index)
{
    auto tab = static_cast<DocumentTab *>(m_tabs->widget(index));
    if (!tab)
        return;

    const bool lastTab = m_tabs->count() == 1;

    // closeEvent() takes care of prompting for the remaining document
    if (lastTab && QTextPadSettings().closeWindowOnLastTab()) {
        close();
        return;
    }

    m_tabs->setCurrentIndex(index);
    if (!promptForSave())
        return;

    if (lastTab) {
        // Keep the window around with a fresh, empty document
        resetEditor();
        updateTitle();
        return;
    }

    tab->deleteLater();
}

bool QTextPadWindow::closeAllTabs()
{
    if (!promptSaveAll())
        return false;

    while (m_tabs->count() > 1) {
        auto tab = static_cast<DocumentTab *>(m_tabs->widget(0));
        m_tabs->removeTab(0);
        tab->deleteLater();
    }

    // Reuse the last tab for a new, empty document
    resetEditor();
    updateTitle();
    return true;
}

void QTextPadWindow::closeOtherTabs()
{
    auto currentTab = m_tabs->currentIndex();
    auto currentTabWidget = m_tabs->widget(currentTab);
    if (!promptSaveAll(currentTab))
        return;

    int tabIndex = 0;
    while (m_tabs->count() > 1) {
        if (m_tabs->widget(tabIndex) == currentTabWidget) {
            Q_ASSERT(tabIndex == 0);
            tabIndex = 1;
        }

        auto tab = static_cast<DocumentTab *>(m_tabs->widget(tabIndex));
        m_tabs->removeTab(tabIndex);
        tab->deleteLater();
    }

    // Our tab should be the only one remaining
    if (m_tabs->count() > 0)
        m_tabs->setCurrentIndex(0);
}

bool QTextPadWindow::openDocument(const QString &filename, const QString &textEncoding)
{
    if (DocumentTab *existing = findTab(filename)) {
        m_tabs->setCurrentWidget(existing);
        return true;
    }

    // Reuse the current tab if it is just an empty, untouched "Untitled"
    DocumentTab *previous = currentTab();
    if (previous->filename.isEmpty() && !isDocumentModified()
            && previous->editor()->document()->isEmpty()) {
        return loadDocumentFrom(filename, textEncoding);
    }

    DocumentTab *tab = addDocumentTab();
    m_tabs->setCurrentWidget(tab);
    if (loadDocumentFrom(filename, textEncoding))
        return true;

    // Loading failed or was cancelled -- don't leave an empty tab behind
    m_tabs->setCurrentWidget(previous);
    m_tabs->removeTab(m_tabs->indexOf(tab));
    tab->deleteLater();
    return false;
}

void QTextPadWindow::syncIndentSettings()
{
    SyntaxTextEdit *current = editor();
    const int mode = static_cast<int>(current->indentationMode());
    const int tabWidth = current->tabWidth();
    const int indentWidth = current->indentWidth();
    for (auto ed : editors()) {
        if (ed == current)
            continue;
        ed->setIndentationMode(mode);
        ed->setTabWidth(tabWidth);
        ed->setIndentWidth(indentWidth);
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
        const QTextCursor cursor = editor()->textCursor();
        if (cursor.hasSelection())
            m_searchWidget->setSearchText(cursor.selectedText());
        m_searchWidget->activate();
    } else {
        editor()->clearLiveSearch();
        editor()->setFocus(Qt::OtherFocusReason);
    }
}

void QTextPadWindow::setSyntax(const KSyntaxHighlighting::Definition &syntax)
{
    editor()->setSyntax(syntax);
    updateSyntaxStatus(syntax);
}

void QTextPadWindow::updateSyntaxStatus(const KSyntaxHighlighting::Definition &syntax)
{
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

void QTextPadWindow::setEditorTheme(const KSyntaxHighlighting::Theme &theme)
{
    for (auto ed : editors())
        ed->setTheme(theme);

    // Update the menus when this is triggered via other callers
    for (const auto &action : m_themeActions->actions()) {
        if (!action->data().canConvert<KSyntaxHighlighting::Theme>())
            continue;

        const auto actionTheme = action->data().value<KSyntaxHighlighting::Theme>();
        if (actionTheme.filePath() == theme.filePath()) {
            action->setChecked(true);
            break;
        }
    }

    QTextPadSettings().setEditorTheme(theme.name());
}

void QTextPadWindow::setDefaultEditorTheme()
{
    for (auto ed : editors()) {
        ed->setDefaultTheme();
    }
    m_defaultThemeAction->setChecked(true);
    QTextPadSettings().clearEditorTheme();
}

void QTextPadWindow::setEncoding(const QString &codecName)
{
    currentTab()->textEncoding = codecName;

    // We may not directly match the passed encoding, so don't show a
    // radio check at all if we can't find the encoding.
    if (m_setEncodingActions->checkedAction())
        m_setEncodingActions->checkedAction()->setChecked(false);

    if (!QTextPadCharsets::codecForName(codecName.toLatin1())) {
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
    currentTab()->utfBOM = bom;
    m_utfBOMAction->setChecked(bom);
}

void QTextPadWindow::setOverwriteMode(bool overwrite)
{
    editor()->setOverwriteMode(overwrite);
    m_overwriteModeAction->setChecked(overwrite);
    m_insertLabel->setText(overwrite ? tr("OVR") : tr("INS"));
}

void QTextPadWindow::setAutoIndent(bool ai)
{
    for (auto ed : editors()) {
        ed->setAutoIndent(ai);
    }
    m_autoIndentAction->setChecked(ai);
    QTextPadSettings().setAutoIndent(ai);
    updateIndentStatus();
}

void QTextPadWindow::setLineEndingMode(FileTypeInfo::LineEndingType mode)
{
    currentTab()->lineEndingMode = mode;

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

bool QTextPadWindow::saveDocumentTo(const QString &filename)
{
    auto codec = QTextPadCharsets::codecForName(currentTab()->textEncoding.toLatin1());
    if (!codec) {
        QMessageBox::critical(this, QString(),
            tr("The selected encoding (%1) is invalid.  Please select a valid "
               "encoding before attempting to save.").arg(currentTab()->textEncoding));
        return false;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, QString(),
                              tr("Cannot open file %1 for writing").arg(filename));
        return false;
    }

    auto document = convertRawText(editor()->document()->toRawText());
    switch (currentTab()->lineEndingMode) {
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

    const QTextCursor cursor = editor()->textCursor();
    QTextPadSettings::setFileModes(filename, currentTab()->textEncoding, editor()->syntaxName(),
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
        currentTab()->fileState = FS_New;
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
        codec = QTextPadCharsets::codecForName(codecName.toLatin1());
        if (!codec) {
            qDebug("Invalid manually-specified encoding: %s",
                   codecName.toLocal8Bit().constData());
        }
    }
    if (!codec)
        codec = detect.textCodec();
    setEncoding(QString::fromLatin1(codec->name()));

    buffer.append(file.readAll());
    QString document = codec->toUnicode(buffer);
    if (!document.isEmpty() && document[0] == QChar(0xFEFF))
        document = document.mid(1);

    // Don't search while we're in the middle of loading a new file
    showSearchBar(false);

    // Don't let the syntax highlighter hinder us while setting the new content
    editor()->clear();
    setSyntax(SyntaxTextEdit::nullSyntax());
    editor()->setPlainText(document);
    editor()->document()->clearUndoRedoStacks();

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
    QTextPadSettings::setFileModes(filename, currentTab()->textEncoding, definition.name(), fileModes.lineNum);
    QTextPadSettings().addRecentFile(filename);
    populateRecentFiles();

    currentTab()->fileState = 0;
    currentTab()->cachedModTime = QFileInfo(file).lastModified();

    undoStack()->clear();
    undoStack()->setClean();
    m_reloadAction->setEnabled(true);
    setUtfBOM(detect.bomOffset() != 0);
    updateTitle();
    return true;
}

bool QTextPadWindow::isDocumentModified() const
{
    return !undoStack()->isClean();
}

bool QTextPadWindow::documentExists() const
{
    // Checking currentTab()->fileState is faster than asking the file system...
    return !currentTab()->filename.isEmpty() && !(currentTab()->fileState & FS_New);
}

void QTextPadWindow::gotoLine(int line, int column)
{
    editor()->moveCursorTo(line, column);
}

void QTextPadWindow::checkForModifications()
{
    if (m_closing || currentTab()->filename.isEmpty() || (currentTab()->fileState & FS_OutOfDate) != 0)
        return;

    QFileInfo info(currentTab()->filename);
    if (!info.exists()) {
        if (!(currentTab()->fileState & FS_New)) {
            auto result = QMessageBox::warning(this, tr("File Deleted"),
                    tr("File %1 was deleted by another program.").arg(currentTab()->filename),
                    QMessageBox::Ignore | QMessageBox::Close);
            if (result == QMessageBox::Close) {
                closeTab(m_tabs->currentIndex());
            } else if (result == QMessageBox::Ignore) {
                currentTab()->fileState = FS_New;
                currentTab()->cachedModTime = QDateTime();
                updateTitle();
            }
        }
    } else if ((currentTab()->fileState & FS_New) != 0 || info.lastModified() != currentTab()->cachedModTime) {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setWindowTitle(tr("File Modified"));
        msg.setText(tr("File %1 was modified by another program.").arg(currentTab()->filename));
        auto reloadButton = msg.addButton(tr("&Reload"), QMessageBox::AcceptRole);
        auto ignoreButton = msg.addButton(QMessageBox::Ignore);
        msg.setDefaultButton(reloadButton);

        msg.exec();
        if (msg.clickedButton() == reloadButton) {
            if (!loadDocumentFrom(currentTab()->filename))
                closeTab(m_tabs->currentIndex());
        } else if (msg.clickedButton() == ignoreButton) {
            currentTab()->fileState = FS_OutOfDate;
            updateTitle();
        }
    }
}

bool QTextPadWindow::promptForSave()
{
    if (documentExists()) {
        const QTextCursor cursor = editor()->textCursor();
        QTextPadSettings::setFileModes(currentTab()->filename, currentTab()->textEncoding,
                                       editor()->syntaxName(), cursor.blockNumber() + 1);
    }

    if (isDocumentModified()) {
        int response = QMessageBox::question(this, QString(),
                            tr("%1 has been modified.  Would you like to save your changes first?")
                            .arg(currentTab()->documentName()),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (response == QMessageBox::Cancel)
            return false;
        else if (response == QMessageBox::Yes)
            return saveDocument();
    }
    return true;
}

bool QTextPadWindow::promptSaveAll(int excludeIndex)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (i == excludeIndex)
            continue;

        m_tabs->setCurrentIndex(i);
        if (!promptForSave())
            return false;
    }
    return true;
}

bool QTextPadWindow::promptForDiscard()
{
    if (documentExists()) {
        const QTextCursor cursor = editor()->textCursor();
        QTextPadSettings::setFileModes(currentTab()->filename, currentTab()->textEncoding,
                                       editor()->syntaxName(), cursor.blockNumber() + 1);
    }

    if (isDocumentModified()) {
        int response = QMessageBox::question(this, QString(),
                            tr("%1 has been modified.  Are you sure you want to discard your changes?")
                            .arg(currentTab()->documentName()), QMessageBox::Yes | QMessageBox::No);
        if (response == QMessageBox::No)
            return false;
    }
    return true;
}

void QTextPadWindow::newDocument()
{
    m_tabs->setCurrentWidget(addDocumentTab());
    resetEditor();
    updateTitle();
}

void QTextPadWindow::resetEditor()
{
    editor()->clear();
    editor()->document()->clearUndoRedoStacks();

    setSyntax(SyntaxTextEdit::nullSyntax());
    setEncoding(QStringLiteral("UTF-8"));
#ifdef _WIN32
    setLineEndingMode(FileTypeInfo::CRLF);
#else
    // OSX uses LF as well, and we don't support building for classic MacOS
    setLineEndingMode(FileTypeInfo::LFOnly);
#endif

    setOpenFilename(QString());
    currentTab()->cachedModTime = QDateTime();
    undoStack()->clear();
    undoStack()->setClean();
    m_reloadAction->setEnabled(false);
    setUtfBOM(false);
}

bool QTextPadWindow::saveDocument()
{
    if (currentTab()->filename.isEmpty())
        return saveDocumentAs();
    if (!saveDocumentTo(currentTab()->filename))
        return false;

    currentTab()->fileState = 0;
    currentTab()->cachedModTime = QFileInfo(currentTab()->filename).lastModified();
    undoStack()->setClean();
    updateTitle();
    return true;
}

bool QTextPadWindow::saveDocumentAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save File As"),
                                                currentTab()->absolutePath());
    if (path.isEmpty())
        return false;
    if (!saveDocumentTo(path))
        return false;

    setOpenFilename(path);
    currentTab()->fileState = 0;
    currentTab()->cachedModTime = QFileInfo(path).lastModified();
    undoStack()->setClean();
    updateTitle();
    return true;
}

bool QTextPadWindow::saveDocumentCopy()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Copy As"),
                                                currentTab()->absolutePath());
    if (path.isEmpty())
        return false;
    return saveDocumentTo(path);
}

bool QTextPadWindow::loadDocument()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open File"),
                                                currentTab()->absolutePath());
    if (path.isEmpty())
        return false;

    return openDocument(path);
}

bool QTextPadWindow::reloadDocument()
{
    if (!documentExists())
        return true;
    if (!promptForDiscard())
        return false;
    return loadDocumentFrom(currentTab()->filename);
}

void QTextPadWindow::reloadDocumentEncoding(const QString &textEncoding)
{
    Q_ASSERT(documentExists());

    const QString oldEncoding = currentTab()->textEncoding;
    if (!promptForDiscard() || !loadDocumentFrom(currentTab()->filename, textEncoding))
        setEncoding(oldEncoding);
}

void QTextPadWindow::printDocument()
{
    QPrinter printer;
    printer.setDocName(currentTab()->documentName());

    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted)
        editor()->printDocument(&printer);
}

void QTextPadWindow::printPreviewDocument()
{
    QPrinter printer;
    printer.setDocName(currentTab()->documentName());

    QPrintPreviewDialog previewDialog(&printer, this);
    connect(&previewDialog, &QPrintPreviewDialog::paintRequested,
            editor(), &SyntaxTextEdit::printDocument);
    previewDialog.exec();
}

void QTextPadWindow::editorContextMenu(const QPoint &pos)
{
    auto menu = std::make_unique<QMenu>(editor());
    for (auto action : m_editorContextActions)
        menu->addAction(action);

#if 0
    // TODO: Qt provides a control code menu in RTL text mode, but
    // does not provide any public API way to use it without instantiating
    // QPlainTextEdit's default context menu :(
    if (QGuiApplication::styleHints()->useRtlExtensions()) {
        menu->addSeparator();
        // Private API
        auto ctrlCharMenu = new QUnicodeControlCharacterMenu(editor(), menu);
        menu->addMenu(ctrlCharMenu);
    }
#endif

    menu->exec(editor()->viewport()->mapToGlobal(pos));
}

void QTextPadWindow::updateCursorPosition()
{
    if (!m_positionLabel)
        return;

    const QTextCursor cursor = editor()->textCursor();
    const int column = editor()->textColumn(cursor.block().text(), cursor.positionInBlock());
    const int selectedChars = std::abs(cursor.selectionEnd() - cursor.selectionStart());
    QString positionText = tr("Line %1, Col %2")
                                .arg(cursor.blockNumber() + 1)
                                .arg(column + 1);
    if (selectedChars)
        positionText += tr(" (Selected: %1)").arg(selectedChars);
    m_positionLabel->setText(positionText);
}

void QTextPadWindow::updateTitle()
{
    QString title = currentTab()->documentName();
    if (m_showFilePath && !currentTab()->filename.isEmpty()) {
        QFileInfo fi(currentTab()->filename);
        title += QStringLiteral(" [%1]").arg(fi.absolutePath());
    }
    if ((currentTab()->fileState & FS_OutOfDate) != 0)
        title += tr(" (Not Current)");
    else if ((currentTab()->fileState & FS_New) != 0)
        title += tr(" (New File)");
    title += QStringLiteral(u" \u2013 qtextpad");  // n-dash
    if (isDocumentModified())
        title = QStringLiteral("* ") + title;

    setWindowTitle(title);
    updateTabText(currentTab());
}

void QTextPadWindow::nextInsertMode()
{
    setOverwriteMode(!editor()->overwriteMode());
}

void QTextPadWindow::nextLineEndingMode()
{
    switch (currentTab()->lineEndingMode) {
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
    const int tabWidth = editor()->tabWidth();
    const int indentWidth = editor()->indentWidth();
    const auto indentMode = editor()->indentationMode();

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
    QFont newFont = QFontDialog::getFont(&ok, editor()->defaultFont(), this,
                                         tr("Set Editor Font"));
    if (ok) {
        for (auto ed : editors())
            ed->setDefaultFont(newFont);
        QTextPadSettings().setEditorFont(newFont);
    }
}

void QTextPadWindow::changeEncoding(const QString &encoding)
{
    if (encoding == currentTab()->textEncoding)
        return;

    if (!documentExists()) {
        // Don't save changes in the undo stack if we are creating a new file
        setEncoding(encoding);
    } else {
        QMessageBox mbQuestion(QMessageBox::Question, tr("Change Document Encoding"),
               tr("The current document encoding is '%1'.  Would you like to:<ul>"
                  "<li><b>Reload</b> the existing file in the '%2' encoding, or</li>"
                  "<li><b>Convert</b> the current document's encoding to '%2'?</li></ul>")
                  .arg(currentTab()->textEncoding, encoding),
               QMessageBox::Cancel, this);
        auto reloadButton = mbQuestion.addButton(tr("&Reload"), QMessageBox::AcceptRole);
        auto convertButton = mbQuestion.addButton(tr("&Convert"), QMessageBox::AcceptRole);

        (void)mbQuestion.exec();
        if (mbQuestion.clickedButton() == convertButton) {
            auto command = new ChangeEncodingCommand(this, encoding);
            undoStack()->push(command);
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
        undoStack()->push(command);
    }
}

void QTextPadWindow::changeUtfBOM()
{
    if (documentExists()) {
        // Don't save changes in the undo stack if we are creating a new file
        auto command = new ChangeUtfBOMCommand(this);
        undoStack()->push(command);
    } else {
        currentTab()->utfBOM = m_utfBOMAction->isChecked();
    }
}

void QTextPadWindow::promptIndentSettings()
{
    auto dialog = new IndentSettingsDialog(this);
    dialog->loadSettings(editor());
    if (dialog->exec() == QDialog::Accepted) {
        dialog->applySettings(editor());
        syncIndentSettings();
        updateIndentStatus();
    }
}

void QTextPadWindow::promptLongLineWidth()
{
    bool ok;
    auto width = QInputDialog::getInt(this, tr("Long Line Width"),
            tr("Set Long Line Margin Position (characters)"),
            editor()->longLineWidth(), 0, std::numeric_limits<int>::max(), 1, &ok);
    if (ok) {
        for (auto ed : editors())
            ed->setLongLineWidth(width);
        QTextPadSettings().setLongLineWidth(width);
    }
}

void QTextPadWindow::navigateToLine()
{
    const QTextCursor cursor = editor()->textCursor();
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
    auto cursor = editor()->textCursor();
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
    modifySelection(editor(), [](const QString &selectedText) {
        return QLocale().toUpper(selectedText);
    });
}

void QTextPadWindow::downcaseSelection()
{
    modifySelection(editor(), [](const QString &selectedText) {
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
    auto cursor = editor()->textCursor();

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
    editor()->setTextCursor(cursor);
}

void QTextPadWindow::resizeEvent(QResizeEvent *event)
{
    if (event)
        QMainWindow::resizeEvent(event);

    // Move the search widget to the upper-right corner
    const QPoint editorPos = editor()->mapTo(this, QPoint(0, 0));
    QSize searchSize = m_searchWidget->sizeHint();
    m_searchWidget->resize(searchSize);
    m_searchWidget->move(editorPos.x() + editor()->viewport()->width() - searchSize.width() - 16,
                         editorPos.y());
}

void QTextPadWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    resizeEvent(nullptr);
}

void QTextPadWindow::closeEvent(QCloseEvent *event)
{
    // Offer to save every modified document before the window goes away
    m_closing = true;
    if (!promptSaveAll()) {
        m_closing = false;
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
            openDocument(recent);
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

    (void) m_syntaxMenu->addSeparator();
    auto updateAction = m_syntaxMenu->addAction(tr("Update Definitions"));
    connect(updateAction, &QAction::triggered, this, [this] {
        auto downloadDialog = new DefinitionDownloadDialog(SyntaxTextEdit::syntaxRepo(), this);
        downloadDialog->setAttribute(Qt::WA_DeleteOnClose);
        downloadDialog->show();
        downloadDialog->raise();
        downloadDialog->activateWindow();
    });
}

void QTextPadWindow::populateThemeMenu()
{
    m_themeActions = new QActionGroup(this);

    m_defaultThemeAction = m_themeMenu->addAction(tr("Automatic"));
    m_defaultThemeAction->setCheckable(true);
    m_defaultThemeAction->setActionGroup(m_themeActions);
    connect(m_defaultThemeAction, &QAction::triggered, this, [this] { setDefaultEditorTheme(); });
    m_themeMenu->addSeparator();

    KSyntaxHighlighting::Repository *syntaxRepo = SyntaxTextEdit::syntaxRepo();
    auto themeDefs = syntaxRepo->themes();
    std::sort(themeDefs.begin(), themeDefs.end(),
              [](const KSyntaxHighlighting::Theme &left, const KSyntaxHighlighting::Theme &right)
    {
        return left.translatedName().compare(right.translatedName(), Qt::CaseInsensitive) < 0;
    });
    for (const auto &theme : themeDefs) {
        auto item = m_themeMenu->addAction(theme.translatedName());
        item->setCheckable(true);
        item->setActionGroup(m_themeActions);
        item->setData(QVariant::fromValue(theme));
        connect(item, &QAction::triggered, this, [this, theme] { setEditorTheme(theme); });
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
        return left.first().compare(right.first(), Qt::CaseInsensitive) < 0;
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
            for (auto ed : editors()) {
                ed->setTabWidth(width);
            }
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
            for (auto ed : editors()) {
                ed->setIndentWidth(width);
            }
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
            for (auto ed : editors()) {
                ed->setIndentationMode(mode);
            }
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
