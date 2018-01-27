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

#ifndef _APPSETTINGS_H
#define _APPSETTINGS_H

#include <QSettings>

#define SIMPLE_SETTING(type, name, get, set, defaultValue) \
    type get() const { return m_settings.value(name, defaultValue).value<type>(); } \
    void set(type value) { m_settings.setValue(name, value); }

class QTextPadSettings
{
public:
    QTextPadSettings();
    QString settingsDir() const;

    QStringList recentFiles() const;
    void addRecentFile(const QString &filename);

    SIMPLE_SETTING(bool, "WordWrap", wordWrap, setWordWrap, false)
    SIMPLE_SETTING(bool, "ShowLongLineMargin", showLongLineMargin,
                   setShowLongLineMargin, false)
    SIMPLE_SETTING(int, "LongLineWidth", longLineWidth, setLongLineWidth, 80)
    SIMPLE_SETTING(bool, "IndentationGuides", indentationGuides, setIndentationGuides, false)
    SIMPLE_SETTING(bool, "LineNumbers", lineNumbers, setLineNumbers, false)
    SIMPLE_SETTING(bool, "ShowWhitespace", showWhitespace, setShowWhitespace, false)
    SIMPLE_SETTING(bool, "HighlightCurrentLine", highlightCurLine,
                   setHighlightCurLine, true)
    SIMPLE_SETTING(bool, "MatchBraces", matchBraces, setMatchBraces, true)

    SIMPLE_SETTING(int, "TabWidth", tabWidth, setTabWidth, 4)
    SIMPLE_SETTING(int, "IndentWidth", indentWidth, setIndentWidth, 4)
    // -1 or any other invalid value will trigger the setting of the default
    SIMPLE_SETTING(int, "IndentMode", indentMode, setIndentMode, -1)
    SIMPLE_SETTING(bool, "AutoIndent", autoIndent, setAutoIndent, true)

    // "Hidden" settings (for now)
    SIMPLE_SETTING(bool, "ScrollPastEndOfFile", scrollPastEndOfFile,
                   setScrollPastEndOfFile, true)

    QFont editorFont() const;
    void setEditorFont(const QFont &font);

    QString editorTheme() const { return m_settings.value("Theme").toString(); }
    void setEditorTheme(const QString &theme) { m_settings.setValue("Theme", theme); }

    QSize windowSize() const;
    void setWindowSize(const QSize &size) { m_settings.setValue("WindowSize", size); }

private:
    QSettings m_settings;
};

#endif // _APPSETTINGS_H
