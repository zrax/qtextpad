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

#include <QTranslator>
#include <QLibraryInfo>
#include <QCommandLineParser>
#include <QJsonObject>
#include <QFileInfo>
#include <QIcon>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#include <QStyle>
#endif

#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/DefinitionDownloader>

#include "singleapp.h"
#include "qtextpadwindow.h"
#include "syntaxtextedit.h"
#include "appversion.h"

struct StartupFileInfo
{
    QString filename;
    int line = -1;
    int column = -1;
};

// Determine if the default icon theme includes the necessary icons for
// our toolbar.  If not, we need to use our own theme.
static bool checkDefaultIconTheme()
{
    const QString iconNames[] = {
        QStringLiteral("document-new"), QStringLiteral("document-open"),
        QStringLiteral("document-save"),
        QStringLiteral("edit-undo"), QStringLiteral("edit-redo"),
        QStringLiteral("edit-cut"), QStringLiteral("edit-copy"),
        QStringLiteral("edit-paste"),
        QStringLiteral("edit-find"), QStringLiteral("edit-find-replace"),
    };
    return std::all_of(std::begin(iconNames), std::end(iconNames),
                       qOverload<const QString&>(&QIcon::hasThemeIcon));
}

static void setDefaultIconTheme()
{
    static const bool defaultThemeOk = checkDefaultIconTheme();
    if (!defaultThemeOk) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        const bool darkTheme = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
        const bool darkTheme = (QGuiApplication::palette().color(QPalette::Window).lightness() < 128);
#endif
        QIcon::setThemeName(darkTheme ? QStringLiteral("qtextpad-dark")
                                      : QStringLiteral("qtextpad"));
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("qtextpad"));
    QCoreApplication::setApplicationVersion(QTextPadVersion::versionString());
    QTextpadSingleApp app(argc, argv);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
# ifdef Q_OS_WIN
    const QString defaultStyle = QApplication::style()->name();
    if (defaultStyle != QStringLiteral("windows11")) {
        if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)
            QApplication::setStyle(QStringLiteral("fusion"));

        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                         [defaultStyle](Qt::ColorScheme colorScheme) {
            if (colorScheme == Qt::ColorScheme::Dark)
                QApplication::setStyle(QStringLiteral("fusion"));
            else
                QApplication::setStyle(defaultStyle);
            setDefaultIconTheme();
        });
    } else {
# endif
        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                         [](Qt::ColorScheme) { setDefaultIconTheme(); });
# ifdef Q_OS_WIN
    }
