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

#include "syntaxtextedit.h"

#include <QScrollBar>
#include <QTextBlock>
#include <QPainter>
#include <QPrinter>
#include <QRegularExpression>
#include <QStack>

#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>

#include <cmath>

#include "appsettings.h"

enum SyntaxTextEdit_Config
{
    Config_ShowLineNumbers = (1U<<0),
    Config_AutoIndent = (1U<<1),
    Config_MatchBraces = (1U<<2),
    Config_HighlightCurLine = (1U<<3),
    Config_IndentGuides = (1U<<4),
    Config_LongLineEdge = (1U<<5),
};

class LineNumberMargin : public QWidget
{
public:
    explicit LineNumberMargin(SyntaxTextEdit *editor)
        : QWidget(editor), m_editor(editor) { }

    QSize sizeHint() const Q_DECL_OVERRIDE
    {
        return QSize(m_editor->lineMarginWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE
    {
        m_editor->paintLineNumbers(e);
    }

private:
    SyntaxTextEdit *m_editor;
};

class WhitespaceSyntaxHighlighter : public KSyntaxHighlighting::SyntaxHighlighter
{
public:
    explicit WhitespaceSyntaxHighlighter(QTextDocument *document)
        : KSyntaxHighlighting::SyntaxHighlighter(document)
    { }

    void highlightBlock(const QString &text) Q_DECL_OVERRIDE
    {
        KSyntaxHighlighting::SyntaxHighlighter::highlightBlock(text);

        static const QRegularExpression ws_regex("\\s+");
        auto iter = ws_regex.globalMatch(text);
        QTextCharFormat ws_format;
        const QBrush ws_brush(theme().editorColor(KSyntaxHighlighting::Theme::TabMarker));
        ws_format.setForeground(ws_brush);
        while (iter.hasNext()) {
            const auto match = iter.next();
            setFormat(match.capturedStart(), match.capturedLength(), ws_format);
        }
    }
};

KSyntaxHighlighting::Repository *SyntaxTextEdit::syntaxRepo()
{
    static KSyntaxHighlighting::Repository s_syntaxRepo;
    return &s_syntaxRepo;
}

const KSyntaxHighlighting::Definition &SyntaxTextEdit::nullSyntax()
{
    static KSyntaxHighlighting::Definition s_nullSyntax;
    return s_nullSyntax;
}

SyntaxTextEdit::SyntaxTextEdit(QWidget *parent)
    : QPlainTextEdit(parent), m_tabCharSize(4), m_indentWidth(4),
      m_longLineMarker(80), m_config(), m_indentationMode(),
      m_originalFontSize()
{
    m_lineMargin = new LineNumberMargin(this);
    m_highlighter = new WhitespaceSyntaxHighlighter(document());

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &SyntaxTextEdit::updateMargins);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &SyntaxTextEdit::updateLineNumbers);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &SyntaxTextEdit::updateCursor);

    // Default editor configuration
    QTextPadSettings settings;

    if (settings.lineNumbers())
        m_config |= Config_ShowLineNumbers;
    if (settings.autoIndent())
        m_config |= Config_AutoIndent;
    if (settings.matchBraces())
        m_config |= Config_MatchBraces;
    if (settings.highlightCurLine())
        m_config |= Config_HighlightCurLine;
    if (settings.indentationGuides())
        m_config |= Config_IndentGuides;
    if (settings.showLongLineMargin())
        m_config |= Config_LongLineEdge;

    m_tabCharSize = settings.tabWidth();
    m_indentWidth = settings.indentWidth();
    m_longLineMarker = settings.longLineWidth();

    setDefaultFont(settings.editorFont());
    setWordWrap(settings.wordWrap());
    setIndentationMode(settings.indentMode());

    // See comment in SyntaxTextEdit::setScrollPastEndOfFile()
    setCenterOnScroll(settings.scrollPastEndOfFile());

    QTextOption opt = document()->defaultTextOption();
    opt.setFlags(opt.flags() | QTextOption::AddSpaceForLineAndParagraphSeparators);
    if (settings.showWhitespace())
        opt.setFlags(opt.flags() | QTextOption::ShowTabsAndSpaces);
    else
        opt.setFlags(opt.flags() & ~QTextOption::ShowTabsAndSpaces);
    document()->setDefaultTextOption(opt);
}

bool SyntaxTextEdit::haveSelection() const
{
    const QTextCursor cursor = textCursor();
    return cursor.anchor() != cursor.position();
}

void SyntaxTextEdit::deleteSelection()
{
    QTextCursor cursor = textCursor();
    cursor.removeSelectedText();
}

