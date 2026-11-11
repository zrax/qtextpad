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

#include "ipc.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

#include "appsettings.h"
#include "qtextpadwindow.h"

#define S(str) QStringLiteral(str)

namespace {

const int ConnectTimeoutMs = 300;
const int ReplyTimeoutMs = 2000;

enum class IpcResult { Ok, Unreachable, Failed };

QString userTag()
{
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty())
        user = S("user");
    for (QChar &ch : user) {
        if (!ch.isLetterOrNumber())
            ch = QLatin1Char('_');
    }
    return user;
}

QString socketNameForPid(qint64 pid)
{
    return S("qtextpad-%1-%2").arg(userTag()).arg(pid);
}

QString registryFileName()
{
    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!cacheDir.exists())
        cacheDir.mkpath(S("."));
    return cacheDir.absoluteFilePath(S("instances.list"));
}

QList<qint64> readRegistry()
{
    QList<qint64> pids;
    QFile file(registryFileName());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return pids;
    while (!file.atEnd()) {
        bool ok = false;
        const qint64 pid = file.readLine().trimmed().toLongLong(&ok);
        if (ok && pid > 0 && !pids.contains(pid))
            pids.append(pid);
    }
    return pids;
}

void writeRegistry(const QList<qint64> &pids)
{
    QFile file(registryFileName());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning("Could not open %s for writing.", qPrintable(registryFileName()));
        return;
    }
    for (qint64 pid : pids)
        file.write(QByteArray::number(pid) + '\n');
}

void modifyRegistry(const std::function<void(QList<qint64> &)> &modify)
{
    QLockFile lock(registryFileName() + S(".lock"));
    lock.setStaleLockTime(5000);
    lock.tryLock(1000);

    QList<qint64> pids = readRegistry();
    modify(pids);
    writeRegistry(pids);
}

QList<qint64> registryPids()
{
    QLockFile lock(registryFileName() + S(".lock"));
    lock.setStaleLockTime(5000);
    lock.tryLock(1000);
    return readRegistry();
}

QJsonObject command(const char *name)
{
    QJsonObject obj;
    obj.insert(S("cmd"), QString::fromLatin1(name));
    return obj;
}

QJsonObject toJson(const IpcOpenRequest &req)
{
    QJsonObject obj;
    obj.insert(S("path"), req.path);
    obj.insert(S("encoding"), req.encoding);
    obj.insert(S("syntax"), req.syntax);
    obj.insert(S("line"), req.line);
    obj.insert(S("column"), req.column);
    return obj;
}

IpcOpenRequest fromJson(const QJsonObject &obj)
{
    IpcOpenRequest req;
    req.path = obj.value(S("path")).toString();
    req.encoding = obj.value(S("encoding")).toString();
    req.syntax = obj.value(S("syntax")).toString();
    req.line = obj.value(S("line")).toInt(-1);
    req.column = obj.value(S("column")).toInt(-1);
    return req;
}

void writeLine(QLocalSocket *socket, const QJsonObject &obj)
{
    socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
    socket->flush();
}

IpcResult sendRequest(const QString &serverName, const QJsonObject &request,
                      QJsonObject *reply)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(ConnectTimeoutMs))
        return IpcResult::Unreachable;

    // writeLine() flushes, so normally nothing is left to write.
    writeLine(&socket, request);
    if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(ReplyTimeoutMs))
        return IpcResult::Failed;

    // Check what is already buffered before blocking
    QByteArray buffer;
    for ( ;; ) {
        buffer += socket.readAll();
        if (buffer.contains('\n'))
            break;
        if (!socket.waitForReadyRead(ReplyTimeoutMs))
            return IpcResult::Failed;
    }

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(buffer.left(buffer.indexOf('\n')), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return IpcResult::Failed;
    if (reply)
        *reply = doc.object();
    return IpcResult::Ok;
}

} // namespace


QTextPadIpc::QTextPadIpc(QTextPadWindow *window)
    : QObject(window), m_window(window)
{
}

