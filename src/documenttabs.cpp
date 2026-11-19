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

#include <QApplication>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QMouseEvent>

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

QString DocumentTab::documentName() const
{
    return filename.isEmpty() ? tr("Untitled") : QFileInfo(filename).fileName();
}

QString DocumentTab::absolutePath() const
{
    return filename.isEmpty() ? QString() : QFileInfo(filename).absoluteFilePath();
}

void DocumentTabBar::mousePressEvent(QMouseEvent *event)
{
    m_pressedTabIndex = (event->button() == Qt::LeftButton) ? tabAt(event->pos()) : -1;

    // The user can drag the tab horizontally before detaching it, so we need
    // to capture the pixmap ahead of time to ensure it is drawn correctly.
    m_dragTabPixmap = (m_pressedTabIndex >= 0) ? grab(tabRect(m_pressedTabIndex)) : QPixmap();

    QTabBar::mousePressEvent(event);
}

void DocumentTabBar::mouseMoveEvent(QMouseEvent *event)
{
    // Moving sideways reorders tabs (built into QTabBar). Leaving the bar
    // vertically turns it into a drag which another window can accept
    if (m_pressedTabIndex >= 0 && (event->buttons() & Qt::LeftButton) != 0) {
        const int margin = 2 * QApplication::startDragDistance();
        const int y = event->pos().y();
        if (y < -margin || y > height() + margin) {
            const int index = m_pressedTabIndex;
            m_pressedTabIndex = -1;

            // Finish QTabBar's own move-in-progress before the QDrag takes over
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QMouseEvent release(QEvent::MouseButtonRelease, event->position(),
                                event->globalPosition(), Qt::MouseButton::LeftButton,
                                Qt::MouseButton::NoButton, event->modifiers());
#else
            QMouseEvent release(QEvent::MouseButtonRelease, event->pos(),
                                event->globalPos(), Qt::MouseButton::LeftButton,
                                Qt::MouseButton::NoButton, event->modifiers());
#endif
            QTabBar::mouseReleaseEvent(&release);
            Q_EMIT tabDragRequested(index, m_dragTabPixmap);
            return;
        }
    }

    QTabBar::mouseMoveEvent(event);
}

void DocumentTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressedTabIndex = -1;
    m_dragTabPixmap = QPixmap();

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

void DocumentTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MouseButton::LeftButton) {
        const int tabIndex = tabAt(event->pos());
        if (tabIndex < 0) {
            Q_EMIT newTabRequested();
            return;
        }
    }

    QTabBar::mouseDoubleClickEvent(event);
}


DocumentTabWidget::DocumentTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    QTextPadSettings settings;

    // This must be created before anything that affects the tabs or tab bar is called
    auto tabBar = new DocumentTabBar(this);
    setTabBar(tabBar);

    setDocumentMode(true);
    setMovable(true);
    setTabsClosable(true);
    tabBar->setAcceptDrops(true);
    setTabBarAutoHide(settings.hideSingleTab());

    connect(tabBar, &DocumentTabBar::newTabRequested,
            this, &DocumentTabWidget::newTabRequested);
    connect(tabBar, &DocumentTabBar::tabDragRequested,
            this, &DocumentTabWidget::tabDragRequested);
}