void SyntaxTextEdit::deleteLines()
{
    QTextCursor cursor = textCursor();
    if (haveSelection()) {
        const int startPos = cursor.selectionStart();
        const int endPos = cursor.selectionEnd();
        cursor.setPosition(startPos);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);
        if (!cursor.atBlockStart())
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
    } else {
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
}

int SyntaxTextEdit::lineMarginWidth()
{
    if (!showLineNumbers())
        return 0;

    int digits = 1;
    int maxLine = qMax(1, blockCount());
    while (maxLine >= 10) {
        maxLine /= 10;
        ++digits;
    }
    return fontMetrics().boundingRect(QString(digits + 1, '0')).width() + 2;
}

void SyntaxTextEdit::paintLineNumbers(QPaintEvent *paintEvent)
{
    if (!showLineNumbers())
        return;

    QPainter painter(m_lineMargin);
    painter.fillRect(paintEvent->rect(), m_lineMarginBg);

    QTextBlock block = firstVisibleBlock();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    qreal bottom = top + blockBoundingRect(block).height();
    const QFontMetricsF metrics(font());
    const qreal offset = metrics.width(QLatin1Char('0')) / 2.0;
    QTextCursor cursor = textCursor();

    while (block.isValid() && top <= paintEvent->rect().bottom()) {
        if (block.isVisible() && bottom >= paintEvent->rect().top()) {
            const QString lineNum = QString::number(block.blockNumber() + 1);
            if (block.blockNumber() == cursor.blockNumber())
                painter.setPen(m_cursorLineNum);
            else
                painter.setPen(m_lineMarginFg);
            const QRectF numberRect(0, top, m_lineMargin->width() - offset,
                                    metrics.height());
            painter.drawText(numberRect, Qt::AlignRight, lineNum);
        }

        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
    }
}

void SyntaxTextEdit::setShowLineNumbers(bool show)
{
    if (show)
        m_config |= Config_ShowLineNumbers;
    else
        m_config &= ~Config_ShowLineNumbers;
    updateMargins();
    m_lineMargin->update();

    QTextPadSettings().setLineNumbers(show);
}

bool SyntaxTextEdit::showLineNumbers() const
{
    return !!(m_config & Config_ShowLineNumbers);
}

void SyntaxTextEdit::setShowWhitespace(bool show)
{
    QTextOption opt = document()->defaultTextOption();
    if (show)
        opt.setFlags(opt.flags() | QTextOption::ShowTabsAndSpaces);
    else
        opt.setFlags(opt.flags() & ~QTextOption::ShowTabsAndSpaces);
    document()->setDefaultTextOption(opt);

    QTextPadSettings().setShowWhitespace(show);
}

bool SyntaxTextEdit::showWhitespace() const
{
    const QTextOption opt = document()->defaultTextOption();
    return !!(opt.flags() & QTextOption::ShowTabsAndSpaces);
}

void SyntaxTextEdit::setScrollPastEndOfFile(bool scroll)
{
    // This feature, counter-intuitively, scrolls the document such that the
    // cursor is in the center ONLY when moving the cursor -- it does NOT
    // reposition the cursor when normal scrolling occurs.  Furthermore, this
    // property is the only way to enable scrolling past the last line of
    // the document.  TL;DR: This property is poorly named.
    setCenterOnScroll(scroll);

    QTextPadSettings().setScrollPastEndOfFile(scroll);
}

bool SyntaxTextEdit::scrollPastEndOfFile() const
{
    return centerOnScroll();
}

void SyntaxTextEdit::setHighlightCurrentLine(bool show)
{
    if (show)
        m_config |= Config_HighlightCurLine;
    else
        m_config &= ~Config_HighlightCurLine;
    viewport()->update();

    QTextPadSettings().setHighlightCurLine(show);
}

bool SyntaxTextEdit::highlightCurrentLine() const
{
    return !!(m_config & Config_HighlightCurLine);
}

void SyntaxTextEdit::setTabWidth(int width)
{
    m_tabCharSize = width;
    updateTabMetrics();

    QTextPadSettings().setTabWidth(width);
}

void SyntaxTextEdit::setIndentWidth(int width)
{
    m_indentWidth = width;
    QTextPadSettings().setIndentWidth(width);
}

void SyntaxTextEdit::updateTabMetrics()
{
    // setTabStopWidth only allows int widths, which doesn't line up correctly
    // on many fonts.  Hack from QtCreator: Set it in the text option instead
    const qreal tabWidth = QFontMetricsF(font()).width(QString(m_tabCharSize, ' '));
    QTextOption opt = document()->defaultTextOption();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    opt.setTabStopDistance(tabWidth);
#else
    opt.setTabStop(tabWidth);
#endif
    document()->setDefaultTextOption(opt);
}

