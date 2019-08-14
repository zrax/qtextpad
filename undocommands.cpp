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
    : m_window(window), m_newMode(newMode)
{
    m_oldMode = window->lineEndingMode();

    // Apply the change now, since this is the only time we should
    // construct this undo command
    window->setLineEndingMode(static_cast<QTextPadWindow::LineEndingMode>(newMode));
}

void ChangeLineEndingCommand::undo()
{
    m_window->setLineEndingMode(static_cast<QTextPadWindow::LineEndingMode>(m_oldMode));
}

void ChangeLineEndingCommand::redo()
{
    m_window->setLineEndingMode(static_cast<QTextPadWindow::LineEndingMode>(m_newMode));
}


ChangeEncodingCommand::ChangeEncodingCommand(QTextPadWindow *window,
                                             QString newEncoding)
    : m_window(window), m_newEncoding(std::move(newEncoding))
{
    m_oldEncoding = window->textEncoding();

    // Apply the change now, since this is the only time we should
    // construct this undo command
    window->setEncoding(m_newEncoding);
}

void ChangeEncodingCommand::undo()
{
    m_window->setEncoding(m_oldEncoding);
}

void ChangeEncodingCommand::redo()
{
    m_window->setEncoding(m_newEncoding);
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
