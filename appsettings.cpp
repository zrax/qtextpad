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

QDataStream &operator<<(QDataStream &out, const RecentFile &file)
{
    out << file.m_path
        << file.m_encoding
        << file.m_line;
    return out;
}

QDataStream &operator>>(QDataStream &in, RecentFile &file)
{
    in >> file.m_path;
    in >> file.m_encoding;
    in >> file.m_line;
    return in;
}

struct RegisterMetaType_RecentFile
{
    RegisterMetaType_RecentFile()
    {
        qRegisterMetaTypeStreamOperators<RecentFile>("RecentFile");
    }
};
static RegisterMetaType_RecentFile _autoreg_RecentFile;

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
        auto key = QStringLiteral("RecentFile_%1").arg(i, 2, 10, QLatin1Char('0'));
        if (m_settings.contains(key))
            recentList << m_settings.value(key).value<RecentFile>();
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
    for (int i = 0; i < RECENT_FILES; ++i) {
        if (i >= files.size())
            break;
        auto key = QStringLiteral("RecentFile_%1").arg(i, 2, 10, QLatin1Char('0'));
        settings.setValue(key, QVariant::fromValue(files.at(i)));
    }
}

void QTextPadSettings::addRecentFile(const QString &filename, const QString &encoding,
                                     unsigned int line)
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
    for (int i = 0; i < RECENT_FILES; ++i) {
        auto key = QStringLiteral("RecentFile_%1").arg(i, 2, 10, QLatin1Char('0'));
        m_settings.remove(key);
    }
}

unsigned int QTextPadSettings::recentFilePosition(const QString &filename)
{
    auto files = recentFiles();
    const QString absFilename = QFileInfo(filename).absoluteFilePath();
    for (const auto &file : files) {
        if (file.m_path.compare(absFilename, FILE_COMPARE_CS) == 0)
            return file.m_line;
    }
    return 0;
}

void QTextPadSettings::setRecentFilePosition(const QString &filename, unsigned int line)
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
