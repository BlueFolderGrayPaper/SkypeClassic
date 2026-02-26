#include "server/ChatServer.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

ChatServer::ChatServer(quint16 port, QObject* parent)
    : QObject(parent)
    , m_server(new QWebSocketServer("SkypeClassicServer",
          QWebSocketServer::NonSecureMode, this))
{
    Q_UNUSED(port);

    // Create some default accounts
    m_users["echo123"] = {"echo123", "echo", {}};
    m_users["alice"] = {"alice", "alice", {"bob", "charlie", "echo123"}};
    m_users["bob"] = {"bob", "bob", {"alice", "charlie"}};
    m_users["charlie"] = {"charlie", "charlie", {"alice", "bob"}};
}

ChatServer::~ChatServer() {
    m_server->close();
    for (auto& client : m_clients) {
        client.socket->close();
    }
}

bool ChatServer::start() {
    if (m_server->listen(QHostAddress::Any, 33033)) {
        qDebug() << "Chat server listening on port 33033";
        connect(m_server, &QWebSocketServer::newConnection,
                this, &ChatServer::onNewConnection);
        return true;
    }
    qWarning() << "Failed to start server:" << m_server->errorString();
    return false;
}

void ChatServer::onNewConnection() {
    auto* socket = m_server->nextPendingConnection();
    connect(socket, &QWebSocket::textMessageReceived, this, &ChatServer::onTextMessage);
    connect(socket, &QWebSocket::disconnected, this, &ChatServer::onDisconnected);

    ConnectedClient client;
    client.socket = socket;
    m_clients.append(client);

    qDebug() << "New connection from" << socket->peerAddress().toString();
}

void ChatServer::onTextMessage(const QString& message) {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    if (message.size() > 65536) {
        qWarning() << "Dropping oversized message:" << message.size() << "bytes";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "login") {
        handleLogin(socket, obj);
    } else if (type == "register") {
        handleRegister(socket, obj);
    } else if (type == "message") {
        handleMessage(socket, obj);
    } else if (type == "get_contacts") {
        handleContactList(socket);
    } else if (type == "add_contact") {
        handleAddContact(socket, obj);
    } else if (type == "status") {
        handleStatusChange(socket, obj);
    }
}

void ChatServer::onDisconnected() {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    QString username;
    for (int i = 0; i < m_clients.size(); ++i) {
        if (m_clients[i].socket == socket) {
            username = m_clients[i].username;
            m_clients.removeAt(i);
            break;
        }
    }

    if (!username.isEmpty()) {
        m_onlineStatus.remove(username);
        broadcastPresence(username, "Offline");
        qDebug() << username << "disconnected";
    }

    socket->deleteLater();
}

void ChatServer::handleLogin(QWebSocket* socket, const QJsonObject& data) {
    QString username = data["username"].toString().toLower();
    QString password = data["password"].toString();

    // Auto-register if user doesn't exist
    if (!m_users.contains(username)) {
        ServerUser newUser;
        newUser.username = username;
        newUser.password = password;
        newUser.contacts << "echo123";
        m_users[username] = newUser;

        // Add to echo123's contacts too
        if (m_users.contains("echo123")) {
            m_users["echo123"].contacts.append(username);
        }
    }

    ServerUser& user = m_users[username];
    if (user.password != password && !password.isEmpty()) {
        sendJson(socket, {{"type", "login_result"}, {"success", false},
                          {"error", "Invalid password"}});
        return;
    }

    // Associate socket with username
    for (auto& client : m_clients) {
        if (client.socket == socket) {
            client.username = username;
            break;
        }
    }

    m_onlineStatus[username] = "Online";

    sendJson(socket, {{"type", "login_result"}, {"success", true},
                      {"username", username}});

    // Send contact list
    handleContactList(socket);

    // Broadcast that this user is online
    broadcastPresence(username, "Online");

    qDebug() << username << "logged in";
}

