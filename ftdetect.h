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

#ifndef _FTDETECT_H
#define _FTDETECT_H

#include "qtextpadwindow.h"

class FileDetection
{
public:
    FileDetection() : m_params() { }
    ~FileDetection();

    static FileDetection detect(const QByteArray &buffer);

    FileDetection(const FileDetection &) = delete;
    FileDetection &operator=(const FileDetection &) = delete;

    FileDetection(FileDetection &&move) Q_DECL_NOEXCEPT
        : m_params(move.m_params)
    {
        move.m_params = Q_NULLPTR;
    }

    FileDetection &operator=(FileDetection &&move) Q_DECL_NOEXCEPT
    {
        Q_ASSERT(m_params == Q_NULLPTR);
        m_params = move.m_params;
        move.m_params = Q_NULLPTR;
        return *this;
    }

    bool isValid() const { return m_params != Q_NULLPTR; }

    QTextCodec *textCodec() const;
    int bomOffset() const;
    QTextPadWindow::LineEndingMode lineEndings() const;

    static KSyntaxHighlighting::Definition definitionForFileMagic(const QString &filename);

private:
    void *m_params;
};

#endif  // _FTDETECT_H
