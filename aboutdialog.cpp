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

#include "aboutdialog.h"

#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QMessageBox>

#include "appversion.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About QTextPad"));

    auto iconLabel = new QLabel(this);
    iconLabel->setPixmap(QPixmap(":/icons/qtextpad-64.png"));
    iconLabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    auto aboutText = new QLabel(this);
    aboutText->setText(tr(
        "<b>QTextPad %1</b><br />"
        "<br />"
        "Copyright \u00A9 2019 Michael Hansen<br />"
        "<br />"
        "<a href=\"https://github.com/zrax/qtextpad\">https://github.com/zrax/qtextpad</a><br />"
    ).arg(QTextPadVersion::versionString()));
    aboutText->setTextInteractionFlags(Qt::TextBrowserInteraction);
    aboutText->setOpenExternalLinks(true);

    auto licenseText = new QLabel(this);
    licenseText->setText(tr(
        "QTextPad is free software: you can redistribute it and/or modify "
        "it under the terms of the GNU General Public License as published by "
        "the Free Software Foundation, either version 3 of the License, or "
        "(at your option) any later version.<br />"
        "<br />"
        "QTextPad is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details.<br />"
        "<br />"
        "You should have received a copy of the GNU General Public License "
        "along with QTextPad.  If not, see "
        "&lt;<a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>&gt;."
    ));
    licenseText->setWordWrap(true);
    licenseText->setTextInteractionFlags(Qt::TextBrowserInteraction);
    licenseText->setOpenExternalLinks(true);

    auto buttons = new QDialogButtonBox(this);
    buttons->setStandardButtons(QDialogButtonBox::Close);
    auto aboutQt = buttons->addButton(tr("About Qt"), QDialogButtonBox::ActionRole);
    connect(aboutQt, &QAbstractButton::clicked, [this](bool) {
        QMessageBox::aboutQt(this, tr("About Qt"));
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttons->button(QDialogButtonBox::Close)->setDefault(true);

    auto layout = new QGridLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->addWidget(iconLabel, 0, 0);
    layout->addWidget(aboutText, 0, 1);
    layout->addWidget(licenseText, 1, 0, 1, 2);
    layout->addWidget(buttons, 2, 0, 1, 2);
}
