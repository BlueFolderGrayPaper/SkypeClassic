#pragma once

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QMap>
#include <QList>
#include <QSet>

struct ServerUser {
    QString username;
    QString password;
    QStringList contacts;
};

struct ConnectedClient {
    QWebSocket* socket;
    QString username;
};

class ChatServer : public QObject {
    Q_OBJECT

public:
    explicit ChatServer(quint16 port, QObject* parent = nullptr);
    ~ChatServer();

    bool start();

private slots:
    void onNewConnection();
    void onTextMessage(const QString& message);
    void onDisconnected();

private:
    void handleLogin(QWebSocket* socket, const QJsonObject& data);
    void handleRegister(QWebSocket* socket, const QJsonObject& data);
    void handleMessage(QWebSocket* socket, const QJsonObject& data);
    void handleContactList(QWebSocket* socket);
    void handleAddContact(QWebSocket* socket, const QJsonObject& data);
    void handleStatusChange(QWebSocket* socket, const QJsonObject& data);

    void sendJson(QWebSocket* socket, const QJsonObject& obj);
    void broadcastPresence(const QString& username, const QString& status);
    QWebSocket* findClientSocket(const QString& username);

    QWebSocketServer* m_server;
    QList<ConnectedClient> m_clients;
    QMap<QString, ServerUser> m_users;     // username -> user data
    QMap<QString, QString> m_onlineStatus; // username -> status string
};
