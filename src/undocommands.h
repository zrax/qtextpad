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

#ifndef QTEXTPAD_UNDOCOMMANDS_H
#define QTEXTPAD_UNDOCOMMANDS_H

#include <QUndoCommand>

class SyntaxTextEdit;
class QTextPadWindow;

class TextEditorUndoCommand : public QUndoCommand
{
public:
    explicit TextEditorUndoCommand(SyntaxTextEdit *editor)
        : m_editor(editor) { }

    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;

private:
    SyntaxTextEdit *m_editor;
};

class ChangeLineEndingCommand : public QUndoCommand
{
public:
    ChangeLineEndingCommand(QTextPadWindow *window, int newMode);

    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;

    int id() const Q_DECL_OVERRIDE { return 101; }
    bool mergeWith(const QUndoCommand *cmd) Q_DECL_OVERRIDE;

private:
    QTextPadWindow *m_window;
    int m_oldMode, m_newMode;
};

class ChangeEncodingCommand : public QUndoCommand
{
public:
    ChangeEncodingCommand(QTextPadWindow *window, QString newEncoding);

    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;

    int id() const Q_DECL_OVERRIDE { return 102; }
    bool mergeWith(const QUndoCommand *cmd) Q_DECL_OVERRIDE;

private:
    QTextPadWindow *m_window;
    QString m_oldEncoding, m_newEncoding;
};

class ChangeUtfBOMCommand : public QUndoCommand
{
public:
    explicit ChangeUtfBOMCommand(QTextPadWindow *window);

    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;

    int id() const Q_DECL_OVERRIDE { return 103; }
    bool mergeWith(const QUndoCommand *cmd) Q_DECL_OVERRIDE;

private:
    QTextPadWindow *m_window;
    bool m_utfBOM;
};

#endif // QTEXTPAD_UNDOCOMMANDS_H