void SyntaxTextEdit::setIndentationMode(int mode)
{
    if (mode < 0 || mode >= IndentMode_MAX) {
        // Set default indentation mode instead
        m_indentationMode = IndentSpaces;
    } else {
        m_indentationMode = static_cast<IndentationMode>(mode);
    }

    // Save the real indentation mode
    QTextPadSettings().setIndentMode(static_cast<int>(m_indentationMode));
}

int SyntaxTextEdit::textColumn(const QString &block, int positionInBlock) const
{
    int column = 0;
    for (int i = 0; i < positionInBlock; ++i) {
        if (block.at(i) == QLatin1Char('\t'))
            column = column - (column % m_tabCharSize) + m_tabCharSize;
        else
            ++column;
    }
    return column;
}

void SyntaxTextEdit::moveCursorTo(int line, int column)
{
    const auto block = document()->findBlockByNumber(line - 1);
    if (!block.isValid() && line > 0) {
        // Just navigate to the end of the file if we don't have the requested
        // line number.
        QTextCursor cursor(document());
        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);
        return;
    }

    QTextCursor cursor(block);
    if (column > 0) {
        const QString blockText = block.text();
        int columnIndex = 0, cursorIndex = 0;
        for (; cursorIndex < blockText.size(); ++cursorIndex) {
            if (columnIndex >= column - 1)
                break;
            if (blockText.at(cursorIndex) == QLatin1Char('\t'))
                columnIndex = columnIndex - (columnIndex % m_tabCharSize) + m_tabCharSize;
            else
                ++columnIndex;
        }
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor,
                            cursorIndex);
    }
    setTextCursor(cursor);
}

void SyntaxTextEdit::moveLines(QTextCursor::MoveOperation op)
{
    auto cursor = textCursor();

    cursor.beginEditBlock();
    int startPos = cursor.position();
    int endPos = cursor.anchor();
    cursor.setPosition(qMin(startPos, endPos));
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
    if (startPos == endPos || !cursor.atBlockStart())
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);

    const auto moveText = cursor.selectedText();
    cursor.removeSelectedText();
    const int positionStart = cursor.position();
    cursor.movePosition(op);
    const int postionDelta = cursor.position() - positionStart;
    cursor.insertText(moveText);

    cursor.setPosition(endPos + postionDelta);
    if (startPos != endPos)
        cursor.setPosition(startPos + postionDelta, QTextCursor::KeepAnchor);
    cursor.endEditBlock();

    setTextCursor(cursor);
}

void SyntaxTextEdit::smartHome(QTextCursor::MoveMode moveMode)
{
    auto cursor = textCursor();

    int leadingIndent = 0;
    const QString blockText = cursor.block().text();
    for (const auto ch : blockText) {
        if (ch.isSpace())
            leadingIndent += 1;
        else
            break;
    }
    int cursorPos = cursor.positionInBlock();
    cursor.movePosition(QTextCursor::StartOfLine, moveMode);
    if (cursor.positionInBlock() == 0 && cursorPos != leadingIndent)
        cursor.movePosition(QTextCursor::NextCharacter, moveMode, leadingIndent);

    setTextCursor(cursor);
    updateCursor();
}

void SyntaxTextEdit::setAutoIndent(bool ai)
{
    if (ai)
        m_config |= Config_AutoIndent;
    else
        m_config &= ~Config_AutoIndent;

    QTextPadSettings().setAutoIndent(ai);
}

bool SyntaxTextEdit::autoIndent() const
{
    return !!(m_config & Config_AutoIndent);
}

void SyntaxTextEdit::setShowLongLineEdge(bool show)
{
    if (show)
        m_config |= Config_LongLineEdge;
    else
        m_config &= ~Config_LongLineEdge;
    viewport()->update();

    QTextPadSettings().setShowLongLineMargin(show);
}

bool SyntaxTextEdit::showLongLineEdge() const
{
    return !!(m_config & Config_LongLineEdge);
}

void SyntaxTextEdit::setLongLineWidth(int pos)
{
    m_longLineMarker = pos;
    viewport()->update();

    QTextPadSettings().setLongLineWidth(pos);
}

void SyntaxTextEdit::setShowIndentGuides(bool show)
{
    if (show)
        m_config |= Config_IndentGuides;
    else
        m_config &= ~Config_IndentGuides;
    viewport()->update();

    QTextPadSettings().setIndentationGuides(show);
}

