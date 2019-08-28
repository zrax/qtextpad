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

#include "definitiondownload.h"

#if (SyntaxHighlighting_VERSION >= ((5<<16)|(56<<8)|(0)))

#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <KSyntaxHighlighting/DefinitionDownloader>

#include "appsettings.h"

class ResizedPlainTextEdit : public QPlainTextEdit
{
public:
    ResizedPlainTextEdit(QWidget *parent) : QPlainTextEdit(parent) { }

protected:
    QSize sizeHint() const override
    {
        const auto &metrics = fontMetrics();
        return QSize(metrics.boundingRect(QString(60, QLatin1Char('x'))).width(),
                     metrics.height() * 10);
    }
};

DefinitionDownloadDialog::DefinitionDownloadDialog(
        KSyntaxHighlighting::Repository *repository, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Update Syntax Definitions"));
    setModal(true);

    QTextPadSettings settings;
    m_status = new ResizedPlainTextEdit(this);
    m_status->setFont(settings.editorFont());
    m_status->setReadOnly(true);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_buttonBox->button(QDialogButtonBox::Close)->setEnabled(false);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(m_status);
    layout->addWidget(m_buttonBox);

    m_downloader = new KSyntaxHighlighting::DefinitionDownloader(repository, this);
    QObject::connect(m_downloader, &KSyntaxHighlighting::DefinitionDownloader::informationMessage,
                     m_status, &QPlainTextEdit::appendPlainText);
    QObject::connect(m_downloader, &KSyntaxHighlighting::DefinitionDownloader::done,
                     this, &DefinitionDownloadDialog::downloadFinished);

    m_status->appendPlainText(tr("Updating syntax definitions from online repository..."));
    m_downloader->start();
}

void DefinitionDownloadDialog::downloadFinished()
{
    m_buttonBox->button(QDialogButtonBox::Close)->setEnabled(true);
    m_downloader->deleteLater();
}

void DefinitionDownloadDialog::closeEvent(QCloseEvent *e)
{
    if (!m_buttonBox->button(QDialogButtonBox::Close)->isEnabled())
        e->ignore();
}

#endif // SyntaxHighlighting_VERSION check