QTextPadIpc::~QTextPadIpc()
{
    const qint64 self = QCoreApplication::applicationPid();
    modifyRegistry([self](QList<qint64> &pids) { pids.removeAll(self); });
}

bool QTextPadIpc::start()
{
    const qint64 self = QCoreApplication::applicationPid();
    const QString name = socketNameForPid(self);

    // A leftover socket from a crashed process conflicting pid
    QLocalServer::removeServer(name);

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection,
            this, &QTextPadIpc::acceptConnections);
    if (!m_server->listen(name)) {
        qWarning("Could not create IPC server %s: %s", qPrintable(name),
                 qPrintable(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }

    m_lastActive = QDateTime::currentMSecsSinceEpoch();
    if (m_window)
        m_window->installEventFilter(this);

    modifyRegistry([self](QList<qint64> &pids) {
        if (!pids.contains(self))
            pids.append(self);
    });
    return true;
}

bool QTextPadIpc::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_window && event->type() == QEvent::WindowActivate)
        m_lastActive = QDateTime::currentMSecsSinceEpoch();
    return QObject::eventFilter(obj, event);
}

void QTextPadIpc::acceptConnections()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this,
                [this, socket] { readClient(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);

        // The request may already be buffered: readyRead won't fire again for it.
        if (socket->bytesAvailable() > 0)
            readClient(socket);
    }
}

