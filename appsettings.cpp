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
#include <QDataStream>

#define RECENT_FILES        10
#define RECENT_SEARCHES     20

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

QList<RecentFile> QTextPadSettings::recentFiles() const
{
    QList<RecentFile> recentList;
    recentList.reserve(RECENT_FILES);
    for (int i = 0; i < RECENT_FILES; ++i) {
        const auto keySuffix = QStringLiteral("_%1").arg(i, 2, 10, QLatin1Char('0'));
        const auto key = QStringLiteral("RecentFiles/Path") + keySuffix;
        if (m_settings.contains(key)) {
            RecentFile fileInfo;
            fileInfo.m_path = m_settings.value(key).toString();
            fileInfo.m_encoding = m_settings.value(QStringLiteral("RecentFiles/Encoding") + keySuffix).toString();
            fileInfo.m_line = m_settings.value(QStringLiteral("RecentFiles/Line") + keySuffix).toInt();
            recentList << fileInfo;
        }
    }
    return recentList;
}

#ifdef Q_OS_WIN
#define FILE_COMPARE_CS Qt::CaseInsensitive
#else
#define FILE_COMPARE_CS Qt::CaseSensitive
#endif

static void commitRecentFiles(QSettings &settings, const QList<RecentFile> &files)
{
    settings.beginGroup(QStringLiteral("RecentFiles"));
    for (int i = 0; i < RECENT_FILES; ++i) {
        if (i >= files.size())
            break;
        const auto &fileInfo = files.at(i);
        const auto keySuffix = QStringLiteral("_%1").arg(i, 2, 10, QLatin1Char('0'));
        settings.setValue(QStringLiteral("Path") + keySuffix, fileInfo.m_path);
        if (!fileInfo.m_encoding.isEmpty())
            settings.setValue(QStringLiteral("Encoding") + keySuffix, fileInfo.m_encoding);
        if (fileInfo.m_line > 0)
            settings.setValue(QStringLiteral("Line") + keySuffix, fileInfo.m_line);
    }
    settings.endGroup();
}

void QTextPadSettings::addRecentFile(const QString &filename, const QString &encoding,
                                     int line)
{
    auto files = recentFiles();
    const QString absFilename = QFileInfo(filename).absoluteFilePath();
    auto iter = files.begin();
    while (iter != files.end()) {
        if (iter->m_path.compare(absFilename, FILE_COMPARE_CS) == 0) {
            iter = files.erase(iter);
        } else {
            ++iter;
        }
    }
    files.prepend({absFilename, encoding, line});
    commitRecentFiles(m_settings, files);
}

void QTextPadSettings::clearRecentFiles()
{
    m_settings.beginGroup(QStringLiteral("RecentFiles"));
    for (int i = 0; i < RECENT_FILES; ++i) {
        const auto keySuffix = QStringLiteral("_%1").arg(i, 2, 10, QLatin1Char('0'));
        m_settings.remove(QStringLiteral("Path") + keySuffix);
        m_settings.remove(QStringLiteral("Encoding") + keySuffix);
        m_settings.remove(QStringLiteral("Line") + keySuffix);
    }
    m_settings.endGroup();
}

int QTextPadSettings::recentFilePosition(const QString &filename)
{
    auto files = recentFiles();
    const QString absFilename = QFileInfo(filename).absoluteFilePath();
    for (const auto &file : files) {
        if (file.m_path.compare(absFilename, FILE_COMPARE_CS) == 0)
            return file.m_line;
    }
    return 0;
}

void QTextPadSettings::setRecentFilePosition(const QString &filename, int line)
{
    auto files = recentFiles();
    const QString absFilename = QFileInfo(filename).absoluteFilePath();
    for (auto &file : files) {
        if (file.m_path.compare(absFilename, FILE_COMPARE_CS) == 0)
            file.m_line = line;
    }
    commitRecentFiles(m_settings, files);
}

QFont QTextPadSettings::editorFont() const
{
#ifdef _WIN32
    // Included in Vista or Office 2007, both of which are "Old Enough" (2018)
    static const QString defaultFontName = QStringLiteral("Consolas");
#else
    static const QString defaultFontName = QStringLiteral("Monospace");
#endif

    QFont font(m_settings.value("Editor/DefaultFont", defaultFontName).toString(),
               m_settings.value("Editor/DefaultFontSize", 10).toInt(),
               m_settings.value("Editor/DefaultFontWeight", QFont::Normal).toInt(),
               m_settings.value("Editor/DefaultFontItalic", false).toBool());
    font.setFixedPitch(true);
    return font;
}

void QTextPadSettings::setEditorFont(const QFont &font)
{
    m_settings.setValue("Editor/DefaultFont", font.family());
    m_settings.setValue("Editor/DefaultFontSize", font.pointSize());
    m_settings.setValue("Editor/DefaultFontWeight", font.weight());
    m_settings.setValue("Editor/DefaultFontItalic", font.italic());
}

QSize QTextPadSettings::windowSize() const
{
    return m_settings.value("WindowSize", QSize(600, 400)).toSize();
}

QStringList QTextPadSettings::recentSearches() const
{
    return m_settings.value("Search/Recent", QStringList{}).toStringList();
}

void QTextPadSettings::addRecentSearch(const QString &text)
{
    QStringList searches = recentSearches();
    auto iter = searches.begin();
    while (iter != searches.end()) {
        if (iter->compare(text) == 0) {
            iter = searches.erase(iter);
        } else {
            ++iter;
        }
    }
    searches.prepend(text);
    while (searches.size() > RECENT_SEARCHES)
        searches.removeLast();
    m_settings.setValue("Search/Recent", searches);
}

QStringList QTextPadSettings::recentSearchReplacements() const
{
    return m_settings.value("Search/RecentReplace", QStringList{}).toStringList();
}

void QTextPadSettings::addRecentSearchReplacement(const QString &text)
{
    QStringList replacements = recentSearchReplacements();
    auto iter = replacements.begin();
    while (iter != replacements.end()) {
        if (iter->compare(text) == 0) {
            iter = replacements.erase(iter);
        } else {
            ++iter;
        }
    }
    replacements.prepend(text);
    while (replacements.size() > RECENT_SEARCHES)
        replacements.removeLast();
    m_settings.setValue("Search/RecentReplace", replacements);
}
