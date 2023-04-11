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

#include "undocommands.h"

#include "syntaxtextedit.h"
#include "qtextpadwindow.h"

void TextEditorUndoCommand::undo()
{
    m_editor->undo();
}

void TextEditorUndoCommand::redo()
{
    m_editor->redo();
}


ChangeLineEndingCommand::ChangeLineEndingCommand(QTextPadWindow *window,
                                                 int newMode)
    : m_window(window), m_oldMode(window->lineEndingMode()), m_newMode(newMode)
{
}

void ChangeLineEndingCommand::undo()
{
    m_window->setLineEndingMode(static_cast<FileTypeInfo::LineEndingType>(m_oldMode));
}

void ChangeLineEndingCommand::redo()
{
    m_window->setLineEndingMode(static_cast<FileTypeInfo::LineEndingType>(m_newMode));
}

bool ChangeLineEndingCommand::mergeWith(const QUndoCommand *cmd)
{
    if (cmd->id() != id())
        return false;

    m_newMode = static_cast<const ChangeLineEndingCommand*>(cmd)->m_newMode;
    if (m_oldMode == m_newMode)
        setObsolete(true);

    return true;
}


ChangeEncodingCommand::ChangeEncodingCommand(QTextPadWindow *window,
                                             QString newEncoding)
    : m_window(window), m_oldEncoding(window->textEncoding()),
      m_newEncoding(std::move(newEncoding))
{
}

void ChangeEncodingCommand::undo()
{
    m_window->setEncoding(m_oldEncoding);
}

void ChangeEncodingCommand::redo()
{
    m_window->setEncoding(m_newEncoding);
}

bool ChangeEncodingCommand::mergeWith(const QUndoCommand *cmd)
{
    if (cmd->id() != id())
        return false;

    m_newEncoding = static_cast<const ChangeEncodingCommand*>(cmd)->m_newEncoding;
    if (m_oldEncoding == m_newEncoding)
        setObsolete(true);

    return true;
}


ChangeUtfBOMCommand::ChangeUtfBOMCommand(QTextPadWindow *window)
    : m_window(window), m_utfBOM(window->utfBOM())
{
}

void ChangeUtfBOMCommand::undo()
{
    m_window->setUtfBOM(!m_utfBOM);
}

void ChangeUtfBOMCommand::redo()
{
    m_window->setUtfBOM(m_utfBOM);
}

bool ChangeUtfBOMCommand::mergeWith(const QUndoCommand *cmd)
{
    if (cmd->id() != id())
        return false;

    setObsolete(true);
    return true;
}