bool SyntaxTextEdit::showIndentGuides() const
{
    return !!(m_config & Config_IndentGuides);
}

void SyntaxTextEdit::setWordWrap(bool wrap)
{
    setWordWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere
                         : QTextOption::NoWrap);
    QTextPadSettings().setWordWrap(wrap);
}

bool SyntaxTextEdit::wordWrap() const
{
    return wordWrapMode() != QTextOption::NoWrap;
}

void SyntaxTextEdit::setMatchBraces(bool match)
{
    if (match)
        m_config |= Config_MatchBraces;
    else
        m_config &= ~Config_MatchBraces;
    updateCursor();

    QTextPadSettings().setMatchBraces(match);
}

bool SyntaxTextEdit::matchBraces() const
{
    return !!(m_config & Config_MatchBraces);
}

void SyntaxTextEdit::setDefaultFont(const QFont &font)
{
    // Note:  This will reset the zoom factor to 100%
    setFont(font);
    m_originalFontSize = font.pointSize();
    updateMargins();
    updateTabMetrics();

    QTextPadSettings().setEditorFont(font);
}

QFont SyntaxTextEdit::defaultFont() const
{
    QFont baseFont = font();
    baseFont.setPointSize(m_originalFontSize);
    return baseFont;
}

void SyntaxTextEdit::setTheme(const KSyntaxHighlighting::Theme &theme)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Text, theme.textColor(KSyntaxHighlighting::Theme::Normal));
    pal.setColor(QPalette::Base, theme.editorColor(KSyntaxHighlighting::Theme::BackgroundColor));
    pal.setColor(QPalette::Highlight, theme.editorColor(KSyntaxHighlighting::Theme::TextSelection));
    setPalette(pal);

    bool darkTheme = pal.color(QPalette::Base).lightness() < 128;

    // Cache other colors used by the widget
    m_lineMarginFg = theme.editorColor(KSyntaxHighlighting::Theme::LineNumbers);
    m_lineMarginBg = theme.editorColor(KSyntaxHighlighting::Theme::IconBorder);
    m_cursorLineBg = theme.editorColor(KSyntaxHighlighting::Theme::CurrentLine);
    m_cursorLineNum = theme.editorColor(KSyntaxHighlighting::Theme::CurrentLineNumber);
    m_longLineBg = theme.editorColor(KSyntaxHighlighting::Theme::WordWrapMarker);
    m_longLineEdge = darkTheme ? m_longLineBg.lighter(120) : m_longLineBg.darker(120);
    m_longLineCursorBg = darkTheme ? m_cursorLineBg.lighter(110) : m_cursorLineBg.darker(110);
    m_indentGuideFg = theme.editorColor(KSyntaxHighlighting::Theme::IndentationLine);
    m_braceMatchBg = theme.editorColor(KSyntaxHighlighting::Theme::BracketMatching);
    m_errorBg = theme.editorColor(KSyntaxHighlighting::Theme::MarkError);

    m_highlighter->setTheme(theme);
    m_highlighter->rehighlight();
}

void SyntaxTextEdit::setSyntax(const KSyntaxHighlighting::Definition &syntax)
{
    m_highlighter->setDefinition(syntax);
}

void SyntaxTextEdit::updateMargins()
{
    setViewportMargins(lineMarginWidth(), 0, 0, 0);
}

