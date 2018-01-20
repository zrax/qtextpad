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

#ifndef _SYNTAXTEXTEDIT_H
#define _SYNTAXTEXTEDIT_H

#include <QPlainTextEdit>
#include <SyntaxHighlighter>

namespace KSyntaxHighlighting
{
    class Theme;
    class Definition;
    class Repository;
}

class WhitespaceSyntaxHighlighter;

class SyntaxTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit SyntaxTextEdit(QWidget *parent);

    bool haveSelection() const;
    void deleteSelection();

    int lineMarginWidth();
    void paintLineNumbers(QPaintEvent *e);
    bool showLineNumbers() const;
    void setShowLineNumbers(bool show);

    void setTabWidth(int width);
    int tabWidth() const { return m_tabCharSize; }
    void setExpandTabs(bool expand);
    bool expandTabs() const;

    int textColumn(const QString &block, int positionInBlock) const;

    void setAutoIndent(bool ai);
    bool autoIndent() const;

    void setLongLineMarker(int pos);
    int longLineMarker() const { return m_longLineMarker; }

    void setMatchParentheses(bool match);
    bool matchParentheses() const;

    static KSyntaxHighlighting::Repository *syntaxRepo();
    static const KSyntaxHighlighting::Definition &nullSyntax();

    void setTheme(const KSyntaxHighlighting::Theme &theme);
    void setSyntax(const KSyntaxHighlighting::Definition &syntax);

protected:
    void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;

public slots:
    void indentSelection();
    void outdentSelection();

private slots:
    void updateMargins();
    void updateLineNumbers(const QRect &rect, int dy);
    void updateCursor();

private:
    QWidget *m_lineMargin;
    WhitespaceSyntaxHighlighter *m_highlighter;
    QColor m_lineMarginBg, m_lineMarginFg;
    QColor m_cursorLineBg, m_cursorLineNum;
    QColor m_longLineBg, m_longLineEdge, m_longLineCursorBg;
    QColor m_parenMatchBg;
    QColor m_errorBg;
    int m_tabCharSize;
    int m_longLineMarker;
    unsigned int m_config;
};

#endif // _SYNTAXTEXTEDIT_H
