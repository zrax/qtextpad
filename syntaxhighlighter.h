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

#ifndef _SYNTAXHIGHLIGHTER_H
#define _SYNTAXHIGHLIGHTER_H

#include <KSyntaxHighlighting/SyntaxHighlighter>

class SyntaxHighlighter : public KSyntaxHighlighting::SyntaxHighlighter
{
public:
    explicit SyntaxHighlighter(QTextDocument *document)
        : KSyntaxHighlighting::SyntaxHighlighter(document)
    { }

    static void hideBlock(QTextBlock block, bool hide);

    static bool isFolded(const QTextBlock &block)
    {
        return block.userState() > 0;
    }

    bool foldContains(const QTextBlock &foldBlock, const QTextBlock &targetBlock) const;

    void foldBlock(QTextBlock block) const;
    void unfoldBlock(QTextBlock block) const;

protected:
    void highlightBlock(const QString &text) Q_DECL_OVERRIDE;
};

#endif // _SYNTAXHIGHLIGHTER_H
