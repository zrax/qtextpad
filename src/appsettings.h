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

#ifndef QTEXTPAD_APPSETTINGS_H
#define QTEXTPAD_APPSETTINGS_H

#include <QSettings>
#include <QSize>

#define SIMPLE_SETTING(type, name, get, set, defaultValue) \
    type get() const { return m_settings.value(QStringLiteral(name), defaultValue).value<type>(); } \
    void set(type value) { m_settings.setValue(QStringLiteral(name), value); }

struct FileModes
{
    QString encoding;
    QString syntax;
    int lineNum;
};

class QTextPadSettings
{
public:
    QTextPadSettings();
    QString settingsDir() const;

    QStringList recentFiles() const;
    void addRecentFile(const QString &filename);
    void clearRecentFiles();

    static FileModes fileModes(const QString &filename);
    static void setFileModes(const QString &filename, const QString &encoding,
                             const QString &syntax, int lineNum);

    SIMPLE_SETTING(bool, "ShowToolBar", showToolBar, setShowToolBar, true)
    SIMPLE_SETTING(bool, "ShowStatusBar", showStatusBar, setShowStatusBar, true)
    SIMPLE_SETTING(bool, "ShowFilePath", showFilePath, setShowFilePath, false)

    // Editor settings
    SIMPLE_SETTING(bool, "Editor/WordWrap", wordWrap, setWordWrap, false)
    SIMPLE_SETTING(bool, "Editor/ShowLongLineMargin", showLongLineMargin,
                   setShowLongLineMargin, false)
    SIMPLE_SETTING(int, "Editor/LongLineWidth", longLineWidth, setLongLineWidth, 80)
    SIMPLE_SETTING(bool, "Editor/IndentationGuides", indentationGuides,
                   setIndentationGuides, false)
    SIMPLE_SETTING(bool, "Editor/LineNumbers", lineNumbers, setLineNumbers, false)
    SIMPLE_SETTING(bool, "Editor/ShowFolding", showFolding, setShowFolding, false)
    SIMPLE_SETTING(bool, "Editor/ShowWhitespace", showWhitespace, setShowWhitespace, false)
    SIMPLE_SETTING(bool, "Editor/HighlightCurrentLine", highlightCurLine,
                   setHighlightCurLine, true)
    SIMPLE_SETTING(bool, "Editor/MatchBraces", matchBraces, setMatchBraces, true)

    SIMPLE_SETTING(int, "Editor/TabWidth", tabWidth, setTabWidth, 4)
    SIMPLE_SETTING(int, "Editor/IndentWidth", indentWidth, setIndentWidth, 4)
    // -1 or any other invalid value will trigger the setting of the default
    SIMPLE_SETTING(int, "Editor/IndentMode", indentMode, setIndentMode, -1)
    SIMPLE_SETTING(bool, "Editor/AutoIndent", autoIndent, setAutoIndent, true)

    SIMPLE_SETTING(bool, "Editor/ScrollPastEndOfFile", scrollPastEndOfFile,
                   setScrollPastEndOfFile, false)

    QFont editorFont() const;
    void setEditorFont(const QFont &font);

    QString editorTheme() const
    {
        return m_settings.value(QStringLiteral("Editor/Theme")).toString();
    }
    void setEditorTheme(const QString &theme)
    {
        m_settings.setValue(QStringLiteral("Editor/Theme"), theme);
    }
    void clearEditorTheme()
    {
        m_settings.remove(QStringLiteral("Editor/Theme"));
    }

    QSize windowSize() const;
    void setWindowSize(const QSize &size)
    {
        m_settings.setValue(QStringLiteral("WindowSize"), size);
    }

    // Search dialog options
    QStringList recentSearches() const;
    void addRecentSearch(const QString &text);

    QStringList recentSearchReplacements() const;
    void addRecentSearchReplacement(const QString &text);

    SIMPLE_SETTING(bool, "Search/CaseSensitive", searchCaseSensitive,
                   setSearchCaseSensitive, false)
    SIMPLE_SETTING(bool, "Search/WholeWord", searchWholeWord,
                   setSearchWholeWord, false)
    SIMPLE_SETTING(bool, "Search/Regex", searchRegex, setSearchRegex, false)
    SIMPLE_SETTING(bool, "Search/Escapes", searchEscapes, setSearchEscapes, false)
    SIMPLE_SETTING(bool, "Search/Wrap", searchWrap, setSearchWrap, true)

    static QIcon staticIcon(const QString &iconName, const QPalette &palette);

private:
    QSettings m_settings;
};

/* Helper for loading theme icons */
#define ICON(name)      QIcon::fromTheme(QStringLiteral(name))

#endif // QTEXTPAD_APPSETTINGS_H
