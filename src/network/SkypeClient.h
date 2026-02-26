#pragma once

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include "models/Contact.h"

class SkypeClient : public QObject {
    Q_OBJECT

public:
    explicit SkypeClient(QObject* parent = nullptr);

    void connectToServer(const QString& host, quint16 port = 33033);
    void login(const QString& username, const QString& password);
    void sendMessage(const QString& to, const QString& text);
    void addContact(const QString& contactName);
    void setStatus(const QString& status);
    void requestContacts();

    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void loginResult(bool success, const QString& error);
    void contactListReceived(const QJsonArray& contacts);
    void messageReceived(const QString& from, const QString& text, const QString& timestamp);
    void messageAcknowledged(const QString& to, const QString& text);
    void presenceChanged(const QString& username, const QString& status);
    void contactAdded(const QString& contact);
    void connectionError(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessage(const QString& message);
    void onError(QAbstractSocket::SocketError error);

private:
    void sendJson(const QJsonObject& obj);

    QWebSocket m_socket;
    QString m_username;
};
