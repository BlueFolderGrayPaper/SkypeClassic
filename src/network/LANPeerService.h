#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QMap>
#include <QList>
#include <QHostAddress>
#include <QDateTime>

struct PeerInfo {
    QString username;
    QString status;
    QString skypeNumber;
    QHostAddress address;
    quint16 wsPort;
    qint64 lastSeen;
};

class LANPeerService : public QObject {
    Q_OBJECT

public:
    explicit LANPeerService(QObject* parent = nullptr);
    ~LANPeerService();

    bool start(const QString& username, quint16 discoveryPort = 33034);
    void stop();

    void sendMessage(const QString& to, const QString& text);
    void sendTyping(const QString& to);
    void sendFileOffer(const QString& to, const QString& fileName, qint64 fileSize);
    void sendFileData(const QString& to, const QString& fileName, const QByteArray& data);
    void sendCallOffer(const QString& to, const QString& callId);
    void sendCallAccept(const QString& to, const QString& callId);
    void sendCallReject(const QString& to, const QString& callId);
    void sendCallEnd(const QString& to, const QString& callId);
    void sendAudioData(const QString& to, const QByteArray& audioData);
    void sendVideoData(const QString& to, const QByteArray& jpegData);
    void sendContactShare(const QString& to, const QString& contactName, const QString& skypeName, const QString& skypeNumber);
    void sendGroupCreate(const QString& to, const QString& groupId, const QString& groupName, const QStringList& members);
    void sendGroupMessage(const QString& to, const QString& groupId, const QString& text);
    void sendGroupTyping(const QString& to, const QString& groupId);
    void sendGroupInvite(const QString& to, const QString& groupId, const QString& groupName, const QStringList& members);
    void sendGroupLeave(const QString& to, const QString& groupId);
    void sendConferenceCreate(const QString& to, const QString& conferenceId, const QStringList& participants);
    void sendConferenceJoin(const QString& to, const QString& conferenceId);
    void sendConferenceLeave(const QString& to, const QString& conferenceId);
    void sendConferenceAudio(const QStringList& participants, const QString& conferenceId, const QByteArray& audioData);
    void sendConferenceVideo(const QStringList& participants, const QString& conferenceId, const QByteArray& jpegData);
    void setStatus(const QString& status);
    void addContact(const QString& contactName);
    void requestContacts();

    bool isRunning() const;

signals:
    void connected();
    void disconnected();
    void loginResult(bool success, const QString& error);
    void contactListReceived(const QJsonArray& contacts);
    void messageReceived(const QString& from, const QString& text, const QString& timestamp);
    void messageAcknowledged(const QString& to, const QString& text);
    void typingReceived(const QString& from);
    void presenceChanged(const QString& username, const QString& status);
    void fileOfferReceived(const QString& from, const QString& fileName, qint64 fileSize);
    void fileDataReceived(const QString& from, const QString& fileName, const QByteArray& data);
    void callOfferReceived(const QString& from, const QString& callId);
    void callAcceptReceived(const QString& from, const QString& callId);
    void callRejectReceived(const QString& from, const QString& callId);
    void callEndReceived(const QString& from, const QString& callId);
    void audioDataReceived(const QString& from, const QByteArray& data);
    void videoDataReceived(const QString& from, const QByteArray& jpegData);
    void conferenceCreateReceived(const QString& from, const QString& conferenceId, const QStringList& participants);
    void conferenceJoinReceived(const QString& from, const QString& conferenceId);
    void conferenceLeaveReceived(const QString& from, const QString& conferenceId);
    void contactShareReceived(const QString& from, const QString& contactName, const QString& skypeName, const QString& skypeNumber);
    void groupCreateReceived(const QString& from, const QString& groupId, const QString& groupName, const QStringList& members);
    void groupMessageReceived(const QString& from, const QString& groupId, const QString& text);
    void groupTypingReceived(const QString& from, const QString& groupId);
    void groupInviteReceived(const QString& from, const QString& groupId, const QString& groupName, const QStringList& members);
    void groupLeaveReceived(const QString& from, const QString& groupId);
    void conferenceAudioReceived(const QString& from, const QString& conferenceId, const QByteArray& data);
    void conferenceVideoReceived(const QString& from, const QString& conferenceId, const QByteArray& jpegData);
    void contactAdded(const QString& contact);
    void connectionError(const QString& error);

private slots:
    void onDiscoveryReadyRead();
    void onHeartbeatTimer();
    void onPeerTimeoutCheck();
    void onNewPeerConnection();
    void onPeerTextMessage(const QString& message);
    void onPeerBinaryMessage(const QByteArray& data);
    void onPeerDisconnected();

private:
    void broadcastPresence();
    void handleDiscoveryPacket(const QByteArray& data, const QHostAddress& sender);
    QWebSocket* getOrCreateConnection(const QString& peerUsername);
    void sendJsonToPeer(const QString& peerUsername, const QJsonObject& obj);
    void flushPendingMessages(const QString& peerUsername);
    QJsonArray buildContactArray() const;
    void emitContactList();

    QString m_username;
    QString m_status;
    QString m_skypeNumber;
    bool m_running = false;

    // UDP discovery
    QUdpSocket* m_discoverySocket = nullptr;
    quint16 m_discoveryPort = 33034;
    QTimer* m_heartbeatTimer = nullptr;
    QTimer* m_timeoutTimer = nullptr;

    // WebSocket server for incoming peer connections
    QWebSocketServer* m_wsServer = nullptr;
    quint16 m_wsListenPort = 0;

    // Peer tracking
    QMap<QString, PeerInfo> m_peers;
    QMap<QString, QWebSocket*> m_outgoingConnections;
    QList<QWebSocket*> m_incomingConnections;
    QMap<QWebSocket*, QString> m_socketToUsername;

    // Message queue for connections still establishing
    QMap<QString, QList<QJsonObject>> m_pendingMessages;

    // Rate limiting: peer username -> list of message timestamps
    QMap<QString, QList<qint64>> m_rateLimitMap;
    bool checkRateLimit(const QString& peer);
};
