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

#ifndef QTEXTPAD_IPC_H
#define QTEXTPAD_IPC_H

#include <QObject>
#include <QList>
#include <QPointer>
#include <QString>
#include <functional>

class QLocalServer;
class QLocalSocket;
class QJsonObject;
class QEvent;
class QTextPadWindow;

struct IpcOpenRequest
{
    QString path; // Must be absolute: the receiver has a different cwd
    QString encoding;
    QString syntax;
    int line = -1;
    int column = -1;
};

/*
 * Every QTextPad process listens on its own local socket
 * ("qtextpad-<user>-<pid>") and lists its pid in a small registry file in the
 * cache directory. A new process that wants single-instance behavior walks
 * the registry and, for each live peer:
 *   - "info":     asks whether the peer currently accepts forwarded files
 *                 (its "Single Instance Mode" setting, read live) and when it
 *                 was last active
 *   - "focus":    asks whether the peer already has a given file open, and if
 *                 so, raises that window/tab
 *   - "open":     hands the remaining files to the most recently active peer
 *   - "activate": raises the most recently active peer (launch without files)
 *   - "has":      reports whether a peer has a given file open, without
 *                 touching it (used when Single Instance Mode is off)
 * Anything that can't be handed off is left in the list for the caller to open
 * in its own window, so a failed/stale/declining peer never loses a request.
 */
class QTextPadIpc : public QObject
{
    Q_OBJECT

public:
    // The window is also the parent: the server is torn down with it
    explicit QTextPadIpc(QTextPadWindow *window);
    ~QTextPadIpc() override;

    bool start();

    // Try to hand files to an already running instance
    static bool forwardToExisting(QList<IpcOpenRequest> &files);

    // Single Instance off: find any running instance that already has the file open
    static QString findInstanceWithFile(const IpcOpenRequest &req);

    // Raise that instance window/tab
    static bool focusFileInInstance(const QString &serverName,
                                    const IpcOpenRequest &req);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
    void acceptConnections();
    void runQueue();

private:
    void readClient(QLocalSocket *socket);
    void handleMessage(QLocalSocket *socket, const QJsonObject &message);
    void enqueue(std::function<void()> action);

    QPointer<QTextPadWindow> m_window;
    QLocalServer *m_server = nullptr;
    qint64 m_lastActive = 0;
    QList<std::function<void()>> m_queue;
    bool m_running = false;
};

#endif // QTEXTPAD_IPC_H
