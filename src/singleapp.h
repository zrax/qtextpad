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

#ifndef QTEXTPAD_SINGLEAPP_H
#define QTEXTPAD_SINGLEAPP_H

#include <QApplication>

class QLockFile;
class QLocalServer;
class QLocalSocket;

class QTextpadSingleApp : public QApplication
{
    Q_OBJECT

public:
    QTextpadSingleApp(int &argc, char **argv);

    bool startIpcServer();
    bool sendMessage(const QJsonObject &message) const;

Q_SIGNALS:
    void messageReceived(const QJsonObject &message);

private Q_SLOTS:
    void handleClient(QLocalSocket *clientSock);

private:
    QString m_socketName;
    QLockFile *m_lockFile = Q_NULLPTR;
    QLocalServer *m_server = Q_NULLPTR;
};

#endif // QTEXTPAD_SINGLEAPP_H
