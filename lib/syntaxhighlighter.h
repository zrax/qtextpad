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

#ifndef QTEXTPAD_SYNTAXHIGHLIGHTER_H
#define QTEXTPAD_SYNTAXHIGHLIGHTER_H

#include <KSyntaxHighlighting/SyntaxHighlighter>

class SyntaxHighlighter : public KSyntaxHighlighting::SyntaxHighlighter
{
public:
    explicit SyntaxHighlighter(QTextDocument *document)
        : KSyntaxHighlighting::SyntaxHighlighter(document),
          m_tabCharSize()
    { }

    void setTabWidth(int width) { m_tabCharSize = width; }
    int tabWidth() const { return m_tabCharSize; }

    static void hideBlock(QTextBlock block, bool hide);

    static bool isFolded(const QTextBlock &block)
    {
        return block.userState() > 0;
    }

    bool foldContains(const QTextBlock &foldBlock, const QTextBlock &targetBlock) const;

    void foldBlock(QTextBlock block) const;
    void unfoldBlock(QTextBlock block) const;

    int leadingIndentation(const QString &blockText, int *indentPos = nullptr) const;

    bool isFoldable(const QTextBlock &block) const;
    QTextBlock findFoldEnd(const QTextBlock &startBlock) const;

protected:
    void highlightBlock(const QString &text) Q_DECL_OVERRIDE;

private:
    int m_tabCharSize;
};

#endif // QTEXTPAD_SYNTAXHIGHLIGHTER_H
