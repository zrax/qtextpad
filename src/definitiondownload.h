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

#ifndef QTEXTPAD_DEFINITIONDOWNLOAD_H
#define QTEXTPAD_DEFINITIONDOWNLOAD_H

#include <ksyntaxhighlighting_version.h>

#if (SyntaxHighlighting_VERSION >= ((5<<16)|(56<<8)|(0)))

#include <QDialog>
#include <QElapsedTimer>

class QPlainTextEdit;
class QDialogButtonBox;

namespace KSyntaxHighlighting
{
    class Repository;
    class DefinitionDownloader;
}

class DefinitionDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    DefinitionDownloadDialog(KSyntaxHighlighting::Repository *repository, QWidget *parent);

public slots:
    void downloadFinished();

protected:
    void closeEvent(QCloseEvent *) override;

private:
    KSyntaxHighlighting::DefinitionDownloader *m_downloader;
    QPlainTextEdit *m_status;
    QDialogButtonBox *m_buttonBox;
    QElapsedTimer m_timer;
};

#endif // SyntaxHighlighting_VERSION check

#endif // QTEXTPAD_DEFINITIONDOWNLOAD_H