void SyntaxTextEdit::updateLineNumbers(const QRect &rect, int dy)
{
    if (dy)
        m_lineMargin->scroll(0, dy);
    else
        m_lineMargin->update(0, rect.y(), m_lineMargin->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateMargins();
}

static bool isQuote(const QChar &ch)
{
    return ch == QLatin1Char('"') || ch == QLatin1Char('\'');
}

static bool braceMatch(const QChar &left, const QChar &right)
{
    switch (left.unicode()) {
    case '{':
        return right == '}';
    case '(':
        return right == ')';
    case '[':
        return right == ']';
    case '}':
        return right == '{';
    case ')':
        return right == '(';
    case ']':
        return right == '[';
    default:
        return false;
    }
}

struct BraceMatchResult
{
    BraceMatchResult() : position(-1), validMatch(false) { }
    BraceMatchResult(int pos, bool valid) : position(pos), validMatch(valid) { }

    int position;
    bool validMatch;
};

static BraceMatchResult findNextBrace(QTextBlock block, int position)
{
    QStack<QChar> balance;
    do {
        QString text = block.text();
        while (position < text.size()) {
            const QChar ch = text.at(position);
            if (isQuote(ch)) {
                if (!balance.isEmpty() && balance.top() == ch)
                    (void) balance.pop();
                else
                    balance.push(ch);
            } else if (!balance.isEmpty() && isQuote(balance.top())) {
                /* Don't look for matching braces until we exit the quote */
            } else if (ch == '(' || ch == '[' || ch == '{') {
                balance.push(ch);
            } else if (ch == ')' || ch == ']' || ch == '}') {
                if (balance.isEmpty())
                    return BraceMatchResult();
                const QChar match = balance.pop();
                if (balance.isEmpty())
                    return BraceMatchResult(block.position() + position, braceMatch(match, ch));
            }
            ++position;
        }

        block = block.next();
        position = 0;
    } while (block.isValid());

    // No match found in the document
    return BraceMatchResult();
}

static BraceMatchResult findPrevBrace(QTextBlock block, int position)
{
    QStack<QChar> balance;
    do {
        QString text = block.text();
        while (position > 0) {
            --position;
            const QChar ch = text.at(position);
            if (isQuote(ch)) {
                if (!balance.isEmpty() && balance.top() == ch)
                    (void) balance.pop();
                else
                    balance.push(ch);
            } else if (!balance.isEmpty() && isQuote(balance.top())) {
                /* Don't look for matching braces until we exit the quote */
            } else if (ch == ')' || ch == ']' || ch == '}') {
                balance.push(ch);
            } else if (ch == '(' || ch == '[' || ch == '{') {
                if (balance.isEmpty())
                    return BraceMatchResult();
                const QChar match = balance.pop();
                if (balance.isEmpty())
                    return BraceMatchResult(block.position() + position, braceMatch(match, ch));
            }
        }

        block = block.previous();
        position = block.text().size();
    } while (block.isValid());

    // No match found in the document
    return BraceMatchResult();
}

void SyntaxTextEdit::updateCursor()
{
    QList<QTextEdit::ExtraSelection> selections;

    if (matchBraces()) {
        QTextCursor cursor = textCursor();
        cursor.clearSelection();
        const QString blockText = cursor.block().text();
        const int blockPos = cursor.positionInBlock();
        const QChar chPrev = (blockPos > 0)
                             ? blockText[cursor.positionInBlock() - 1]
                             : QLatin1Char(0);
        const QChar chNext = (blockPos < blockText.size())
                             ? blockText[cursor.positionInBlock()]
                             : QLatin1Char(0);
        BraceMatchResult match;
        if (chNext == '(' || chNext == '[' || chNext == '{') {
            match = findNextBrace(cursor.block(), blockPos);
        } else if (chPrev == ')' || chPrev == ']' || chPrev == '}') {
            match = findPrevBrace(cursor.block(), blockPos);
            cursor.movePosition(QTextCursor::PreviousCharacter);
        }

        if (match.position >= 0) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(match.validMatch ? m_braceMatchBg : m_errorBg);
            selection.cursor = cursor;
            selection.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            selections.append(selection);
            selection.cursor = textCursor();
            selection.cursor.setPosition(match.position);
            selection.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            selections.append(selection);
        }
    }

    setExtraSelections(selections);

    // Ensure the entire viewport gets repainted to account for the
    // "current line" highlight change
    viewport()->update();

    // Also update the entire line number margin; otherwise word-wrapped lines
    // may not get the correct block updated
    m_lineMargin->update();
}

void SyntaxTextEdit::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect rect = contentsRect();
    rect.setWidth(lineMarginWidth());
    m_lineMargin->setGeometry(rect);
}

void SyntaxTextEdit::cutLines()
{
    if (!haveSelection()) {
        auto cursor = textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
    }
    cut();
}

void SyntaxTextEdit::copyLines()
{
    if (!haveSelection()) {
        auto cursor = textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
    }
    copy();
}

