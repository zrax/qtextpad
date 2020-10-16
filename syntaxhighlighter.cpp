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

#include "syntaxhighlighter.h"

#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/Definition>

#include <QRegularExpression>

void SyntaxHighlighter::hideBlock(QTextBlock block, bool hide)
{
    block.setVisible(!hide);
    block.clearLayout();
    block.setLineCount(hide ? 0 : 1);
}

bool SyntaxHighlighter::foldContains(const QTextBlock &foldBlock,
                                     const QTextBlock &targetBlock) const
{
    if (!isFoldable(foldBlock))
        return false;
    return (targetBlock.position() >= foldBlock.position())
        && (findFoldEnd(foldBlock).position() >= targetBlock.position());
}

void SyntaxHighlighter::foldBlock(QTextBlock block) const
{
    block.setUserState(1);

    const QTextBlock endBlock = findFoldEnd(block);
    block = block.next();
    while (block.isValid() && block != endBlock) {
        hideBlock(block, true);
        block = block.next();
    }

    // Only hide the last block if it doesn't also start a new fold region
    if (block.isValid() && !isFoldable(block))
        hideBlock(block, true);
}

void SyntaxHighlighter::unfoldBlock(QTextBlock block) const
{
    block.setUserState(-1);

    const QTextBlock endBlock = findFoldEnd(block);
    block = block.next();
    while (block.isValid() && block != endBlock) {
        hideBlock(block, false);
        if (isFolded(block)) {
            block = findFoldEnd(block);
            if (block.isValid() && !isFoldable(block))
                block = block.next();
        } else {
            block = block.next();
        }
    }

    if (block.isValid() && !isFoldable(block))
        hideBlock(block, false);
}

int SyntaxHighlighter::leadingIndentation(const QString &blockText, int *indentPos) const
{
    int leadingIndent = 0;
    int startOfLine = 0;
    for (const auto ch : blockText) {
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
    if (indentPos)
        *indentPos = startOfLine;
    return leadingIndent;
}

static QList<QRegularExpression> reCompileAll(const QStringList &regexList)
{
    QList<QRegularExpression> compiled;
    compiled.reserve(regexList.size());
    for (const QString &expr : regexList)
        compiled << QRegularExpression(QStringLiteral("^") + expr + QStringLiteral("$"));
    return compiled;
}

static bool lineEmpty(const QString &text, const QList<QRegularExpression> &regexList)
{
    if (text.isEmpty())
        return true;

    return std::any_of(regexList.begin(), regexList.end(), [text](const QRegularExpression &re) {
        const QRegularExpressionMatch m = re.match(text);
        return m.hasMatch();
    });
}

bool SyntaxHighlighter::isFoldable(const QTextBlock &block) const
{
    if (startsFoldingRegion(block))
        return true;
    if (definition().indentationBasedFoldingEnabled()) {
        const auto emptyList = reCompileAll(definition().foldingIgnoreList());
        if (lineEmpty(block.text(), emptyList))
            return false;

        const int curIndent = leadingIndentation(block.text());
        QTextBlock nextBlock = block.next();
        while (nextBlock.isValid() && lineEmpty(nextBlock.text(), emptyList))
            nextBlock = nextBlock.next();
        if (nextBlock.isValid() && leadingIndentation(nextBlock.text()) > curIndent)
            return true;
    }
    return false;
}

QTextBlock SyntaxHighlighter::findFoldEnd(const QTextBlock &startBlock) const
{
    if (startsFoldingRegion(startBlock))
        return findFoldingRegionEnd(startBlock);

    if (definition().indentationBasedFoldingEnabled()) {
        const auto emptyList = reCompileAll(definition().foldingIgnoreList());

        const int curIndent = leadingIndentation(startBlock.text());
        QTextBlock block = startBlock.next();
        QTextBlock endBlock;
        for ( ;; ) {
            while (block.isValid() && lineEmpty(block.text(), emptyList))
                block = block.next();
            if (!block.isValid() || leadingIndentation(block.text()) <= curIndent)
                break;
            endBlock = block;
            block = block.next();
        }
        return endBlock;
    }
    return QTextBlock();
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    KSyntaxHighlighting::SyntaxHighlighter::highlightBlock(text);

    static const QRegularExpression ws_regex(QStringLiteral("\\s+"));
    auto iter = ws_regex.globalMatch(text);
    QTextCharFormat ws_format;
    const QBrush ws_brush(theme().editorColor(KSyntaxHighlighting::Theme::TabMarker));
    ws_format.setForeground(ws_brush);
    while (iter.hasNext()) {
        const auto match = iter.next();
        setFormat(match.capturedStart(), match.capturedLength(), ws_format);
    }
}