void ChatServer::handleRegister(QWebSocket* socket, const QJsonObject& data) {
    QString username = data["username"].toString().toLower();
    QString password = data["password"].toString();

    if (m_users.contains(username)) {
        sendJson(socket, {{"type", "register_result"}, {"success", false},
                          {"error", "Username already exists"}});
        return;
    }

    ServerUser newUser;
    newUser.username = username;
    newUser.password = password;
    newUser.contacts << "echo123";
    m_users[username] = newUser;

    sendJson(socket, {{"type", "register_result"}, {"success", true}});
}

void ChatServer::handleMessage(QWebSocket* socket, const QJsonObject& data) {
    QString from;
    for (auto& client : m_clients) {
        if (client.socket == socket) {
            from = client.username;
            break;
        }
    }
    if (from.isEmpty()) return;

    QString to = data["to"].toString();
    QString text = data["text"].toString();

    // Relay to recipient if online
    QWebSocket* recipientSocket = findClientSocket(to);
    if (recipientSocket) {
        sendJson(recipientSocket, {
            {"type", "message"},
            {"from", from},
            {"text", text},
            {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
        });
    }

    // Echo service: auto-reply
    if (to == "echo123") {
        QJsonObject echo;
        echo["type"] = "message";
        echo["from"] = "echo123";
        echo["text"] = QString("Echo: %1").arg(text);
        echo["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        sendJson(socket, echo);
    }

    // Acknowledge to sender
    sendJson(socket, {
        {"type", "message_ack"},
        {"to", to},
        {"text", text},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    });
}

void ChatServer::handleContactList(QWebSocket* socket) {
    QString username;
    for (auto& client : m_clients) {
        if (client.socket == socket) {
            username = client.username;
            break;
        }
    }
    if (username.isEmpty()) return;

    QJsonArray contactArray;
    for (auto& contactName : m_users[username].contacts) {
        QJsonObject contactObj;
        contactObj["username"] = contactName;
        contactObj["displayName"] = contactName;
        contactObj["status"] = m_onlineStatus.value(contactName, "Offline");
        contactArray.append(contactObj);
    }

    sendJson(socket, {{"type", "contact_list"}, {"contacts", contactArray}});
}

void ChatServer::handleAddContact(QWebSocket* socket, const QJsonObject& data) {
    QString username;
    for (auto& client : m_clients) {
        if (client.socket == socket) {
            username = client.username;
            break;
        }
    }
    if (username.isEmpty()) return;

    QString contactName = data["contact"].toString().toLower();

    if (!m_users[username].contacts.contains(contactName)) {
        m_users[username].contacts.append(contactName);
    }

    // Mutual add
    if (m_users.contains(contactName)) {
        if (!m_users[contactName].contacts.contains(username)) {
            m_users[contactName].contacts.append(username);
        }

        // Notify the other user
        QWebSocket* otherSocket = findClientSocket(contactName);
        if (otherSocket) {
            handleContactList(otherSocket);
        }
    }

    handleContactList(socket);

    sendJson(socket, {{"type", "add_contact_result"}, {"success", true},
                      {"contact", contactName}});
}

void ChatServer::handleStatusChange(QWebSocket* socket, const QJsonObject& data) {
    QString username;
    for (auto& client : m_clients) {
        if (client.socket == socket) {
            username = client.username;
            break;
        }
    }
    if (username.isEmpty()) return;

    QString status = data["status"].toString();
    m_onlineStatus[username] = status;
    broadcastPresence(username, status);
}

void ChatServer::sendJson(QWebSocket* socket, const QJsonObject& obj) {
    socket->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void ChatServer::broadcastPresence(const QString& username, const QString& status) {
    QJsonObject presenceMsg;
    presenceMsg["type"] = "presence";
    presenceMsg["username"] = username;
    presenceMsg["status"] = status;

    for (auto& client : m_clients) {
        if (client.username != username && !client.username.isEmpty()) {
            // Only send to contacts of this user
            if (m_users.contains(client.username) &&
                m_users[client.username].contacts.contains(username)) {
                sendJson(client.socket, presenceMsg);
            }
        }
    }
}

QWebSocket* ChatServer::findClientSocket(const QString& username) {
    for (auto& client : m_clients) {
        if (client.username == username) {
            return client.socket;
        }
    }
    return nullptr;
}