void SyntaxTextEdit::indentSelection()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    const int startPos = cursor.selectionStart();
    cursor.setPosition(cursor.selectionEnd());
    const int endBlock = (cursor.position() == cursor.block().position())
                         ? cursor.blockNumber() - 1 : cursor.blockNumber();
    cursor.setPosition(startPos);
    do {
        int leadingIndent = 0;
        int startOfLine = 0;
        for (const auto ch : cursor.block().text()) {
            if (ch == QLatin1Char('\t')) {
                leadingIndent += (m_tabCharSize - (leadingIndent % m_tabCharSize));
                startOfLine += 1;
            } else if (ch == QLatin1Char(' ')) {
                leadingIndent += 1;
                startOfLine += 1;
            } else {
                break;
            }
        }

        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                            startOfLine);
        cursor.removeSelectedText();

        const int indent = leadingIndent + (m_indentationMode == IndentTabs
                                            ? m_tabCharSize : m_indentWidth);
        if (m_indentationMode == IndentSpaces) {
            cursor.insertText(QString(indent, ' '));
        } else {
            const int tabs = indent / m_tabCharSize;
            const int spaces = indent % m_tabCharSize;
            cursor.insertText(QString(tabs, '\t'));
            cursor.insertText(QString(spaces, ' '));
        }

        if (!cursor.movePosition(QTextCursor::NextBlock))
            break;
    } while (cursor.blockNumber() <= endBlock);

    cursor.endEditBlock();
}

void SyntaxTextEdit::outdentSelection()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    const int startPos = cursor.selectionStart();
    cursor.setPosition(cursor.selectionEnd());
    const int endBlock = (cursor.position() == cursor.block().position())
                         ? cursor.blockNumber() - 1 : cursor.blockNumber();
    cursor.setPosition(startPos);
    do {
        int leadingIndent = 0;
        int startOfLine = 0;
        for (const auto ch : cursor.block().text()) {
            if (ch == QLatin1Char('\t')) {
                leadingIndent += (m_tabCharSize - (leadingIndent % m_tabCharSize));
                startOfLine += 1;
            } else if (ch == QLatin1Char(' ')) {
                leadingIndent += 1;
                startOfLine += 1;
            } else {
                break;
            }
        }

        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                            startOfLine);
        cursor.removeSelectedText();

        const int indent = leadingIndent - (m_indentationMode == IndentTabs
                                            ? m_tabCharSize : m_indentWidth);
        if (indent > 0) {
            if (m_indentationMode == IndentSpaces) {
                cursor.insertText(QString(indent, ' '));
            } else {
                const int tabs = indent / m_tabCharSize;
                const int spaces = indent % m_tabCharSize;
                cursor.insertText(QString(tabs, '\t'));
                cursor.insertText(QString(spaces, ' '));
            }
        }

        if (!cursor.movePosition(QTextCursor::NextBlock))
            break;
    } while (cursor.blockNumber() <= endBlock);

    cursor.endEditBlock();
}

void SyntaxTextEdit::zoomIn()
{
    QPlainTextEdit::zoomIn(1);
    updateMargins();
    updateTabMetrics();
}

void SyntaxTextEdit::zoomOut()
{
    QPlainTextEdit::zoomOut(1);
    updateMargins();
    updateTabMetrics();
}

void SyntaxTextEdit::zoomReset()
{
    setFont(defaultFont());
    updateMargins();
    updateTabMetrics();
}

