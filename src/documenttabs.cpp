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

#include "documenttabs.h"

#include <QUndoStack>
#include <QVBoxLayout>

#include "appsettings.h"
#include "syntaxtextedit.h"

DocumentTab::DocumentTab(QWidget *parent)
    : QWidget(parent)
{
    textEncoding = QStringLiteral("UTF-8");
#ifdef _WIN32
    lineEndingMode = FileTypeInfo::CRLF;
#else
    lineEndingMode = FileTypeInfo::LFOnly;
#endif

    m_editor = new SyntaxTextEdit(this);
    m_editor->setFrameStyle(QFrame::NoFrame);
    m_undoStack = new QUndoStack(this);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_editor);
}

void DocumentTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    // This feature is built-in to Qt as of 6.11
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
    if (event->button() == Qt::MouseButton::MiddleButton) {
        const int tabIndex = tabAt(event->pos());
        if (tabIndex >= 0) {
            Q_EMIT tabCloseRequested(tabIndex);
            return;
        }
    }
#endif

    QTabBar::mouseReleaseEvent(event);
}

DocumentTabWidget::DocumentTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    QTextPadSettings settings;

    // This must be created before anything that affects the tabs or tab bar is called
    setTabBar(new DocumentTabBar(this));

    setDocumentMode(true);
    setMovable(true);
    setTabsClosable(true);
    setTabBarAutoHide(settings.hideSingleTab());
}
