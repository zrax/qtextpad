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

#include "ftdetect.h"

#include <QTextCodec>
#include <QRegularExpression>
#include <Definition>
#include <Repository>
#include <magic.h>

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
    QTextCodec *textCodec;
    int bomOffset;
    QTextPadWindow::LineEndingMode lineEndings;
    KSyntaxHighlighting::Definition syntaxDefinition;
};

struct magic_t_RAII
{
    magic_t m_magic;

    magic_t_RAII(magic_t magic) : m_magic(magic) { }
    ~magic_t_RAII() { magic_close(m_magic); }

    operator magic_t() { return m_magic; }
    bool operator!() const { return !m_magic; }
};


FileDetection::~FileDetection()
{
    delete reinterpret_cast<DetectionParams_p *>(m_params);
}

QTextCodec *FileDetection::textCodec() const
{
    return reinterpret_cast<DetectionParams_p *>(m_params)->textCodec;
}

int FileDetection::bomOffset() const
{
    return reinterpret_cast<DetectionParams_p *>(m_params)->bomOffset;
}

QTextPadWindow::LineEndingMode FileDetection::lineEndings() const
{
    return reinterpret_cast<DetectionParams_p *>(m_params)->lineEndings;
}

static QTextCodec *detectMagicEncoding(const QString &filename)
{
    magic_t_RAII magic = magic_open(MAGIC_MIME_ENCODING | MAGIC_SYMLINK);
    if (!magic) {
        qDebug("Could not initialize libmagic");
        return Q_NULLPTR;
    }

    if (magic_load(magic, Q_NULLPTR) < 0) {
        qDebug("Could not load magic database: %s", magic_error(magic));
        return Q_NULLPTR;
    }

    const QByteArray filenameEncoded = QFile::encodeName(filename);
    const char *mime = magic_file(magic, filenameEncoded.constData());
    if (!mime) {
        qDebug("Could not get MIME encoding from libmagic: %s", magic_error(magic));
        return Q_NULLPTR;
    }

    // Treat pure 7-bit ASCII as UTF-8
    if (strcmp(mime, "us-ascii") == 0 || strcmp(mime, "utf-8") == 0)
        return QTextCodec::codecForMib(UTF8_MIB);
    if (strcmp(mime, "utf-16le") == 0)
        return QTextCodec::codecForMib(UTF16_LE_MIB);
    if (strcmp(mime, "utf-16be") == 0)
        return QTextCodec::codecForMib(UTF16_BE_MIB);
    if (strcmp(mime, "iso-8859-1") == 0)
        return QTextCodec::codecForMib(LATIN1_MIB);
    if (strcmp(mime, "utf-7") == 0)
        return QTextCodec::codecForMib(UTF7_MIB);

    return Q_NULLPTR;
}