void SyntaxTextEdit::keyPressEvent(QKeyEvent *e)
{
    // Ensure these are handled by the application, NOT by QPlainTextEdit's
    // built-in implementation that bypasses us altogether
    if (e->matches(QKeySequence::Undo)) {
        emit parentUndo();
        return;
    }
    if (e->matches(QKeySequence::Redo)) {
        emit parentRedo();
        return;
    }

    // Custom versions of Cut and Copy
    if (e->matches(QKeySequence::Cut)) {
        cutLines();
        return;
    }
    if (e->matches(QKeySequence::Copy)) {
        copyLines();
        return;
    }

    // "Smart" home key
    if (e->matches(QKeySequence::MoveToStartOfLine)) {
        smartHome(QTextCursor::MoveAnchor);
        return;
    }
    if (e->matches(QKeySequence::SelectStartOfLine)) {
        smartHome(QTextCursor::KeepAnchor);
        return;
    }

    switch (e->key()) {
    case Qt::Key_Tab:
        if (haveSelection()) {
            indentSelection();
        } else if (m_indentationMode == IndentTabs) {
            textCursor().insertText(QStringLiteral("\t"));
        } else if (m_indentationMode == IndentSpaces) {
            QTextCursor cursor = textCursor();
            const QString blockText = cursor.block().text();
            const QStringRef cursorText = blockText.leftRef(cursor.positionInBlock());
            int vpos = 0;
            for (const auto ch : cursorText) {
                if (ch == QLatin1Char('\t'))
                    vpos += (m_tabCharSize - (vpos % m_tabCharSize));
                else
                    vpos += 1;
            }
            const int spaces = m_indentWidth - (vpos % m_indentWidth);
            cursor.insertText(QString(spaces, ' '));
        } else {
            QTextCursor cursor = textCursor();
            const QString blockText = cursor.block().text();
            const QStringRef cursorText = blockText.leftRef(cursor.positionInBlock());
            int vpos = 0, cpos = 0;
            int wsvStart = 0, wscStart = 0;
            for (const auto ch : cursorText) {
                cpos += 1;
                if (ch == QLatin1Char('\t')) {
                    vpos += (m_tabCharSize - (vpos % m_tabCharSize));
                } else {
                    vpos += 1;
                    if (ch != QLatin1Char(' ')) {
                        wsvStart = vpos;
                        wscStart = cpos;
                    }
                }
            }

            // Fix up only the current block of whitespace up to the
            // cursor position.  This most closely matches vim's mixed
            // indentation (softtabstop+noexpandtab)
            cursor.beginEditBlock();
            cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor,
                                cpos - wscStart);
            cursor.removeSelectedText();

            const int indentTo = vpos + m_indentWidth - (vpos % m_indentWidth);
            vpos = wsvStart;
            const int alignTo = m_tabCharSize - (vpos % m_tabCharSize);
            if (vpos + alignTo <= indentTo) {
                cursor.insertText(QStringLiteral("\t"));
                vpos += alignTo;
            }
            const int tabs = (indentTo - vpos) / m_tabCharSize;
            const int spaces = (indentTo - vpos) % m_tabCharSize;
            cursor.insertText(QString(tabs, '\t'));
            cursor.insertText(QString(spaces, ' '));
            cursor.endEditBlock();
        }
        e->accept();
        break;

    case Qt::Key_Backtab:
        outdentSelection();
        e->accept();
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        {
            QTextCursor undoCursor = textCursor();
            undoCursor.beginEditBlock();

            // Don't allow QPlainTextEdit to insert a soft break :(
            QKeyEvent retnEvent(e->type(), e->key(),
                                e->modifiers() & ~Qt::ShiftModifier,
                                e->nativeScanCode(), e->nativeVirtualKey(),
                                e->nativeModifiers(), e->text(),
                                e->isAutoRepeat(), e->count());
            QPlainTextEdit::keyPressEvent(&retnEvent);

            // Simple auto-indent: Just copy the previous non-empty line's
            // leading whitespace
            if ((e->modifiers() & Qt::ShiftModifier) == 0 && autoIndent()) {
                QTextCursor scanCursor = textCursor();
                int startOfLine = 0;
                while (scanCursor.blockNumber() > 0 && startOfLine == 0) {
                    scanCursor.movePosition(QTextCursor::PreviousBlock);

                    const QString blockText = scanCursor.block().text();
                    for (const auto ch : blockText) {
                        if (ch.isSpace())
                            startOfLine += 1;
                        else
                            break;
                    }
                    if (startOfLine == 0 && !blockText.isEmpty()) {
                        // No leading whitespace, but line is not empty.
                        // Therefore, current leading indent level is 0.
                        break;
                    }
                }
                if (startOfLine != 0) {
                    const QString leadingIndent = scanCursor.block().text().left(startOfLine);
                    textCursor().insertText(leadingIndent);
                }
            }
            undoCursor.endEditBlock();
        }
        break;

    case Qt::Key_Up:
        if (e->modifiers() & Qt::ControlModifier) {
            if (e->modifiers() & Qt::ShiftModifier) {
                moveLines(QTextCursor::PreviousBlock);
            } else {
                auto scrollBar = verticalScrollBar();
                scrollBar->setValue(scrollBar->value() - 1);
            }
        } else {
            QPlainTextEdit::keyPressEvent(e);
        }
        break;

    case Qt::Key_Down:
        if (e->modifiers() & Qt::ControlModifier) {
            if (e->modifiers() & Qt::ShiftModifier) {
                moveLines(QTextCursor::NextBlock);
            } else {
                auto scrollBar = verticalScrollBar();
                scrollBar->setValue(scrollBar->value() + 1);
            }
        } else {
            QPlainTextEdit::keyPressEvent(e);
        }
        break;

    case Qt::Key_D:
        if (e->modifiers() & Qt::ControlModifier) {
            // Default of Ctrl+D is the same as "Del"; This means we can
            // repurpose it for "Delete Line"
            auto cursor = textCursor();
            cursor.beginEditBlock();
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.endEditBlock();
        } else {
            QPlainTextEdit::keyPressEvent(e);
        }
        break;

    default:
        QPlainTextEdit::keyPressEvent(e);
        break;
    }

    updateCursor();
}

void SyntaxTextEdit::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        // NOTE: This actually changes the font size
        if (e->angleDelta().y() > 0)
            zoomIn();
        else if (e->angleDelta().y() < 0)
            zoomOut();
    } else {
        QPlainTextEdit::wheelEvent(e);
    }
}

