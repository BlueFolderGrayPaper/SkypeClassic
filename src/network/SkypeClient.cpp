#include "network/SkypeClient.h"

#include <QJsonDocument>
#include <QDebug>

SkypeClient::SkypeClient(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QWebSocket::connected, this, &SkypeClient::onConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &SkypeClient::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &SkypeClient::onTextMessage);
    connect(&m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &SkypeClient::onError);
}

void SkypeClient::connectToServer(const QString& host, quint16 port) {
    QString url = QString("ws://%1:%2").arg(host).arg(port);
    m_socket.open(QUrl(url));
}

void SkypeClient::login(const QString& username, const QString& password) {
    m_username = username;
    sendJson({{"type", "login"}, {"username", username}, {"password", password}});
}

void SkypeClient::sendMessage(const QString& to, const QString& text) {
    sendJson({{"type", "message"}, {"to", to}, {"text", text}});
}

void SkypeClient::addContact(const QString& contactName) {
    sendJson({{"type", "add_contact"}, {"contact", contactName}});
}

void SkypeClient::setStatus(const QString& status) {
    sendJson({{"type", "status"}, {"status", status}});
}

void SkypeClient::requestContacts() {
    sendJson({{"type", "get_contacts"}});
}

bool SkypeClient::isConnected() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void SkypeClient::onConnected() {
    qDebug() << "Connected to server";
    emit connected();
}

void SkypeClient::onDisconnected() {
    qDebug() << "Disconnected from server";
    emit disconnected();
}

void SkypeClient::onTextMessage(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "login_result") {
        bool success = obj["success"].toBool();
        QString error = obj["error"].toString();
        emit loginResult(success, error);
    } else if (type == "contact_list") {
        emit contactListReceived(obj["contacts"].toArray());
    } else if (type == "message") {
        emit messageReceived(obj["from"].toString(), obj["text"].toString(),
                             obj["timestamp"].toString());
    } else if (type == "message_ack") {
        emit messageAcknowledged(obj["to"].toString(), obj["text"].toString());
    } else if (type == "presence") {
        emit presenceChanged(obj["username"].toString(), obj["status"].toString());
    } else if (type == "add_contact_result") {
        if (obj["success"].toBool()) {
            emit contactAdded(obj["contact"].toString());
        }
    }
}

void SkypeClient::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    emit connectionError(m_socket.errorString());
}

void SkypeClient::sendJson(const QJsonObject& obj) {
    m_socket.sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
