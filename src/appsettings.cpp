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
#include <QDir>
#include <QLockFile>
#include <QPalette>
#include <QIcon>

#define RECENT_FILES        10
#define RECENT_SEARCHES     20
#define FM_CACHE_SIZE       50

#ifdef Q_OS_WIN
#define FILE_COMPARE_CS Qt::CaseInsensitive
#else
#define FILE_COMPARE_CS Qt::CaseSensitive
#endif

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
    QStringList recentList;
    recentList.reserve(RECENT_FILES);
    for (int i = 0; i < RECENT_FILES; ++i) {
        const auto key = QStringLiteral("RecentFiles/Path_%1").arg(i, 2, 10, QLatin1Char('0'));
        if (m_settings.contains(key))
            recentList << m_settings.value(key).toString();
    }
    return recentList;
}

void QTextPadSettings::addRecentFile(const QString &filename)
{
    auto files = recentFiles();
    const QString absFilename = QFileInfo(filename).absoluteFilePath();
    auto iter = files.begin();
    while (iter != files.end()) {
        if (iter->compare(absFilename, FILE_COMPARE_CS) == 0) {
            iter = files.erase(iter);
        } else {
            ++iter;
        }
    }
    files.prepend(absFilename);

    for (int i = 0; i < RECENT_FILES; ++i) {
        if (i >= files.size())
            break;
        const auto &filePath = files.at(i);
        const auto keySuffix = QStringLiteral("_%1").arg(i, 2, 10, QLatin1Char('0'));
        m_settings.setValue(QStringLiteral("RecentFiles/Path") + keySuffix, filePath);

        // Clean up settings from older versions of qtextpad
        m_settings.remove(QStringLiteral("RecentFiles/Encoding") + keySuffix);
        m_settings.remove(QStringLiteral("RecentFiles/Line") + keySuffix);
    }
}

void QTextPadSettings::clearRecentFiles()
{
    for (int i = 0; i < RECENT_FILES; ++i) {
        const auto keySuffix = QStringLiteral("_%1").arg(i, 2, 10, QLatin1Char('0'));
        m_settings.remove(QStringLiteral("RecentFiles/Path") + keySuffix);

        // Also clean up settings from older versions of qtextpad
        m_settings.remove(QStringLiteral("RecentFiles/Encoding") + keySuffix);
        m_settings.remove(QStringLiteral("RecentFiles/Line") + keySuffix);
    }
}

static QString fmCacheFileName()
{
    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!cacheDir.exists()) {
        if (!cacheDir.mkpath(QStringLiteral(".")))
            qWarning("Could not create cache directory %s.", qPrintable(cacheDir.absolutePath()));
    }

    return cacheDir.absoluteFilePath(QStringLiteral("fmcache.list"));
}

static QByteArray fmEncode(QString value)
{
    return value.replace(QLatin1Char('%'), QLatin1String("%25"))
            .replace(QLatin1Char(':'), QLatin1String("%3A")).toUtf8();
}

static QString fmDecode(const QByteArray &value)
{
    return QString::fromUtf8(value)
            .replace(QLatin1String("%3A"), QLatin1String(":"))
            .replace(QLatin1String("%25"), QLatin1String("%"));
}

FileModes QTextPadSettings::fileModes(const QString &filename)
{
    const QString absFilename = QFileInfo(filename).absoluteFilePath();

    QFile cacheFile(fmCacheFileName());
    if (cacheFile.open(QIODevice::ReadOnly)) {
        for ( ;; ) {
            QByteArray line = cacheFile.readLine();
            if (line.isEmpty())
                break;
            const auto parts = line.trimmed().split(':');

            if (fmDecode(parts.first()).compare(absFilename, FILE_COMPARE_CS) == 0) {
                FileModes modes;
                if (parts.size() > 1)
                    modes.encoding = fmDecode(parts[1]);
                if (parts.size() > 2)
                    modes.syntax = fmDecode(parts[2]);
                modes.lineNum = (parts.size() > 3) ? parts[3].toInt() : 0;
                return modes;
            }
        }
    }

    return { QString(), QString(), 0 };
}

