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

#include "appsettings.h"

#include <QStandardPaths>
#include <QFileInfo>
#include <QFont>
#include <QSize>

QTextPadSettings::QTextPadSettings()
    : m_settings(QSettings::IniFormat, QSettings::UserScope,
                 QStringLiteral("QTextPad"), QStringLiteral("qtextpad"))
{
    // Just use our config file, no "fancy" overrides.  This keeps things
    // simple for copying and nuking the QTextPad configuration
    m_settings.setFallbacksEnabled(false);
}

QString QTextPadSettings::settingsDir() const
{
    QFileInfo settingsFile(m_settings.fileName());
    return settingsFile.absolutePath();
}

QStringList QTextPadSettings::recentFiles() const
{
    return m_settings.value("RecentFiles", QStringList{}).toStringList();
}

void QTextPadSettings::addRecentFile(const QString &filename)
{
    QStringList files = recentFiles();
    files.append(filename);
    while (files.size() > 10)
        files.removeFirst();
    m_settings.setValue("RecentFiles", files);
}

QFont QTextPadSettings::editorFont() const
{
#ifdef _WIN32
    // Included in Vista or Office 2007, both of which are "Old Enough" (2018)
    static const QString defaultFontName = QStringLiteral("Consolas");
#else
    static const QString defaultFontName = QStringLiteral("Monospace");
#endif

    QFont font(m_settings.value("DefaultFont", defaultFontName).toString(),
               m_settings.value("DefaultFontSize", 10).toInt(),
               m_settings.value("DefaultFontWeight", QFont::Normal).toInt(),
               m_settings.value("DefaultFontItalic", false).toBool());
    font.setFixedPitch(true);
    return font;
}

void QTextPadSettings::setEditorFont(const QFont &font)
{
    m_settings.setValue("DefaultFont", font.family());
    m_settings.setValue("DefaultFontSize", font.pointSize());
    m_settings.setValue("DefaultFontWeight", font.weight());
    m_settings.setValue("DefaultFontItalic", font.italic());
}

QSize QTextPadSettings::windowSize() const
{
    return m_settings.value("WindowSize", QSize(600, 400)).toSize();
}
