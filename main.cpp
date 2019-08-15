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

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>

#include "qtextpadwindow.h"
#include "syntaxtextedit.h"
#include "appsettings.h"

// Determine if the default icon theme includes the necessary icons for
// our toolbar.  If not, we need to use our own theme.
static void setDefaultIconTheme()
{
    const QString iconNames[] = {
        "document-new", "document-open", "document-save",
        "edit-undo", "edit-redo",
        "edit-cut", "edit-copy", "edit-paste",
        "edit-find", "edit-find-replace",
    };
    bool defaultThemeOk = true;
    for (const auto name : iconNames) {
        if (!QIcon::hasThemeIcon(name)) {
            defaultThemeOk = false;
            break;
        }
    }
    if (!defaultThemeOk)
        QIcon::setThemeName(QStringLiteral("qtextpad"));
}

#define trMain(text) QCoreApplication::translate("main", text)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qtextpad"));
    app.setApplicationVersion(QTextPadSettings::version());

    QCommandLineParser parser;
    parser.setApplicationDescription(trMain("qtextpad - The lightweight Qt code and text editor"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("filename", trMain("A document to open at startup"),
                                 trMain("[filename]"));
    parser.addPositionalArgument("line", trMain("Move the cursor to the specified line"),
                                 trMain("[+line]"));

    const QCommandLineOption encodingOption(QStringList() << "e" << "encoding",
            trMain("Specify the encoding of the file (default: detect)"),
            trMain("encoding"));
    const QCommandLineOption syntaxOption(QStringList() << "S" << "syntax",
            trMain("Specify the syntax definition to use for the file (default: detect)"),
            trMain("syntax"));
    parser.addOption(encodingOption);
    parser.addOption(syntaxOption);

    parser.process(app);

    setDefaultIconTheme();

    // TODO: Make a unique icon for QTextPad?
    // This one is borrowed from Oxygen
    QIcon appIcon;
    appIcon.addFile(":/icons/qtextpad-64.png", QSize(64, 64));
    appIcon.addFile(":/icons/qtextpad-48.png", QSize(48, 48));
    appIcon.addFile(":/icons/qtextpad-32.png", QSize(32, 32));
    appIcon.addFile(":/icons/qtextpad-16.png", QSize(16, 16));
    appIcon.addFile(":/icons/qtextpad-128.png", QSize(128, 128));
    app.setWindowIcon(appIcon);

    QTextPadWindow win;
    win.show();

    QString startupFile;
    int startupLine = -1;
    int startupCol = -1;
    for (const auto &arg : parser.positionalArguments()) {
        if (arg.startsWith(QLatin1Char('+'))) {
            bool ok;
            QStringList parts = arg.split(QLatin1Char(','));
            startupLine = parts.at(0).midRef(1).toInt(&ok, 0);
            if (!ok) {
                qWarning(trMain("Invalid startup line parameter: '%s'").toLocal8Bit().constData(),
                         arg.toLocal8Bit().constData());
                startupLine = -1;
            }
            if (parts.size() > 1) {
                startupCol = parts.at(1).toInt(&ok, 0);
                if (!ok) {
                    qWarning(trMain("Invalid startup line parameter: '%s'").toLocal8Bit().constData(),
                             arg.toLocal8Bit().constData());
                    startupCol = -1;
                }
            }
        } else {
            startupFile = arg;
        }
    }

    QString textEncoding;
    if (parser.isSet(encodingOption))
        textEncoding = parser.value(encodingOption);
    if (!startupFile.isEmpty() && win.loadDocumentFrom(startupFile, textEncoding)) {
        if (startupLine > 0)
            win.gotoLine(startupLine, startupCol);
        if (parser.isSet(syntaxOption)) {
            auto syntaxRepo = SyntaxTextEdit::syntaxRepo();
            auto syntaxDef = syntaxRepo->definitionForName(parser.value(syntaxOption));
            if (syntaxDef.isValid()) {
                win.setSyntax(syntaxDef);
            } else {
                qDebug(trMain("Invalid syntax definition specified: %s").toLocal8Bit().constData(),
                       parser.value(syntaxOption).toLocal8Bit().constData());
            }
        }
    }

    return app.exec();
}