void SyntaxTextEdit::paintEvent(QPaintEvent *e)
{
    const QRect eventRect = e->rect();
    const QRect viewRect = viewport()->rect();
    QRectF cursorBlockRect;

    if (highlightCurrentLine()) {
        // Highlight current line first, so the long line marker will draw over it
        // Unlike setExtraSelections(), we paint the entire line past even the
        // document margins.
        QTextCursor cursor = textCursor();
        cursorBlockRect = blockBoundingGeometry(cursor.block());
        cursorBlockRect.translate(contentOffset());
        cursorBlockRect.setLeft(eventRect.left());
        cursorBlockRect.setWidth(eventRect.width());
        if (eventRect.intersects(cursorBlockRect.toAlignedRect())) {
            QPainter p(viewport());
            p.fillRect(cursorBlockRect, m_cursorLineBg);
        }
    }

    if (showLongLineEdge() && m_longLineMarker > 0) {
        QFontMetricsF fm(font());
        // averageCharWidth() and width('x') don't seem to give an accurate
        // enough position.  I'm sure I'm missing something, but this works
        // for now and doesn't seem to be too slow (yet).
        const qreal longLinePos = fm.width(QString(m_longLineMarker, 'x'))
                                  + contentOffset().x() + document()->documentMargin();
        if (longLinePos < viewRect.width()) {
            QPainter p(viewport());
            QRectF longLineRect(longLinePos, eventRect.top(),
                                viewRect.width() - longLinePos, eventRect.height());
            p.fillRect(longLineRect, m_longLineBg);
            if (longLineRect.intersects(cursorBlockRect))
                p.fillRect(cursorBlockRect.intersected(longLineRect), m_longLineCursorBg);
            p.setPen(m_longLineEdge);
            p.drawLine(longLinePos, eventRect.top(), longLinePos, eventRect.bottom());
        }
    }

    QPlainTextEdit::paintEvent(e);

    // Overlay indentation guides after rendering the text
    if (showIndentGuides()) {
        QPainter p(viewport());
        p.setPen(m_indentGuideFg);
        QTextBlock block = firstVisibleBlock();
        const QFontMetricsF fm(font());
        const qreal indentLine = fm.width(QString(m_indentWidth, ' '));
        const qreal lineOffset = contentOffset().x() + document()->documentMargin();
        while (block.isValid()) {
            QString blockText = block.text();
            int wsColumn = 0;
            bool onlySpaces = true;
            for (const QChar &ch : blockText) {
                if (ch == QLatin1Char('\t')) {
                    wsColumn = wsColumn - (wsColumn % m_tabCharSize) + m_tabCharSize;
                } else if (ch.isSpace()) {
                    ++wsColumn;
                } else {
                    onlySpaces = false;
                    break;
                }
            }
            if (onlySpaces) {
                // Pretend we have one more column so whitespace-only lines
                // show the indent guideline when applicable
                wsColumn += 1;
            }
            QRectF blockRect = blockBoundingGeometry(block);
            blockRect.translate(contentOffset());
            wsColumn = (wsColumn + m_indentWidth - 1) / m_indentWidth;
            for (int i = 1; i < wsColumn; ++i) {
                const qreal lineX = (indentLine * i) + lineOffset;
                p.drawLine(QPointF(lineX, blockRect.top()),
                           QPointF(lineX, blockRect.bottom()));
            }
            block = block.next();
        }
    }
}

void SyntaxTextEdit::printDocument(QPrinter *printer)
{
    // Override settings for printing
    auto displayFont = font();
    setFont(defaultFont());
    updateTabMetrics();

    auto displayTheme = m_highlighter->theme();
    auto printingTheme = syntaxRepo()->theme(QStringLiteral("Printing"));
    if (!printingTheme.isValid())
        printingTheme = syntaxRepo()->defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
    if (printingTheme.isValid())
        setTheme(printingTheme);

    auto displayOption = document()->defaultTextOption();
    auto printOption = displayOption;
    printOption.setFlags(printOption.flags() & ~QTextOption::ShowTabsAndSpaces);
    document()->setDefaultTextOption(printOption);

    auto displayWrapMode = wordWrapMode();
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    // Let the document handle its own print formatting
    print(printer);

    // Restore display settings
    setWordWrapMode(displayWrapMode);
    document()->setDefaultTextOption(displayOption);
    setTheme(displayTheme);
    setFont(displayFont);
    updateTabMetrics();
}
