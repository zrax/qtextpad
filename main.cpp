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
#include <QIcon>

#include "qtextpadwindow.h"

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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

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

    return app.exec();
}
