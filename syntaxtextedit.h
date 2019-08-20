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

namespace KSyntaxHighlighting
{
    class Repository;
    class Definition;
    class Theme;
}

class WhitespaceSyntaxHighlighter;

class QPrinter;

class SyntaxTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit SyntaxTextEdit(QWidget *parent);

    bool haveSelection() const;
    void deleteSelection();

    int lineMarginWidth();
    void paintLineNumbers(QPaintEvent *e);
    void setShowLineNumbers(bool show);
    bool showLineNumbers() const;

    void setShowWhitespace(bool show);
    bool showWhitespace() const;

    void setScrollPastEndOfFile(bool scroll);
    bool scrollPastEndOfFile() const;

    void setHighlightCurrentLine(bool show);
    bool highlightCurrentLine() const;

    void setTabWidth(int width);
    int tabWidth() const { return m_tabCharSize; }
    void setIndentWidth(int width);
    int indentWidth() const { return m_indentWidth; }

    enum IndentationMode
    {
        IndentSpaces,
        IndentTabs,
        IndentMixed,
        IndentMode_MAX,
    };
    void setIndentationMode(int mode);
    IndentationMode indentationMode() const { return m_indentationMode; }

    int textColumn(const QString &block, int positionInBlock) const;
    void moveCursorTo(int line, int column = 0);

    void setAutoIndent(bool ai);
    bool autoIndent() const;

    void setShowLongLineEdge(bool show);
    bool showLongLineEdge() const;
    void setLongLineWidth(int pos);
    int longLineWidth() const { return m_longLineMarker; }
    void setShowIndentGuides(bool show);
    bool showIndentGuides() const;
    void setWordWrap(bool wrap);
    bool wordWrap() const;

    void setMatchBraces(bool match);
    bool matchBraces() const;

    static KSyntaxHighlighting::Repository *syntaxRepo();
    static const KSyntaxHighlighting::Definition &nullSyntax();

    void setDefaultFont(const QFont &font);
    void setTheme(const KSyntaxHighlighting::Theme &theme);
    void setSyntax(const KSyntaxHighlighting::Definition &syntax);

    QFont defaultFont() const;

protected:
    void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;

signals:
    void parentUndo();
    void parentRedo();

public slots:
    void indentSelection();
    void outdentSelection();
    void zoomIn();      // Hides QPlainTextEdit::zoomIn(int = 1)
    void zoomOut();     // Hides QPlainTextEdit::zoomOut(int = 1)
    void zoomReset();

    void printDocument(QPrinter *printer);

private slots:
    void updateMargins();
    void updateLineNumbers(const QRect &rect, int dy);
    void updateCursor();
    void updateTabMetrics();

private:
    QWidget *m_lineMargin;
    WhitespaceSyntaxHighlighter *m_highlighter;
    QColor m_lineMarginBg, m_lineMarginFg;
    QColor m_cursorLineBg, m_cursorLineNum;
    QColor m_longLineBg, m_longLineEdge, m_longLineCursorBg;
    QColor m_indentGuideFg;
    QColor m_braceMatchBg;
    QColor m_errorBg;
    int m_tabCharSize, m_indentWidth;
    int m_longLineMarker;
    unsigned int m_config;
    IndentationMode m_indentationMode;
    int m_originalFontSize;
};

#endif // _SYNTAXTEXTEDIT_H