void QTextPadSettings::setFileModes(const QString &filename, const QString &encoding,
                                    const QString &syntax, int lineNum)
{
    const QString absFilename = QFileInfo(filename).absoluteFilePath();

    QLockFile lockFile(fmCacheFileName() + QStringLiteral(".lock"));
    if (!lockFile.lock())
        qWarning("Could not acquire lock for %s.", qPrintable(fmCacheFileName()));

    QFile cacheFile(fmCacheFileName());
    QList<QByteArray> lines;
    lines << (fmEncode(absFilename) + ':' + fmEncode(encoding) + ':'
              + fmEncode(syntax) + ':' + QByteArray::number(lineNum)) + '\n';
    if (cacheFile.open(QIODevice::ReadOnly)) {
        while (lines.size() < FM_CACHE_SIZE) {
            const QByteArray line = cacheFile.readLine();
            if (line.isEmpty())
                break;

            const auto parts = line.split(':');
            if (fmDecode(parts.first()).compare(absFilename, FILE_COMPARE_CS) == 0)
                continue;
            lines << line;
        }
        cacheFile.close();
    }

    if (!cacheFile.open(QIODevice::WriteOnly)) {
        qWarning("Could not open %s for writing.", qPrintable(fmCacheFileName()));
        return;
    }
    for (const auto &line : lines)
        cacheFile.write(line);
    cacheFile.close();
}

QFont QTextPadSettings::editorFont() const
{
#if defined(_WIN32)
    // Included in Vista or Office 2007, both of which are "Old Enough" (2018)
    static const QString defaultFontName = QStringLiteral("Consolas");
    static const int defaultFontSize = 10;
#elif defined(__APPLE__)
    static const QString defaultFontName = QStringLiteral("Menlo");
    static const int defaultFontSize = 12;
#else
    static const QString defaultFontName = QStringLiteral("Monospace");
    static const int defaultFontSize = 10;
#endif

    QFont font(m_settings.value(QStringLiteral("Editor/DefaultFont"), defaultFontName).toString(),
               m_settings.value(QStringLiteral("Editor/DefaultFontSize"), defaultFontSize).toInt(),
               m_settings.value(QStringLiteral("Editor/DefaultFontWeight"), QFont::Normal).toInt(),
               m_settings.value(QStringLiteral("Editor/DefaultFontItalic"), false).toBool());
    font.setFixedPitch(true);
    return font;
}

void QTextPadSettings::setEditorFont(const QFont &font)
{
    m_settings.setValue(QStringLiteral("Editor/DefaultFont"), font.family());
    m_settings.setValue(QStringLiteral("Editor/DefaultFontSize"), font.pointSize());
    m_settings.setValue(QStringLiteral("Editor/DefaultFontWeight"), font.weight());
    m_settings.setValue(QStringLiteral("Editor/DefaultFontItalic"), font.italic());
}

QSize QTextPadSettings::windowSize() const
{
    return m_settings.value(QStringLiteral("WindowSize"), QSize(600, 400)).toSize();
}

QStringList QTextPadSettings::recentSearches() const
{
    return m_settings.value(QStringLiteral("Search/Recent"),
                            QStringList{}).toStringList();
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
    m_settings.setValue(QStringLiteral("Search/Recent"), searches);
}

QStringList QTextPadSettings::recentSearchReplacements() const
{
    return m_settings.value(QStringLiteral("Search/RecentReplace"),
                            QStringList{}).toStringList();
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
    m_settings.setValue(QStringLiteral("Search/RecentReplace"), replacements);
}

QIcon QTextPadSettings::staticIcon(const QString &iconName, bool darkTheme)
{
    const QString iconPattern = darkTheme
            ? QStringLiteral(":/icons/qtextpad-dark/16x16/actions/%1.png")
            : QStringLiteral(":/icons/qtextpad/16x16/actions/%1.png");
    return QIcon(iconPattern.arg(iconName));
}
