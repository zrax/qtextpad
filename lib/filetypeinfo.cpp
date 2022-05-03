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

#define LATIN1_MIB             4    // A.K.A. ISO-8859-1
#define UTF8_MIB             106
#define UTF7_MIB            1012
#define UTF16_BE_MIB        1013
#define UTF16_LE_MIB        1014
#define UTF32_BE_MIB        1018
#define UTF32_LE_MIB        1019

struct DetectionParams_p
{
    TextCodec *textCodec;
    int bomOffset;
    FileTypeInfo::LineEndingType lineEndings;
};


FileTypeInfo::~FileTypeInfo()
{
    delete reinterpret_cast<DetectionParams_p *>(m_params);
}

TextCodec *FileTypeInfo::textCodec() const
{
    return reinterpret_cast<DetectionParams_p *>(m_params)->textCodec;
}

int FileTypeInfo::bomOffset() const
{
    return reinterpret_cast<DetectionParams_p *>(m_params)->bomOffset;
}

FileTypeInfo::LineEndingType FileTypeInfo::lineEndings() const
{
    return reinterpret_cast<DetectionParams_p *>(m_params)->lineEndings;
}

FileTypeInfo FileTypeInfo::detect(const QByteArray &buffer)
{
    FileTypeInfo result;
    auto params = new DetectionParams_p;
    result.m_params = params;

    // BOM detection based partly on QTextCodec::codecForUtfText, except
    // we try a few more things and keep track of the number of BOM bytes
    // to skip when loading the file.
    params->textCodec = Q_NULLPTR;
    params->bomOffset = 0;
#ifdef _WIN32
    params->lineEndings = CRLF;
#else
    params->lineEndings = LFOnly;
#endif
    if (buffer.size() >= 3) {
        if ((uchar)buffer.at(0) == 0xef && (uchar)buffer.at(1) == 0xbb
                && (uchar)buffer.at(2) == 0xbf) {
            params->textCodec = QTextPadCharsets::codecForMib(UTF8_MIB);
            params->bomOffset = 3;
        }
    }
    if (buffer.size() >= 4 && params->textCodec == Q_NULLPTR) {
        if ((uchar)buffer.at(0) == 0x00 && (uchar)buffer.at(1) == 0x00
                && (uchar)buffer.at(2) == 0xfe && (uchar)buffer.at(3) == 0xff) {
            params->textCodec = QTextPadCharsets::codecForMib(UTF32_BE_MIB);
            params->bomOffset = 4;
        } else if ((uchar)buffer.at(0) == 0xff && (uchar)buffer.at(1) == 0xfe
                && (uchar)buffer.at(2) == 0x00 && (uchar)buffer.at(3) == 0x00) {
            params->textCodec = QTextPadCharsets::codecForMib(UTF32_LE_MIB);
            params->bomOffset = 4;
        } else if (buffer.at(0) == '+' && buffer.at(1) == '/' && buffer.at(2) == 'v'
                && (buffer.at(3) == '8' || buffer.at(3) == '9' || buffer.at(3) == '+'
                        || buffer.at(3) == '/')) {
            params->textCodec = QTextPadCharsets::codecForMib(UTF7_MIB);
            params->bomOffset = 4;
        }
    }
    if (buffer.size() >= 2 && params->textCodec == Q_NULLPTR) {
        if ((uchar)buffer.at(0) == 0xfe && (uchar)buffer.at(1) == 0xff) {
            params->textCodec = QTextPadCharsets::codecForMib(UTF16_BE_MIB);
            params->bomOffset = 2;
        } else if ((uchar)buffer.at(0) == 0xff && (uchar)buffer.at(1) == 0xfe) {
            params->textCodec = QTextPadCharsets::codecForMib(UTF16_LE_MIB);
            params->bomOffset = 2;
        }
    }

    // If we don't have a recognizable BOM, try seeing if Qt's UTF-8 codec
    // can decode it without any errors
    if (params->textCodec == Q_NULLPTR) {
        auto codec = QTextPadCharsets::codecForMib(UTF8_MIB);
        if (codec->canDecode(buffer))
            params->textCodec = codec;
    }

    // Fall back to the system locale, and after that just try ISO-8859-1
    // (Latin-1) which can decode "anything" (even if incorrectly)
    if (params->textCodec == Q_NULLPTR) {
        auto codec = QTextPadCharsets::codecForLocale();
        if (codec->canDecode(buffer))
            params->textCodec = codec;
        else
            params->textCodec = QTextPadCharsets::codecForMib(LATIN1_MIB);
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
        params->lineEndings = LFOnly;
    else if (crlfCount > lfCount && crlfCount > crCount)
        params->lineEndings = CRLF;
    else if (crCount > crlfCount && crCount > lfCount)
        params->lineEndings = CROnly;

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
    for (const auto &def : SyntaxTextEdit::syntaxRepo()->definitions()) {
        if (def.priority() < matchPriority)
            continue;

        for (const auto &matchType : def.mimeTypes()) {
            if (mime.name() == matchType || mime.aliases().indexOf(matchType) >= 0) {
                matchDef = def;
                matchPriority = def.priority();
                break;
            }
        }
    }

    return matchDef;
}
