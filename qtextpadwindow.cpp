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

#include <QLabel>
#include <QToolButton>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFontDialog>
#include <QApplication>
#include <QWidgetAction>
#include <QClipboard>
#include <QTextBlock>
#include <QTextCodec>
#include <QTimer>

#include <Repository>
#include <Definition>
#include <Theme>
#include <KCharsets>

#include "activationlabel.h"
#include "syntaxtextedit.h"
#include "settingspopup.h"
#include "appsettings.h"

#define ICON(name)  QIcon::fromTheme(QStringLiteral(name), \
                                     QIcon(QStringLiteral(":/icons/" name ".png")))

class EncodingPopupAction : public QWidgetAction
{
public:
    EncodingPopupAction(QTextPadWindow *parent)
        : QWidgetAction(parent) { }

protected:
    QWidget *createWidget(QWidget *menuParent) Q_DECL_OVERRIDE
    {
        auto popup = new EncodingPopup(menuParent);
        connect(popup, &EncodingPopup::encodingSelected,
                [this](const QString &codecName) {
            qobject_cast<QTextPadWindow *>(parent())->setEncoding(codecName);

            // Don't close the popup right after clicking, so the user can
            // briefly see the visual feedback for the item they selected.
            QTimer::singleShot(100, []() {
                QApplication::activePopupWidget()->close();
            });
        });
        return popup;
    }
};

class SyntaxPopupAction : public QWidgetAction
{
public:
    SyntaxPopupAction(QTextPadWindow *parent)
        : QWidgetAction(parent) { }

protected:
    QWidget *createWidget(QWidget *menuParent) Q_DECL_OVERRIDE
    {
        auto popup = new SyntaxPopup(menuParent);
        connect(popup, &SyntaxPopup::syntaxSelected,
                [this](const KSyntaxHighlighting::Definition &syntax) {
            qobject_cast<QTextPadWindow *>(parent())->setSyntax(syntax);

            // Don't close the popup right after clicking, so the user can
            // briefly see the visual feedback for the item they selected.
            QTimer::singleShot(100, []() {
                QApplication::activePopupWidget()->close();
            });
        });
        return popup;
    }
};

