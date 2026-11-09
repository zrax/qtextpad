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

#ifndef QTEXTPAD_DOCUMENTTABS_H
#define QTEXTPAD_DOCUMENTTABS_H

#include <QTabBar>
#include <QTabWidget>
#include <QDateTime>

#include "filetypeinfo.h"

class SyntaxTextEdit;
class QUndoStack;

class DocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentTab(QWidget *parent = Q_NULLPTR);

    SyntaxTextEdit *editor() const { return m_editor; }
    QUndoStack *undoStack() const { return m_undoStack; }

    QString filename;
    unsigned int fileState = 0;
    QDateTime cachedModTime;
    QString textEncoding;
    FileTypeInfo::LineEndingType lineEndingMode = FileTypeInfo::LFOnly;
    bool utfBOM = false;

private:
    SyntaxTextEdit *m_editor;
    QUndoStack *m_undoStack;
};

class DocumentTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit DocumentTabBar(QWidget *parent = Q_NULLPTR)
        : QTabBar(parent) { }

protected:
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
};

class DocumentTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit DocumentTabWidget(QWidget *parent = Q_NULLPTR);
};

#endif // QTEXTPAD_DOCUMENTTABS_H
