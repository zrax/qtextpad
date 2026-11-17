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

#include "filetypeinfo.h"

#include <QRegularExpression>
#include <QMimeDatabase>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include "charsets.h"
#include "syntaxtextedit.h"

FileTypeInfo FileTypeInfo::detect(const QByteArray &buffer)
{
    FileTypeInfo result;

    // BOM detection based partly on QTextCodec::codecForUtfText, except
    // we try a few more things and keep track of the number of BOM bytes
    // to skip when loading the file.
    result.m_textCodec = Q_NULLPTR;
    result.m_bomOffset = 0;
#ifdef _WIN32
    result.m_lineEndings = CRLF;
#else
    result.m_lineEndings = LFOnly;
#endif
    if (buffer.size() >= 3) {
        if ((uchar)buffer.at(0) == 0xef && (uchar)buffer.at(1) == 0xbb
                && (uchar)buffer.at(2) == 0xbf) {
            result.m_textCodec = QTextPadCharsets::codecForName("UTF-8");
            result.m_bomOffset = 3;
        }
    }
    if (buffer.size() >= 4 && result.m_textCodec == Q_NULLPTR) {
        if ((uchar)buffer.at(0) == 0x00 && (uchar)buffer.at(1) == 0x00
                && (uchar)buffer.at(2) == 0xfe && (uchar)buffer.at(3) == 0xff) {
            result.m_textCodec = QTextPadCharsets::codecForName("UTF-32BE");
            result.m_bomOffset = 4;
        } else if ((uchar)buffer.at(0) == 0xff && (uchar)buffer.at(1) == 0xfe
                && (uchar)buffer.at(2) == 0x00 && (uchar)buffer.at(3) == 0x00) {
            result.m_textCodec = QTextPadCharsets::codecForName("UTF-32LE");
            result.m_bomOffset = 4;
        } else if (buffer.at(0) == '+' && buffer.at(1) == '/' && buffer.at(2) == 'v'
                && (buffer.at(3) == '8' || buffer.at(3) == '9' || buffer.at(3) == '+'
                        || buffer.at(3) == '/')) {
            result.m_textCodec = QTextPadCharsets::codecForName("UTF-7");
            result.m_bomOffset = 4;
        }
    }
    if (buffer.size() >= 2 && result.m_textCodec == Q_NULLPTR) {
        if ((uchar)buffer.at(0) == 0xfe && (uchar)buffer.at(1) == 0xff) {
            result.m_textCodec = QTextPadCharsets::codecForName("UTF-16BE");
            result.m_bomOffset = 2;
        } else if ((uchar)buffer.at(0) == 0xff && (uchar)buffer.at(1) == 0xfe) {
            result.m_textCodec = QTextPadCharsets::codecForName("UTF-16LE");
            result.m_bomOffset = 2;
        }
    }

    // If we don't have a recognizable BOM, try seeing if the UTF-8 codec
    // can decode it without any errors
    if (result.m_textCodec == Q_NULLPTR) {
        auto codec = QTextPadCharsets::codecForName("UTF-8");
        if (codec->canDecode(buffer))
            result.m_textCodec = codec;
    }

    // Fall back to the system locale, and after that just try ISO-8859-1
    // (Latin-1) which can decode "anything" (even if incorrectly)
    if (result.m_textCodec == Q_NULLPTR) {
        auto codec = QTextPadCharsets::codecForLocale();
        if (codec->canDecode(buffer))
            result.m_textCodec = codec;
        else
            result.m_textCodec = QTextPadCharsets::codecForName("ISO-8859-1");
    }

    // Now try to detect line endings.  If there are no line endings, or
    // there is no clear winner, then we stick with the platform default
    // set above.
    int crlfCount = 0;
    int crCount = 0;
    int lfCount = 0;
    for (int i = 0; i < buffer.size(); ++i) {
        if (buffer.at(i) == '\n') {
            lfCount += 1;
        } else if (buffer.at(i) == '\r') {
            if (i + 1 < buffer.size() && buffer.at(i + 1) == '\n') {
                crlfCount += 1;
                ++i;
            } else {
                crCount += 1;
            }
        }
    }
    if (lfCount > crlfCount && lfCount > crCount)
        result.m_lineEndings = LFOnly;
    else if (crlfCount > lfCount && crlfCount > crCount)
        result.m_lineEndings = CRLF;
    else if (crCount > crlfCount && crCount > lfCount)
        result.m_lineEndings = CROnly;

    return result;
}

// For some reason, KSyntaxHighlighting::Repository doesn't provide a lookup
// for MIME types like it does for names...
KSyntaxHighlighting::Definition FileTypeInfo::definitionForFileMagic(const QString &filename)
{
    using KSyntaxHighlighting::Definition;

    QMimeDatabase mimeDb;
    const auto &mime = mimeDb.mimeTypeForFile(filename);
    if (mime.isDefault() || mime.name() == QStringLiteral("text/plain"))
        return Definition();

    Definition matchDef;
    int matchPriority = std::numeric_limits<int>::min();
    const auto definitions = SyntaxTextEdit::syntaxRepo()->definitions();
    for (const auto &def : definitions) {
        if (def.priority() < matchPriority)
            continue;

        const auto mimeTypes = def.mimeTypes();
        for (const auto &matchType : mimeTypes) {
            if (mime.name() == matchType || mime.aliases().indexOf(matchType) >= 0) {
                matchDef = def;
                matchPriority = def.priority();
                break;
            }
        }
    }

    return matchDef;
}