void QTextPadIpc::readClient(QLocalSocket *socket)
{
    while (socket->canReadLine()) {
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(socket->readLine(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            QJsonObject reply;
            reply.insert(S("ok"), false);
            reply.insert(S("error"), S("malformed message"));
            writeLine(socket, reply);
            continue;
        }
        handleMessage(socket, doc.object());
    }
}

void QTextPadIpc::handleMessage(QLocalSocket *socket, const QJsonObject &message)
{
    const QString cmd = message.value(S("cmd")).toString();
    QPointer<QTextPadWindow> window = m_window;

    QJsonObject reply;
    reply.insert(S("ok"), true);

    // Always answer first and do the real work afterwards (enqueue):
    if (!window) {
        reply.insert(S("ok"), false);
    } else if (cmd == QStringLiteral("info")) {
        reply.insert(S("pid"), static_cast<double>(QCoreApplication::applicationPid()));
        reply.insert(S("lastActive"), static_cast<double>(m_lastActive));

        // Read live so toggling the setting takes effect without a restart
        reply.insert(S("singleInstance"), QTextPadSettings().singleInstance());
    } else if (cmd == QStringLiteral("has")) {
        // Report only, don't touch anything: used when files aren't forwarded
        reply.insert(S("found"), window->hasDocument(fromJson(message).path));
    } else if (cmd == QStringLiteral("activate")) {
        enqueue([window] {
            if (window)
                window->bringToFront();
        });
    } else if (cmd == QStringLiteral("focus")) {
        const IpcOpenRequest req = fromJson(message);
        const bool found = window->hasDocument(req.path);
        reply.insert(S("found"), found);
        if (found) {
            enqueue([window, req] {
                if (!window)
                    return;
                window->focusDocument(req.path);
                if (req.line > 0)
                    window->gotoLine(req.line, req.column);
            });
        }
    } else if (cmd == QStringLiteral("open")) {
        QList<IpcOpenRequest> requests;
        const QJsonArray files = message.value(S("files")).toArray();
        for (const QJsonValue &value : files)
            requests.append(fromJson(value.toObject()));
        enqueue([window, requests] {
            if (!window)
                return;
            window->bringToFront();
            for (const IpcOpenRequest &req : requests) {
                window->openDocumentWithOptions(req.path, req.encoding, req.syntax,
                                                req.line, req.column);
            }
        });
    } else {
        reply.insert(S("ok"), false);
        reply.insert(S("error"), S("unknown command"));
    }

    writeLine(socket, reply);
}

void QTextPadIpc::enqueue(std::function<void()> action)
{
    m_queue.append(std::move(action));
    QTimer::singleShot(0, this, &QTextPadIpc::runQueue);
}

void QTextPadIpc::runQueue()
{
    if (m_running)
        return;
    if (QApplication::activeModalWidget()) {
        QTimer::singleShot(250, this, &QTextPadIpc::runQueue);
        return;
    }

    m_running = true;
    while (!m_queue.isEmpty()) {
        auto action = m_queue.takeFirst();
        action();
    }
    m_running = false;
}

bool QTextPadIpc::forwardToExisting(QList<IpcOpenRequest> &files)
{
    struct Peer
    {
        qint64 pid;
        QString name;
        qint64 lastActive;
    };

    QList<Peer> peers;
    const qint64 self = QCoreApplication::applicationPid();
    const QList<qint64> pids = registryPids();
    for (const qint64 pid : pids) {
        if (pid == self)
            continue;

        const QString name = socketNameForPid(pid);
        QJsonObject reply;
        const IpcResult result = sendRequest(name, command("info"), &reply);
        if (result == IpcResult::Unreachable) {
            // Stale entry left behind by a crashed instance
            modifyRegistry([pid](QList<qint64> &list) { list.removeAll(pid); });
            continue;
        }
        if (result != IpcResult::Ok)
            continue;

        // The peer decides whether it wants forwarded files
        if (!reply.value(S("singleInstance")).toBool())
            continue;
        peers.append({ pid, name,
                       static_cast<qint64>(reply.value(S("lastActive")).toDouble()) });
    }
    if (peers.isEmpty())
        return false;

    // Most recently used window first
    std::sort(peers.begin(), peers.end(),
              [](const Peer &a, const Peer &b) { return a.lastActive > b.lastActive; });

    QJsonObject reply;
    if (files.isEmpty()) {
        return sendRequest(peers.first().name, command("activate"), &reply)
               == IpcResult::Ok;
    }

    // Files which are already open somewhere get focused there
    for (auto it = files.begin(); it != files.end(); ) {
        bool handled = false;
        for (const Peer &peer : std::as_const(peers)) {
            QJsonObject request = toJson(*it);
            request.insert(S("cmd"), S("focus"));
            if (sendRequest(peer.name, request, &reply) == IpcResult::Ok
                    && reply.value(S("found")).toBool()) {
                handled = true;
                break;
            }
        }
        it = handled ? files.erase(it) : it + 1;
    }
    if (files.isEmpty())
        return true;

    QJsonArray array;
    for (const IpcOpenRequest &req : std::as_const(files))
        array.append(toJson(req));
    QJsonObject request = command("open");
    request.insert(S("files"), array);
    if (sendRequest(peers.first().name, request, &reply) == IpcResult::Ok
            && reply.value(S("ok")).toBool()) {
        files.clear();
        return true;
    }

    // Whatever is left in files is up to the caller
    return false;
}

QString QTextPadIpc::findInstanceWithFile(const IpcOpenRequest &req)
{
    const qint64 self = QCoreApplication::applicationPid();
    const QList<qint64> pids = registryPids();
    for (const qint64 pid : pids) {
        if (pid == self)
            continue;

        const QString name = socketNameForPid(pid);
        QJsonObject request = toJson(req);
        request.insert(S("cmd"), S("has"));
        QJsonObject reply;
        const IpcResult result = sendRequest(name, request, &reply);
        if (result == IpcResult::Unreachable) {
            // Stale entry left behind by a crashed instance
            modifyRegistry([pid](QList<qint64> &list) { list.removeAll(pid); });
            continue;
        }
        if (result == IpcResult::Ok && reply.value(S("found")).toBool())
            return name;
    }
    return QString();
}

bool QTextPadIpc::focusFileInInstance(const QString &serverName, const IpcOpenRequest &req)
{
    QJsonObject request = toJson(req);
    request.insert(S("cmd"), S("focus"));
    QJsonObject reply;
    return sendRequest(serverName, request, &reply) == IpcResult::Ok
           && reply.value(S("found")).toBool();
}
