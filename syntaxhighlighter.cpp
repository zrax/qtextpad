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
    if (!startsFoldingRegion(foldBlock))
        return false;
    return (targetBlock.position() >= foldBlock.position())
        && (findFoldingRegionEnd(foldBlock).position() >= targetBlock.position());
}

void SyntaxHighlighter::foldBlock(QTextBlock block) const
{
    block.setUserState(1);

    const QTextBlock endBlock = findFoldingRegionEnd(block);
    block = block.next();
    while (block.isValid() && block != endBlock) {
        hideBlock(block, true);
        block = block.next();
    }

    // Only hide the last block if it doesn't also start a new fold region
    if (block.isValid() && !startsFoldingRegion(block))
        hideBlock(block, true);
}

void SyntaxHighlighter::unfoldBlock(QTextBlock block) const
{
    block.setUserState(-1);

    const QTextBlock endBlock = findFoldingRegionEnd(block);
    block = block.next();
    while (block.isValid() && block != endBlock) {
        hideBlock(block, false);
        if (isFolded(block)) {
            block = findFoldingRegionEnd(block);
            if (block.isValid() && !startsFoldingRegion(block))
                block = block.next();
        } else {
            block = block.next();
        }
    }

    if (block.isValid() && !startsFoldingRegion(block))
        hideBlock(block, false);
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