FileDetection FileDetection::detect(const QByteArray &buffer, const QString &filename)
{
    FileDetection result;
    auto params = new DetectionParams_p;
    result.m_params = params;

    // BOM detection based partly on QTextCodec::codecForUtfText, except
    // we try a few more things and keep track of the number of BOM bytes
    // to skip when loading the file.
    params->textCodec = Q_NULLPTR;
    params->bomOffset = 0;
#ifdef _WIN32
    params->lineEndings = QTextPadWindow::CRLF;
#else
    params->lineEndings = QTextPadWindow::LFOnly;
#endif
    if (buffer.size() >= 3) {
        if ((uchar)buffer.at(0) == 0xef && (uchar)buffer.at(1) == 0xbb
                && (uchar)buffer.at(2) == 0xbf) {
            params->textCodec = QTextCodec::codecForMib(UTF8_MIB);
            params->bomOffset = 3;
        }
    }
    if (buffer.size() >= 4 && params->textCodec == Q_NULLPTR) {
        if ((uchar)buffer.at(0) == 0x00 && (uchar)buffer.at(1) == 0x00
                && (uchar)buffer.at(2) == 0xfe && (uchar)buffer.at(3) == 0xff) {
            params->textCodec = QTextCodec::codecForMib(UTF32_BE_MIB);
            params->bomOffset = 4;
        } else if ((uchar)buffer.at(0) == 0xff && (uchar)buffer.at(1) == 0xfe
                && (uchar)buffer.at(2) == 0x00 && (uchar)buffer.at(3) == 0x00) {
            params->textCodec = QTextCodec::codecForMib(UTF32_LE_MIB);
            params->bomOffset = 4;
        }
    }
    if (buffer.size() >= 2 && params->textCodec == Q_NULLPTR) {
        if ((uchar)buffer.at(0) == 0xfe && (uchar)buffer.at(1) == 0xff) {
            params->textCodec = QTextCodec::codecForMib(UTF16_BE_MIB);
            params->bomOffset = 2;
        } else if ((uchar)buffer.at(0) == 0xff && (uchar)buffer.at(1) == 0xfe) {
            params->textCodec = QTextCodec::codecForMib(UTF16_LE_MIB);
            params->bomOffset = 2;
        }
    }

    // If no BOM, try asking libmagic.
    if (params->textCodec == Q_NULLPTR)
        params->textCodec = detectMagicEncoding(filename);

    // If libmagic didn't work for us either, fall back to the system locale,
    // and after that just try ISO-8859-1 (Latin-1) which can decode
    // "anything" (even if incorrectly)
    if (params->textCodec == Q_NULLPTR) {
        QTextCodec::ConverterState state;
        auto codec = QTextCodec::codecForLocale();
        (void) codec->toUnicode(buffer.constData(), buffer.size(), &state);
        if (state.invalidChars == 0)
            params->textCodec = codec;
        else
            params->textCodec = QTextCodec::codecForMib(LATIN1_MIB);
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
        params->lineEndings = QTextPadWindow::LFOnly;
    else if (crlfCount > lfCount && crlfCount > crCount)
        params->lineEndings = QTextPadWindow::CRLF;
    else if (crCount > crlfCount && crCount > lfCount)
        params->lineEndings = QTextPadWindow::CROnly;

    return result;
}

static QString detectMimeType(const QString &filename)
{
    magic_t_RAII magic = magic_open(MAGIC_MIME_TYPE | MAGIC_SYMLINK);
    if (!magic) {
        qDebug("Could not initialize libmagic");
        return QString::null;
    }

    if (magic_load(magic, Q_NULLPTR) < 0) {
        qDebug("Could not load magic database: %s", magic_error(magic));
        return QString::null;
    }

    const QByteArray filenameEncoded = QFile::encodeName(filename);
    const char *mime = magic_file(magic, filenameEncoded.constData());
    if (!mime) {
        qDebug("Could not get MIME type from libmagic: %s", magic_error(magic));
        return QString::null;
    }
    return QString::fromLatin1(mime);
}

// For some reason, KSyntaxHighlighting::Repository doesn't provide a lookup
// for MIME types like it does for names...
KSyntaxHighlighting::Definition FileDetection::definitionForFileMagic(const QString &filename)
{
    using KSyntaxHighlighting::Definition;

    const QString mime = detectMimeType(filename);
    if (mime == QStringLiteral("text/plain"))
        return Definition();

    const QStringList mimeParts = mime.split(QLatin1Char('/'));
    if (mimeParts.size() == 0 || mimeParts.last().isEmpty())
        return Definition();

    QVector<Definition> candidates;
    for (const auto &def : SyntaxTextEdit::syntaxRepo()->definitions()) {
        for (const auto &matchType : def.mimeTypes()) {
            // Only compare the second part, to avoid missing matches due to
            // disagreement about whether XML markup should be text/xml or
            // application/xml (for example)
            const QStringList matchParts = matchType.split(QLatin1Char('/'));
            if (matchParts.last().compare(mimeParts.last(), Qt::CaseInsensitive) == 0) {
                candidates.append(def);
                break;
            }
        }
    }

    if (candidates.isEmpty())
        return Definition();

    std::partial_sort(candidates.begin(), candidates.begin() + 1, candidates.end(),
                      [](const Definition &left, const Definition &right) {
        return left.priority() > right.priority();
    });

    return candidates.first();
}
