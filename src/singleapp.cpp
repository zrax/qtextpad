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

#include "singleapp.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QThread>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#endif

constexpr int CONNECT_TIMEOUT_MS = 200;
constexpr int IPC_TIMEOUT_MS = 2000;

QTextpadSingleApp::QTextpadSingleApp(int &argc, char **argv)
    : QApplication(argc, argv)
{
    // Generate a local socket name for the current user (or session on Windows).
    m_socketName = QStringLiteral("qtextpad-ipc-");
#if defined(Q_OS_WIN)
    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    m_socketName += QString::number(sessionId);
#else
    m_socketName += QString::number(getuid());
#endif

    QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    if (!tempDir.exists()) {
        if (!tempDir.mkpath(QStringLiteral(".")))
            qWarning("Could not create temp directory %s.", qPrintable(tempDir.absolutePath()));
    }
    m_lockFile = new QLockFile(tempDir.absoluteFilePath(m_socketName + QLatin1String(".lock")));
    m_lockFile->setStaleLockTime(0);
}

bool QTextpadSingleApp::startIpcServer()
{
    if (m_server)
        return true;

    if (!m_lockFile->tryLock(CONNECT_TIMEOUT_MS)) {
        // tryLock() will already attempt to remove a stale lock file, so if
        // this fails, it means that either another IPC server is already
        // running, or that we can't create the lock file anyway.
        if (m_lockFile->error() != QLockFile::LockFailedError)
            qDebug("Failed to lock IPC socket %s: %d", qPrintable(m_socketName), m_lockFile->error());
        return false;
    }

    QLocalServer::removeServer(m_socketName);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *clientSock = m_server->nextPendingConnection()) {
            connect(clientSock, &QLocalSocket::readyRead, this,
                    [this, clientSock] { handleClient(clientSock); });
            connect(clientSock, &QLocalSocket::disconnected, clientSock, &QObject::deleteLater);

            // The request may already be buffered: readyRead won't fire again for it.
            if (clientSock->bytesAvailable() > 0)
                handleClient(clientSock);
        }
    });

    if (!m_server->listen(m_socketName)) {
        qWarning("Could not create IPC server %s: %s",
                 qPrintable(m_socketName), qPrintable(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        m_lockFile->unlock();
        return false;
    }

    return true;
}

bool QTextpadSingleApp::sendMessage(const QJsonObject &message) const
{
    QLocalSocket clientSock;
    clientSock.connectToServer(m_socketName);
    if (!clientSock.waitForConnected(CONNECT_TIMEOUT_MS)) {
        qDebug("Failed to connect to IPC server");
        return false;
    }

    clientSock.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    clientSock.flush();
    if (clientSock.bytesToWrite() > 0 && !clientSock.waitForBytesWritten(IPC_TIMEOUT_MS)) {
        qDebug("Failed to write to IPC server: %s", qPrintable(clientSock.errorString()));
        return false;
    }

    // We don't currently handle any response from the server, just send it
    // off and then exit.
    return true;
}

void QTextpadSingleApp::handleClient(QLocalSocket *clientSock)
{
    while (clientSock->canReadLine()) {
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(clientSock->readLine(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning("Failed to parse message from IPC server: %s", qPrintable(err.errorString()));
            continue;
        }
        Q_EMIT messageReceived(doc.object());
    }
}