QTextPadWindow::QTextPadWindow(QWidget *parent)
    : QMainWindow(parent), m_modified(false)
{
    m_editor = new SyntaxTextEdit(this);
    setCentralWidget(m_editor);
    m_editor->setFrameStyle(QFrame::NoFrame);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    auto newAction = fileMenu->addAction(ICON("document-new"), tr("&New"));
    newAction->setShortcut(QKeySequence::New);
    (void) fileMenu->addSeparator();
    auto openAction = fileMenu->addAction(ICON("document-open"), tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    m_recentFiles = fileMenu->addMenu(tr("Open &Recent"));
    populateRecentFiles();
    auto reloadAction = fileMenu->addAction(ICON("view-refresh"), tr("Re&load"));
    reloadAction->setShortcut(QKeySequence::Refresh);
    (void) fileMenu->addSeparator();
    auto saveAction = fileMenu->addAction(ICON("document-save"), tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    auto saveAsAction = fileMenu->addAction(ICON("document-save-as"), tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    auto saveCopyAction = fileMenu->addAction(ICON("document-save-as"), tr("Save &Copy..."));
    (void) fileMenu->addSeparator();
    auto printAction = fileMenu->addAction(ICON("document-print"), tr("&Print..."));
    printAction->setShortcut(QKeySequence::Print);
    auto printPreviewAction = fileMenu->addAction(ICON("document-preview"), tr("Print Previe&w"));
    (void) fileMenu->addSeparator();
    auto quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);

    //connect(newAction, &QAction::triggered, ...);
    //connect(openAction, &QAction::triggered, ...);
    //connect(reloadAction, &QAction::triggered, ...);
    //connect(saveAction, &QAction::triggered, ...);
    //connect(saveAsAction, &QAction::triggered, ...);
    //connect(saveCopyAction, &QAction::triggered, ...);
    //connect(printAction, &QAction::triggered, ...);
    //connect(printPreviewAction, &QAction::triggered, ...);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    auto undoAction = editMenu->addAction(ICON("edit-undo"), tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    auto redoAction = editMenu->addAction(ICON("edit-redo"), tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    (void) editMenu->addSeparator();
    auto cutAction = editMenu->addAction(ICON("edit-cut"), tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    auto copyAction = editMenu->addAction(ICON("edit-copy"), tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    auto pasteAction = editMenu->addAction(ICON("edit-paste"), tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    auto clearAction = editMenu->addAction(ICON("edit-delete"), tr("&Delete"));
    clearAction->setShortcut(QKeySequence::Delete);
    (void) editMenu->addSeparator();
    auto selectAllAction = editMenu->addAction(tr("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    (void) editMenu->addSeparator();
    m_overwiteModeAction = editMenu->addAction(tr("&Overwrite Mode"));
    m_overwiteModeAction->setShortcut(Qt::Key_Insert);
    m_overwiteModeAction->setCheckable(true);
    (void) editMenu->addSeparator();
    auto findAction = editMenu->addAction(ICON("edit-find"), tr("&Find..."));
    findAction->setShortcut(QKeySequence::Find);
    auto findNextAction = editMenu->addAction(tr("Find &Next"));
    findNextAction->setShortcut(QKeySequence::FindNext);
    auto findPrevAction = editMenu->addAction(tr("Find &Previous"));
    findPrevAction->setShortcut(QKeySequence::FindPrevious);
    auto replaceAction = editMenu->addAction(ICON("edit-find-replace"), tr("&Replace..."));
    replaceAction->setShortcut(QKeySequence::Replace);

    connect(undoAction, &QAction::triggered, m_editor, &QPlainTextEdit::undo);
    connect(redoAction, &QAction::triggered, m_editor, &QPlainTextEdit::redo);
    connect(cutAction, &QAction::triggered, m_editor, &QPlainTextEdit::cut);
    connect(copyAction, &QAction::triggered, m_editor, &QPlainTextEdit::copy);
    connect(pasteAction, &QAction::triggered, m_editor, &QPlainTextEdit::paste);
    connect(clearAction, &QAction::triggered, m_editor, &SyntaxTextEdit::deleteSelection);
    connect(selectAllAction, &QAction::triggered, m_editor, &QPlainTextEdit::selectAll);
    connect(m_overwiteModeAction, &QAction::toggled,
            this, &QTextPadWindow::setOverwriteMode);
    //connect(findAction, &QAction::triggered, ...);
    //connect(findNextAction, &QAction::triggered, ...);
    //connect(findPrevAction, &QAction::triggered, ...);
    //connect(replaceAction, &QAction::triggered, ...);

    connect(m_editor, &QPlainTextEdit::undoAvailable, undoAction, &QAction::setEnabled);
    undoAction->setEnabled(false);
    connect(m_editor, &QPlainTextEdit::redoAvailable, redoAction, &QAction::setEnabled);
    redoAction->setEnabled(false);
    connect(m_editor, &QPlainTextEdit::copyAvailable, cutAction, &QAction::setEnabled);
    cutAction->setEnabled(false);
    connect(m_editor, &QPlainTextEdit::copyAvailable, copyAction, &QAction::setEnabled);
    copyAction->setEnabled(false);

    connect(QApplication::clipboard(), &QClipboard::dataChanged, [this, pasteAction]() {
        pasteAction->setEnabled(m_editor->canPaste());
    });
    pasteAction->setEnabled(m_editor->canPaste());

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    auto fontAction = viewMenu->addAction(tr("Default &Font..."));
    m_themeMenu = viewMenu->addMenu(tr("&Theme"));
    populateThemeMenu();
    (void) viewMenu->addSeparator();
    auto wordWrapAction = viewMenu->addAction(tr("&Word Wrap"));
    wordWrapAction->setShortcut(Qt::CTRL | Qt::Key_W);
    wordWrapAction->setCheckable(true);
    auto longLineAction = viewMenu->addAction(tr("Long Line &Margin"));
    longLineAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_M);
    longLineAction->setCheckable(true);
    auto longLineSettingsAction = viewMenu->addAction(tr("Long Line Se&ttings..."));
    auto indentGuidesAction = viewMenu->addAction(tr("&Indentation Guides"));
    indentGuidesAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_T);
    indentGuidesAction->setCheckable(true);
    (void) viewMenu->addSeparator();
    auto showLineNumbersAction = viewMenu->addAction(tr("Line &Numbers"));
    showLineNumbersAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_N);
    showLineNumbersAction->setCheckable(true);
    auto showWhitespaceAction = viewMenu->addAction(tr("Show White&space"));
    showWhitespaceAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_W);
    showWhitespaceAction->setCheckable(true);
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

    connect(fontAction, &QAction::triggered, this, &QTextPadWindow::chooseEditorFont);
    connect(wordWrapAction, &QAction::toggled, m_editor, &SyntaxTextEdit::setWordWrap);
    connect(longLineAction, &QAction::toggled,
            m_editor, &SyntaxTextEdit::setShowLongLineEdge);
    //connect(longLineSettingsAction, &QAction::triggered, ...);
    connect(indentGuidesAction, &QAction::toggled,
            m_editor, &SyntaxTextEdit::setShowIndentGuides);
    connect(showLineNumbersAction, &QAction::toggled,
            m_editor, &SyntaxTextEdit::setShowLineNumbers);
    connect(showWhitespaceAction, &QAction::toggled,
            m_editor, &SyntaxTextEdit::setShowWhitespace);
    connect(showCurrentLineAction, &QAction::toggled,
            m_editor, &SyntaxTextEdit::setHighlightCurrentLine);
    connect(showMatchingBraces, &QAction::toggled,
            m_editor, &SyntaxTextEdit::setMatchBraces);
    connect(zoomInAction, &QAction::triggered, m_editor, &SyntaxTextEdit::zoomIn);
    connect(zoomOutAction, &QAction::triggered, m_editor, &SyntaxTextEdit::zoomOut);
    connect(zoomResetAction, &QAction::triggered, m_editor, &SyntaxTextEdit::zoomReset);

    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    m_syntaxMenu = settingsMenu->addMenu(tr("&Syntax"));
    populateSyntaxMenu();
    m_encodingMenu = settingsMenu->addMenu(tr("&Encoding"));
    populateEncodingMenu();
    auto lineEndingMenu = settingsMenu->addMenu(tr("&Line Endings"));
    m_lineEndingActions = new QActionGroup(this);
    auto crOnlyAction = lineEndingMenu->addAction(tr("Classic Mac (CR)"));
    crOnlyAction->setCheckable(true);
    crOnlyAction->setActionGroup(m_lineEndingActions);
    crOnlyAction->setData(static_cast<int>(CROnly));
    auto lfOnlyAction = lineEndingMenu->addAction(tr("UNIX (LF)"));
    lfOnlyAction->setCheckable(true);
    lfOnlyAction->setActionGroup(m_lineEndingActions);
    lfOnlyAction->setData(static_cast<int>(LFOnly));
    auto crlfAction = lineEndingMenu->addAction(tr("Windows/DOS (CRLF)"));
    crlfAction->setCheckable(true);
    crlfAction->setActionGroup(m_lineEndingActions);
    crlfAction->setData(static_cast<int>(CRLF));
    (void) settingsMenu->addSeparator();
    auto tabSettingsAction = settingsMenu->addAction(tr("&Tab Settings..."));
    m_autoIndentAction = settingsMenu->addAction(tr("&Auto Indent"));
    m_autoIndentAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_I);
    m_autoIndentAction->setCheckable(true);

    connect(crOnlyAction, &QAction::triggered, [this]() { setLineEndingMode(CROnly); });
    connect(lfOnlyAction, &QAction::triggered, [this]() { setLineEndingMode(LFOnly); });
    connect(crlfAction, &QAction::triggered, [this]() { setLineEndingMode(CRLF); });
    //connect(tabSettingsAction, &QAction::triggered, ...);
    connect(m_autoIndentAction, &QAction::toggled, this, &QTextPadWindow::setAutoIndent);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto aboutAction = helpMenu->addAction(ICON("help-about"), tr("&About..."));
    aboutAction->setShortcut(QKeySequence::HelpContents);

    auto toolBar = addToolBar(tr("Toolbar"));
    toolBar->setIconSize(QSize(22, 22));
    toolBar->setMovable(false);
    toolBar->addAction(newAction);
    toolBar->addAction(openAction);
    toolBar->addAction(saveAction);
    (void) toolBar->addSeparator();
    toolBar->addAction(undoAction);
    toolBar->addAction(redoAction);
    (void) toolBar->addSeparator();
    toolBar->addAction(cutAction);
    toolBar->addAction(copyAction);
    toolBar->addAction(pasteAction);
    (void) toolBar->addSeparator();
    toolBar->addAction(findAction);
    toolBar->addAction(replaceAction);

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

    //connect(m_positionLabel, &ActivationLabel::activated, ...);
    connect(m_insertLabel, &ActivationLabel::activated,
            this, &QTextPadWindow::nextInsertMode);
    connect(m_crlfLabel, &ActivationLabel::activated,
            this, &QTextPadWindow::nextLineEndingMode);

    wordWrapAction->setChecked(m_editor->wordWrap());
    longLineAction->setChecked(m_editor->showLongLineEdge());
    indentGuidesAction->setChecked(m_editor->showIndentGuides());
    showLineNumbersAction->setChecked(m_editor->showLineNumbers());
    showWhitespaceAction->setChecked(m_editor->showWhitespace());
    showCurrentLineAction->setChecked(m_editor->highlightCurrentLine());
    showMatchingBraces->setChecked(m_editor->matchBraces());
    m_autoIndentAction->setChecked(m_editor->autoIndent());

    setSyntax(SyntaxTextEdit::nullSyntax());
    setEncoding("UTF-8");

    QTextPadSettings settings;
    auto theme = (m_editor->palette().color(QPalette::Base).lightness() < 128)
                 ? SyntaxTextEdit::syntaxRepo()->defaultTheme(KSyntaxHighlighting::Repository::DarkTheme)
                 : SyntaxTextEdit::syntaxRepo()->defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
    QString themeName = settings.editorTheme();
    if (!themeName.isEmpty()) {
        auto requestedTheme = SyntaxTextEdit::syntaxRepo()->theme(themeName);
        if (requestedTheme.isValid())
            theme = requestedTheme;
    }
    setTheme(theme);

    m_insertLabel->setMinimumWidth(QFontMetrics(m_insertLabel->font()).width("OVR") + 4);
    m_crlfLabel->setMinimumWidth(QFontMetrics(m_crlfLabel->font()).width("CRLF") + 4);
    setOverwriteMode(false);
#ifdef _WIN32
    setLineEndingMode(CRLF);
#else
    // OSX uses LF as well, and we don't support building for classic MacOS
    setLineEndingMode(LFOnly);
#endif

    connect(m_editor, &SyntaxTextEdit::cursorPositionChanged,
            this, &QTextPadWindow::updateCursorPosition);
    connect(m_editor->document(), &QTextDocument::modificationChanged,
            this, &QTextPadWindow::modificationStatusChanged);

    m_documentTitle = tr("Untitled");
    updateTitle();

    resize(settings.windowSize());
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
}

void QTextPadWindow::setEncoding(const QString &codecName)
{
    // We may not directly match the passed encoding, so don't show a
    // radio check at all if we can't find the encoding.
    if (m_encodingActions->checkedAction())
        m_encodingActions->checkedAction()->setChecked(false);

    QTextCodec *codec = KCharsets::charsets()->codecForName(codecName);
    if (!codec) {
        qWarning("Invalid codec selected");
        m_encodingButton->setText(tr("Invalid"));
    } else {
        // Use the passed name for UI consistency
        m_encodingButton->setText(codecName);
    }

    // Update the menus when this is triggered via other callers
    for (const auto &action : m_encodingActions->actions()) {
        const auto actionCodec = action->data().toString();
        if (actionCodec == codecName) {
            action->setChecked(true);
            break;
        }
    }
}

void QTextPadWindow::setOverwriteMode(bool overwrite)
{
    m_editor->setOverwriteMode(overwrite);
    m_overwiteModeAction->setChecked(overwrite);
    m_insertLabel->setText(overwrite ? tr("OVR") : tr("INS"));
}

void QTextPadWindow::setAutoIndent(bool ai)
{
    m_editor->setAutoIndent(ai);
    m_autoIndentAction->setChecked(ai);
    updateIndentStatus();
}

void QTextPadWindow::setLineEndingMode(LineEndingMode mode)
{
    m_lineEndingMode = mode;

    switch (mode) {
    case CROnly:
        m_crlfLabel->setText(QStringLiteral("CR"));
        break;
    case LFOnly:
        m_crlfLabel->setText(QStringLiteral("LF"));
        break;
    case CRLF:
        m_crlfLabel->setText(QStringLiteral("CRLF"));
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

void QTextPadWindow::updateCursorPosition()
{
    const QTextCursor cursor = m_editor->textCursor();
    const int column = m_editor->textColumn(cursor.block().text(), cursor.positionInBlock());
    m_positionLabel->setText(QString("Line %1, Col %2")
                             .arg(cursor.blockNumber() + 1)
                             .arg(column + 1));
}

void QTextPadWindow::updateTitle()
{
    QString title = m_documentTitle + QStringLiteral(" – QTextPad");  // n-dash
    if (m_modified)
        title = QStringLiteral("* ") + title;

    setWindowTitle(title);
}

void QTextPadWindow::modificationStatusChanged(bool modified)
{
    m_modified = modified;
    updateTitle();
}

void QTextPadWindow::nextInsertMode()
{
    setOverwriteMode(!m_editor->overwriteMode());
}

void QTextPadWindow::nextLineEndingMode()
{
    switch (m_lineEndingMode) {
    case CROnly:
        setLineEndingMode(LFOnly);
        break;
    case LFOnly:
        setLineEndingMode(CRLF);
        break;
    case CRLF:
        setLineEndingMode(CROnly);
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
                                         tr("Default Editor Font"));
    if (ok)
        m_editor->setDefaultFont(newFont);
}

void QTextPadWindow::closeEvent(QCloseEvent *e)
{
    // TODO:  Prompt for save
    QTextPadSettings settings;
    settings.setWindowSize(size());
    e->accept();
}

void QTextPadWindow::populateRecentFiles()
{
    // TODO: Save and retrieve recent files list
    (void) m_recentFiles->addSeparator();
    auto clearListAction = m_recentFiles->addAction(tr("Clear List"));
    connect(clearListAction, &QAction::triggered, [this]() {
        m_recentFiles->clear();
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
    connect(plainText, &QAction::triggered, [this]() {
        setSyntax(SyntaxTextEdit::nullSyntax());
    });

    KSyntaxHighlighting::Repository *syntaxRepo = SyntaxTextEdit::syntaxRepo();
    const auto syntaxDefs = syntaxRepo->definitions();
    QMap<QString, QMenu *> groupMenus;
    for (const auto def : syntaxDefs) {
        if (def.isHidden())
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
        connect(item, &QAction::triggered, [this, def]() { setSyntax(def); });
    }
}

void QTextPadWindow::populateThemeMenu()
{
    m_themeActions = new QActionGroup(this);

    KSyntaxHighlighting::Repository *syntaxRepo = SyntaxTextEdit::syntaxRepo();
    const auto themeDefs = syntaxRepo->themes();
    for (const auto theme : themeDefs) {
        auto item = m_themeMenu->addAction(theme.translatedName());
        item->setCheckable(true);
        item->setActionGroup(m_themeActions);
        item->setData(QVariant::fromValue(theme));
        connect(item, &QAction::triggered, [this, theme]() { setTheme(theme); });
    }
}

void QTextPadWindow::populateEncodingMenu()
{
    m_encodingActions = new QActionGroup(this);
    auto charsets = KCharsets::charsets();
    auto encodingScripts = charsets->encodingsByScript();

    // Sort the lists by script/region name
    std::sort(encodingScripts.begin(), encodingScripts.end(),
              [](const QStringList &left, const QStringList &right)
    {
        return left.first() < right.first();
    });

    for (const auto encodingList : encodingScripts) {
        QMenu *parentMenu = m_encodingMenu->addMenu(encodingList.first());
        for (int i = 1; i < encodingList.size(); ++i) {
            QString codecName = encodingList.at(i);
            auto item = parentMenu->addAction(codecName);
            item->setCheckable(true);
            item->setActionGroup(m_encodingActions);
            item->setData(codecName);
            connect(item, &QAction::triggered, [this, codecName]() {
                setEncoding(codecName);
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
        connect(action, &QAction::triggered, [this, action]() {
            m_editor->setTabWidth(action->data().toInt());
            updateIndentStatus();
        });
    }
    //connect(tabWidthOtherAction, &QAction::triggered, ...);

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
        connect(action, &QAction::triggered, [this, action]() {
            m_editor->setIndentWidth(action->data().toInt());
            updateIndentStatus();
        });
    }
    //connect(indentWidthOtherAction, &QAction::triggered, ...);

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
        connect(action, &QAction::triggered, [this, action]() {
            const auto mode = static_cast<SyntaxTextEdit::IndentationMode>(action->data().toInt());
            m_editor->setIndentationMode(mode);
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