# endif
#endif

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)
#else
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath)
#endif
                          ))
    {
        QCoreApplication::installTranslator(&qtTranslator);
    }

    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("qtextpad"), QStringLiteral("_")))
        QCoreApplication::installTranslator(&appTranslator);

    QCommandLineParser parser;
    parser.setApplicationDescription(
            QCoreApplication::translate("main", "qtextpad - The lightweight Qt code and text editor"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("filename, line, column"),
            QCoreApplication::translate("main", "Document(s) to open at startup, with an optional "
                                                "line and column to move the cursor to"),
            QCoreApplication::translate("main", "[filename [+line[,column]]]..."));

    const QCommandLineOption encodingOption(QStringList{QStringLiteral("e"), QStringLiteral("encoding")},
            QCoreApplication::translate("main", "Specify the encoding of the file (default: detect)"),
            QCoreApplication::translate("main", "encoding"));
    const QCommandLineOption syntaxOption(QStringList{QStringLiteral("S"), QStringLiteral("syntax")},
            QCoreApplication::translate("main", "Specify the syntax definition to use for the file (default: detect)"),
            QCoreApplication::translate("main", "syntax"));
    parser.addOption(encodingOption);
    parser.addOption(syntaxOption);

    const QCommandLineOption updateOption(QStringList{QStringLiteral("update-definitions")},
            QCoreApplication::translate("main", "Download updated syntax definitions from the internet and exit."));
    parser.addOption(updateOption);

    parser.process(app);

    if (parser.isSet(updateOption)) {
        // Handle this before any GUI objects are created
        KSyntaxHighlighting::Repository syntaxRepo;
        KSyntaxHighlighting::DefinitionDownloader downloader(&syntaxRepo);
        QObject::connect(&downloader, &KSyntaxHighlighting::DefinitionDownloader::informationMessage,
                         [](const QString &msg) {
            printf("%s\n", qPrintable(msg));
        });
        QObject::connect(&downloader, &KSyntaxHighlighting::DefinitionDownloader::done,
                         [] { QCoreApplication::exit(0); });
        downloader.start();
        return QApplication::exec();
    }

    QList<StartupFileInfo> startupFiles;
    const QStringList positionalArguments = parser.positionalArguments();
    for (const auto &arg : positionalArguments) {
        if (arg.startsWith(QLatin1Char('+'))) {
            if (startupFiles.isEmpty()) {
                qWarning("%s", qPrintable(
                    QCoreApplication::translate("main", "Missing filename for startup line parameter '%1'").arg(arg)));
                continue;
            }

            bool ok;
            QList<QStringView> parts = QStringView(arg).split(QLatin1Char(','));
            const int startupLine = parts.at(0).mid(1).toInt(&ok, 0);
            if (ok) {
                startupFiles.last().line = startupLine;
            } else {
                qWarning("%s", qPrintable(
                    QCoreApplication::translate("main", "Invalid startup line parameter: '%1'").arg(arg)));
            }
            if (parts.size() > 1) {
                const int startupCol = parts.at(1).toInt(&ok, 0);
                if (ok) {
                    startupFiles.last().column = startupCol;
                } else {
                    qWarning("%s", qPrintable(
                        QCoreApplication::translate("main", "Invalid startup line parameter: '%1'").arg(arg)));
                }
            }
        } else {
            startupFiles.append(StartupFileInfo { arg, -1, -1 });
        }
    }

    if (!app.startIpcServer()) {
        // Attempt to forward the request to a remote instance.  Only proceeds
        // to handle the startup locally if the IPC request fails.
        if (startupFiles.isEmpty()) {
            QJsonObject request;
            request.insert(QStringLiteral("cmd"), QStringLiteral("new-window"));
            if (app.sendMessage(request))
                return 0;
        } else {
            bool sendOk = true;
            for (const auto &file : startupFiles) {
                QJsonObject request;
                request.insert(QStringLiteral("cmd"), QStringLiteral("open-file"));
                request.insert(QStringLiteral("path"), QFileInfo(file.filename).absoluteFilePath());
                request.insert(QStringLiteral("line"), file.line);
                request.insert(QStringLiteral("column"), file.column);
                if (parser.isSet(encodingOption))
                    request.insert(QStringLiteral("textEncoding"), parser.value(encodingOption));
                if (parser.isSet(syntaxOption))
                    request.insert(QStringLiteral("syntaxDef"), parser.value(syntaxOption));
                if (!app.sendMessage(request)) {
                    sendOk = false;
                    break;
                }
            }
            if (sendOk)
                return 0;
        }

        // Proceed with doing things locally
        qDebug("Failed to send message to remote instance");
    }

    // NOTE: These currently affect all files specified on the command line when set.
    QString textEncoding;
    if (parser.isSet(encodingOption))
        textEncoding = parser.value(encodingOption);
    KSyntaxHighlighting::Definition syntaxDef;
    if (parser.isSet(syntaxOption)) {
        auto syntaxRepo = SyntaxTextEdit::syntaxRepo();
        syntaxDef = syntaxRepo->definitionForName(parser.value(syntaxOption));
        if (!syntaxDef.isValid()) {
            qDebug("%s", qPrintable(
                QCoreApplication::translate("main", "Invalid syntax definition specified: '%1'")
                        .arg(parser.value(syntaxOption))));
        }
    }

    QObject::connect(&app, &QTextpadSingleApp::messageReceived, [](const QJsonObject &msg) {
        const QString cmd = msg.value(QStringLiteral("cmd")).toString();
        if (cmd == QStringLiteral("new-window")) {
            (void) QTextPadWindow::createNewWindow();
        } else {
            qDebug("IPC got unknown command: %s", qPrintable(cmd));
        }
    });

    setDefaultIconTheme();

    // TODO: Make a unique icon for QTextPad?
    // This one is borrowed from Oxygen
    QIcon appIcon;
    appIcon.addFile(QStringLiteral(":/icons/qtextpad-64.png"), QSize(64, 64));
    appIcon.addFile(QStringLiteral(":/icons/qtextpad-48.png"), QSize(48, 48));
    appIcon.addFile(QStringLiteral(":/icons/qtextpad-32.png"), QSize(32, 32));
    appIcon.addFile(QStringLiteral(":/icons/qtextpad-16.png"), QSize(16, 16));
    appIcon.addFile(QStringLiteral(":/icons/qtextpad-128.png"), QSize(128, 128));
    QApplication::setWindowIcon(appIcon);

    auto win = QTextPadWindow::createNewWindow();
    for (const auto &startupFile : startupFiles) {
        if (win->openDocument(startupFile.filename, textEncoding)) {
            if (startupFile.line > 0)
                win->gotoLine(startupFile.line, startupFile.column);
            if (syntaxDef.isValid())
                win->setSyntax(syntaxDef);
        }
    }

    return QApplication::exec();
}
