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

#ifndef _FILETYPEINFO_H
#define _FILETYPEINFO_H

#include <QByteArray>

class TextCodec;

namespace KSyntaxHighlighting
{
    class Definition;
}

class FileTypeInfo
{
public:
    enum LineEndingType
    {
        CROnly,
        LFOnly,
        CRLF,
    };

    FileTypeInfo() : m_params() { }
    ~FileTypeInfo();

    static FileTypeInfo detect(const QByteArray &buffer);

    FileTypeInfo(const FileTypeInfo &) = delete;
    FileTypeInfo &operator=(const FileTypeInfo &) = delete;

    FileTypeInfo(FileTypeInfo &&move) Q_DECL_NOEXCEPT
        : m_params(move.m_params)
    {
        move.m_params = Q_NULLPTR;
    }

    FileTypeInfo &operator=(FileTypeInfo &&move) Q_DECL_NOEXCEPT
    {
        Q_ASSERT(m_params == Q_NULLPTR);
        m_params = move.m_params;
        move.m_params = Q_NULLPTR;
        return *this;
    }

    bool isValid() const { return m_params != Q_NULLPTR; }

    TextCodec *textCodec() const;
    int bomOffset() const;
    LineEndingType lineEndings() const;

    static KSyntaxHighlighting::Definition definitionForFileMagic(const QString &filename);

private:
    void *m_params;
};

#endif  // _FILETYPEINFO_H
