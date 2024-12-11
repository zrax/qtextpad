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

#ifndef QTEXTPAD_SYNTAXTEXTEDIT_H
#define QTEXTPAD_SYNTAXTEXTEDIT_H

#include <QPlainTextEdit>

namespace KSyntaxHighlighting
{
    class Repository;
    class Definition;
    class Theme;
}

class SyntaxHighlighter;

class QPrinter;

class SyntaxTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit SyntaxTextEdit(QWidget *parent = nullptr);

    void deleteSelection();
    void deleteLines();

    int lineMarginWidth() const;
    void setShowLineNumbers(bool show);
    bool showLineNumbers() const;
    void setShowFolding(bool show);
    bool showFolding() const;

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

    void moveLines(QTextCursor::MoveOperation op);
    void smartHome(QTextCursor::MoveMode mode);
    void smartEnd(QTextCursor::MoveMode mode);

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

    struct SearchParams
    {
        QString searchText;
        bool caseSensitive, wholeWord, regex;

        SearchParams() : caseSensitive(), wholeWord(), regex() { }
    };
    QTextCursor textSearch(const QTextCursor &start, const SearchParams& params,
                           bool matchFirst, bool reverse = false,
                           QRegularExpressionMatch *regexMatch = nullptr);
    void setLiveSearch(const SearchParams& params);
    void clearLiveSearch();

    void setMatchBraces(bool match);
    bool matchBraces() const;

    // When this is set, the widget will send undoRequested() and
    // redoRequested() signals instead of handling undo and redo
    // internally within the QPlainTextEdit widget.
    void setExternalUndoRedo(bool enable);
    bool externalUndoRedo() const;

    static KSyntaxHighlighting::Repository *syntaxRepo();
    static const KSyntaxHighlighting::Definition &nullSyntax();

    void setDefaultFont(const QFont &font);
    void setTheme(const KSyntaxHighlighting::Theme &theme);
    QString themeName() const;
    void setDefaultTheme();

    void setSyntax(const KSyntaxHighlighting::Definition &syntax);
    QString syntaxName() const;

    QFont defaultFont() const;

protected:
    void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;

Q_SIGNALS:
    void undoRequested();
    void redoRequested();

public Q_SLOTS:
    void cutLines();
    void copyLines();
    void indentSelection();
    void outdentSelection();

    void foldCurrentLine();
    void unfoldCurrentLine();
    void foldAll();
    void unfoldAll();

    void zoomIn();      // Hides QPlainTextEdit::zoomIn(int = 1)
    void zoomOut();     // Hides QPlainTextEdit::zoomOut(int = 1)
    void zoomReset();

    void printDocument(QPrinter *printer);

private Q_SLOTS:
    void updateMargins();
    void updateLineNumbers(const QRect &rect, int dy);
    void updateCursor();
    void updateTabMetrics();
    void updateTextMetrics();
    void updateLiveSearch();
    void updateExtraSelections();

private:
    QWidget *m_lineMargin;
    SyntaxHighlighter *m_highlighter;
    QColor m_lineMarginBg, m_lineMarginFg;
    QColor m_codeFoldingBg, m_codeFoldingFg;
    QColor m_cursorLineBg, m_cursorLineNum;
    QColor m_longLineBg, m_longLineEdge, m_longLineCursorBg;
    QColor m_indentGuideFg;
    QColor m_searchBg;
    QColor m_braceMatchBg;
    QColor m_errorBg;
    int m_tabCharSize, m_indentWidth;
    int m_longLineMarker;
    unsigned int m_config;
    IndentationMode m_indentationMode;
    int m_originalFontSize;

#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QColor m_editorBg;
    bool m_styleNeedsBgRepaint;
#endif

    QPixmap m_foldOpen, m_foldClosed;

    SearchParams m_liveSearch;
    QList<QTextEdit::ExtraSelection> m_braceMatch;
    QList<QTextEdit::ExtraSelection> m_searchResults;

    void updateScrollBars();

private:
    class LineMargin : public QWidget
    {
    public:
        explicit LineMargin(SyntaxTextEdit *editor);

        QSize sizeHint() const Q_DECL_OVERRIDE
        {
            return QSize(m_editor->lineMarginWidth(), 0);
        }

    protected:
        void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;
        void mouseMoveEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
        void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
        void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE { m_editor->wheelEvent(e); }
        void leaveEvent(QEvent *e) Q_DECL_OVERRIDE;

    private:
        SyntaxTextEdit *m_editor;
        int m_marginSelectStart;
        int m_foldHoverLine;
    };
};

#endif // QTEXTPAD_SYNTAXTEXTEDIT_H
