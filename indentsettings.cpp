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

#include "indentsettings.h"

#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>

#include "syntaxtextedit.h"

IndentSettingsDialog::IndentSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Tab Settings"));

    auto indentModeLabel = new QLabel(tr("Indentation &Mode:"), this);
    m_indentMode = new QComboBox(this);
    m_indentMode->addItem(tr("Spaces Only"), SyntaxTextEdit::IndentSpaces);
    m_indentMode->addItem(tr("Tabs Only"), SyntaxTextEdit::IndentTabs);
    m_indentMode->addItem(tr("Mixed (Tabs and Spaces)"), SyntaxTextEdit::IndentMixed);
    indentModeLabel->setBuddy(m_indentMode);

    auto tabWidthLabel = new QLabel(tr("&Tab Width:"), this);
    m_tabWidth = new QSpinBox(this);
    m_tabWidth->setRange(1, 99);
    tabWidthLabel->setBuddy(m_tabWidth);
    auto tabWidthHelp = new QLabel(tr("The width (in columns) of the tab character,"
                                      " regardless of indentation mode."), this);
    tabWidthHelp->setWordWrap(true);

    auto indentWidthLabel = new QLabel(tr("&Indentation Width:"), this);
    m_indentWidth = new QSpinBox(this);
    m_indentWidth->setRange(1, 99);
    indentWidthLabel->setBuddy(m_indentWidth);
    auto indentWidthHelp = new QLabel(tr("The number of columns to indent in Spaces"
                                         " Only and Mixed modes.  This value is ignored"
                                         " in Tabs Only mode."), this);
    indentWidthHelp->setWordWrap(true);

    auto buttons = new QDialogButtonBox(Qt::Horizontal, this);
    buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto layout = new QGridLayout(this);
    layout->setMargin(10);
    layout->setSpacing(5);
    layout->addWidget(indentModeLabel, 0, 0);
    layout->addWidget(m_indentMode, 0, 1);
    layout->addItem(new QSpacerItem(0, 10), 1, 0, 1, 2);
    layout->addWidget(tabWidthLabel, 2, 0);
    layout->addWidget(m_tabWidth, 2, 1);
    layout->addWidget(tabWidthHelp, 3, 1);
    layout->addItem(new QSpacerItem(0, 10), 4, 0, 1, 2);
    layout->addWidget(indentWidthLabel, 5, 0);
    layout->addWidget(m_indentWidth, 5, 1);
    layout->addWidget(indentWidthHelp, 6, 1);
    layout->addItem(new QSpacerItem(0, 10), 7, 0, 1, 2);
    layout->addWidget(buttons, 8, 0, 1, 2);
}

void IndentSettingsDialog::loadSettings(SyntaxTextEdit *editor)
{
    const auto indentMode = editor->indentationMode();
    for (int i = 0; i < m_indentMode->count(); ++i) {
        if (m_indentMode->itemData(0) == indentMode) {
            m_indentMode->setCurrentIndex(i);
            break;
        }
    }

    m_tabWidth->setValue(editor->tabWidth());
    m_indentWidth->setValue(editor->indentWidth());
}

void IndentSettingsDialog::applySettings(SyntaxTextEdit *editor)
{
    if (m_indentMode->currentIndex() >= 0)
        editor->setIndentationMode(m_indentMode->currentData().toInt());

    editor->setTabWidth(m_tabWidth->value());
    editor->setIndentWidth(m_indentWidth->value());
}
