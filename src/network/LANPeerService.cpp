#include "network/LANPeerService.h"

#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QDebug>
#include <QSettings>
#include <QRandomGenerator>

LANPeerService::LANPeerService(QObject* parent)
    : QObject(parent)
{
}

LANPeerService::~LANPeerService() {
    stop();
}

bool LANPeerService::start(const QString& username, quint16 discoveryPort) {
    if (m_running) return true;

    m_username = username;
    m_status = "Online";
    m_discoveryPort = discoveryPort;

    // Generate or load Skype Number
    QSettings settings("SkypeClassic", "SkypeClassic");
    m_skypeNumber = settings.value("account/skypeNumber").toString();
    if (m_skypeNumber.isEmpty()) {
        int num = QRandomGenerator::global()->bounded(10000, 99999);
        m_skypeNumber = QString("SKP-%1").arg(num, 5, 10, QChar('0'));
        settings.setValue("account/skypeNumber", m_skypeNumber);
    }

    // Bind UDP socket for discovery (multicast for cross-subnet)
    m_discoverySocket = new QUdpSocket(this);
    if (!m_discoverySocket->bind(QHostAddress::AnyIPv4, m_discoveryPort,
                                  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning() << "Failed to bind UDP discovery port" << m_discoveryPort
                    << m_discoverySocket->errorString();
        delete m_discoverySocket;
        m_discoverySocket = nullptr;
        return false;
    }
    // Join multicast group so discovery works across subnets
    m_discoverySocket->joinMulticastGroup(QHostAddress("239.77.83.75"));
    connect(m_discoverySocket, &QUdpSocket::readyRead,
            this, &LANPeerService::onDiscoveryReadyRead);

    // Start WebSocket server on random port
    m_wsServer = new QWebSocketServer("SkypeP2P", QWebSocketServer::NonSecureMode, this);
    if (!m_wsServer->listen(QHostAddress::Any, 0)) {
        qWarning() << "Failed to start P2P WebSocket server" << m_wsServer->errorString();
        delete m_discoverySocket;
        m_discoverySocket = nullptr;
        delete m_wsServer;
        m_wsServer = nullptr;
        return false;
    }
    m_wsListenPort = m_wsServer->serverPort();
    connect(m_wsServer, &QWebSocketServer::newConnection,
            this, &LANPeerService::onNewPeerConnection);

    qDebug() << "P2P started: UDP" << m_discoveryPort << "WS" << m_wsListenPort;

    // Heartbeat timer — broadcast presence every 5s
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &LANPeerService::onHeartbeatTimer);
    m_heartbeatTimer->start(5000);

    // Timeout timer — check for stale peers every 5s
    m_timeoutTimer = new QTimer(this);
    connect(m_timeoutTimer, &QTimer::timeout, this, &LANPeerService::onPeerTimeoutCheck);
    m_timeoutTimer->start(5000);

    m_running = true;

    // Broadcast immediately so peers discover us right away
    broadcastPresence();

    // P2P login always succeeds
    emit loginResult(true, "");
    emit connected();

    return true;
}

void LANPeerService::stop() {
    if (!m_running) return;
    m_running = false;

    // Best-effort offline broadcast
    if (m_discoverySocket) {
        m_status = "Offline";
        broadcastPresence();
    }

    if (m_heartbeatTimer) { m_heartbeatTimer->stop(); delete m_heartbeatTimer; m_heartbeatTimer = nullptr; }
    if (m_timeoutTimer) { m_timeoutTimer->stop(); delete m_timeoutTimer; m_timeoutTimer = nullptr; }

    // Close outgoing connections
    for (auto* ws : m_outgoingConnections) {
        ws->close();
        ws->deleteLater();
    }
    m_outgoingConnections.clear();

    // Close incoming connections
    for (auto* ws : m_incomingConnections) {
        ws->close();
        ws->deleteLater();
    }
    m_incomingConnections.clear();

    m_socketToUsername.clear();
    m_peers.clear();
    m_pendingMessages.clear();

    if (m_wsServer) { m_wsServer->close(); delete m_wsServer; m_wsServer = nullptr; }
    if (m_discoverySocket) { m_discoverySocket->close(); delete m_discoverySocket; m_discoverySocket = nullptr; }

    emit disconnected();
}

bool LANPeerService::isRunning() const {
    return m_running;
}

void LANPeerService::addManualPeer(const QHostAddress& address, quint16 wsPort) {
    // Send a direct discovery packet to a specific IP instead of relying on broadcast
    if (!m_discoverySocket || !m_running) return;

    QJsonObject packet;
    packet["type"] = "discovery";
    packet["username"] = m_username;
    packet["status"] = m_status;
    packet["wsPort"] = static_cast<int>(m_wsListenPort);
    packet["skypeNumber"] = m_skypeNumber;

    QByteArray data = QJsonDocument(packet).toJson(QJsonDocument::Compact);
    m_discoverySocket->writeDatagram(data, address, m_discoveryPort);
    qDebug() << "Sent manual discovery to" << address.toString() << ":" << wsPort;
}

// === UDP Discovery ===

void LANPeerService::broadcastPresence() {
    if (!m_discoverySocket) return;

    QJsonObject packet;
    packet["type"] = "discovery";
    packet["username"] = m_username;
    packet["status"] = m_status;
    packet["wsPort"] = static_cast<int>(m_wsListenPort);
    packet["skypeNumber"] = m_skypeNumber;

    QByteArray data = QJsonDocument(packet).toJson(QJsonDocument::Compact);
    // Send to both multicast (cross-subnet) and broadcast (same-subnet fallback)
    m_discoverySocket->writeDatagram(data, QHostAddress("239.77.83.75"), m_discoveryPort);
    m_discoverySocket->writeDatagram(data, QHostAddress::Broadcast, m_discoveryPort);
}

void LANPeerService::onDiscoveryReadyRead() {
    while (m_discoverySocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_discoverySocket->receiveDatagram();
        handleDiscoveryPacket(datagram.data(), datagram.senderAddress());
    }
}

void LANPeerService::handleDiscoveryPacket(const QByteArray& data, const QHostAddress& sender) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (obj["type"].toString() != "discovery") return;

    QString username = obj["username"].toString();
    QString status = obj["status"].toString();
    quint16 wsPort = static_cast<quint16>(obj["wsPort"].toInt());
    QString skypeNumber = obj["skypeNumber"].toString();

    // Ignore our own broadcasts
    if (username == m_username) return;

    // Normalize IPv4-mapped IPv6 addresses (::ffff:x.x.x.x -> x.x.x.x)
    QHostAddress normalizedSender = sender;
    bool ok;
    quint32 ipv4 = sender.toIPv4Address(&ok);
    if (ok) {
        normalizedSender = QHostAddress(ipv4);
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool isNew = !m_peers.contains(username);
    bool statusChanged = false;

    if (isNew) {
        PeerInfo info;
        info.username = username;
        info.status = status;
        info.skypeNumber = skypeNumber;
        info.address = normalizedSender;
        info.wsPort = wsPort;
        info.lastSeen = now;
        m_peers.insert(username, info);
        statusChanged = true;
        qDebug() << "Discovered peer:" << username << "at" << normalizedSender.toString() << ":" << wsPort;
    } else {
        PeerInfo& info = m_peers[username];
        if (info.status != status) {
            statusChanged = true;
        }
        info.status = status;
        info.skypeNumber = skypeNumber;
        info.address = normalizedSender;
        info.wsPort = wsPort;
        info.lastSeen = now;
    }

    if (statusChanged) {
        emit presenceChanged(username, status);
        emitContactList();
    }
}

void LANPeerService::onHeartbeatTimer() {
    broadcastPresence();
}

void LANPeerService::onPeerTimeoutCheck() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList timedOut;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (now - it->lastSeen > 15000) {
            timedOut.append(it.key());
        }
    }

    for (const QString& username : timedOut) {
        qDebug() << "Peer timed out:" << username;
        m_peers.remove(username);

        if (m_outgoingConnections.contains(username)) {
            m_outgoingConnections[username]->close();
            m_outgoingConnections[username]->deleteLater();
            m_outgoingConnections.remove(username);
        }
        m_pendingMessages.remove(username);

        emit presenceChanged(username, "Offline");
    }

    if (!timedOut.isEmpty()) {
        emitContactList();
    }
}

// === WebSocket Server (incoming peer connections) ===

void LANPeerService::onNewPeerConnection() {
    while (m_wsServer->hasPendingConnections()) {
        QWebSocket* socket = m_wsServer->nextPendingConnection();
        m_incomingConnections.append(socket);
        connect(socket, &QWebSocket::textMessageReceived,
                this, &LANPeerService::onPeerTextMessage);
        connect(socket, &QWebSocket::binaryMessageReceived,
                this, &LANPeerService::onPeerBinaryMessage);
        connect(socket, &QWebSocket::disconnected,
                this, &LANPeerService::onPeerDisconnected);
    }
}

bool LANPeerService::checkRateLimit(const QString& peer) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto& timestamps = m_rateLimitMap[peer];
    while (!timestamps.isEmpty() && now - timestamps.first() > 60000)
        timestamps.removeFirst();
    timestamps.append(now);
    return timestamps.size() <= 30;
}

void LANPeerService::onPeerTextMessage(const QString& message) {
    // Message size limit: 64KB
    if (message.size() > 65536) {
        qWarning() << "Dropping oversized message:" << message.size() << "bytes";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "identify") {
        auto* socket = qobject_cast<QWebSocket*>(sender());
        if (socket) {
            QString username = obj["username"].toString();
            if (username.isEmpty()) {
                qWarning() << "Empty username in identify message";
                socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Empty username");
                return;
            }
            // Accept the identity — IP verification is too strict on LAN
            // (loopback, IPv4-mapped IPv6, multi-homed hosts all cause mismatches)
            if (m_peers.contains(username)) {
                qDebug() << "Peer identified:" << username;
            } else {
                qDebug() << "Peer identified (not yet discovered via UDP):" << username;
            }
            m_socketToUsername[socket] = username;
        }
    } else {
        // For all other message types, verify sender matches socket identity
        auto* socket = qobject_cast<QWebSocket*>(sender());
        QString claimedFrom = obj["from"].toString();
        if (socket && m_socketToUsername.contains(socket)) {
            if (m_socketToUsername[socket] != claimedFrom) {
                qWarning() << "Sender mismatch: claimed" << claimedFrom
                           << "but socket identified as" << m_socketToUsername[socket];
                return;
            }
        }

        // Rate limiting
        if (!claimedFrom.isEmpty() && !checkRateLimit(claimedFrom)) {
            qWarning() << "Rate limit exceeded for" << claimedFrom;
            return;
        }

        if (type == "message") {
            QString from = obj["from"].toString();
            QString text = obj["text"].toString();
            QString timestamp = obj["timestamp"].toString();
            emit messageReceived(from, text, timestamp);

            if (socket) {
                QJsonObject ack;
                ack["type"] = "message_ack";
                ack["from"] = m_username;
                ack["text"] = text;
                socket->sendTextMessage(QJsonDocument(ack).toJson(QJsonDocument::Compact));
            }
        } else if (type == "message_ack") {
            emit messageAcknowledged(obj["from"].toString(), obj["text"].toString());
        } else if (type == "typing") {
            emit typingReceived(obj["from"].toString());
        } else if (type == "file_offer") {
            QString from = obj["from"].toString();
            QString fileName = obj["fileName"].toString();
            qint64 fileSize = static_cast<qint64>(obj["fileSize"].toDouble());
            emit fileOfferReceived(from, fileName, fileSize);
        } else if (type == "file_data") {
            QString from = obj["from"].toString();
            QString fileName = obj["fileName"].toString();
            QByteArray data = QByteArray::fromBase64(obj["data"].toString().toLatin1());
            emit fileDataReceived(from, fileName, data);
        } else if (type == "call_offer") {
            emit callOfferReceived(obj["from"].toString(), obj["callId"].toString());
        } else if (type == "call_accept") {
            emit callAcceptReceived(obj["from"].toString(), obj["callId"].toString());
        } else if (type == "call_reject") {
            emit callRejectReceived(obj["from"].toString(), obj["callId"].toString());
        } else if (type == "call_end") {
            emit callEndReceived(obj["from"].toString(), obj["callId"].toString());
        } else if (type == "group_create") {
            QStringList members;
            for (auto v : obj["members"].toArray()) members.append(v.toString());
            emit groupCreateReceived(obj["from"].toString(), obj["groupId"].toString(),
                obj["groupName"].toString(), members);
        } else if (type == "group_message") {
            emit groupMessageReceived(obj["from"].toString(), obj["groupId"].toString(),
                obj["text"].toString());
        } else if (type == "group_typing") {
            emit groupTypingReceived(obj["from"].toString(), obj["groupId"].toString());
        } else if (type == "group_invite") {
            QStringList members;
            for (auto v : obj["members"].toArray()) members.append(v.toString());
            emit groupInviteReceived(obj["from"].toString(), obj["groupId"].toString(),
                obj["groupName"].toString(), members);
        } else if (type == "group_leave") {
            emit groupLeaveReceived(obj["from"].toString(), obj["groupId"].toString());
        } else if (type == "contact_share") {
            QJsonObject shared = obj["sharedContact"].toObject();
            emit contactShareReceived(obj["from"].toString(),
                shared["displayName"].toString(),
                shared["skypeName"].toString(),
                shared["skypeNumber"].toString());
        } else if (type == "conf_create") {
            QStringList participants;
            for (auto v : obj["participants"].toArray())
                participants.append(v.toString());
            emit conferenceCreateReceived(obj["from"].toString(), obj["conferenceId"].toString(), participants);
        } else if (type == "conf_join") {
            emit conferenceJoinReceived(obj["from"].toString(), obj["conferenceId"].toString());
        } else if (type == "conf_leave") {
            emit conferenceLeaveReceived(obj["from"].toString(), obj["conferenceId"].toString());
        }
    }
}

void LANPeerService::onPeerDisconnected() {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    m_incomingConnections.removeAll(socket);
    m_socketToUsername.remove(socket);

    // Check if it was an outgoing connection
    for (auto it = m_outgoingConnections.begin(); it != m_outgoingConnections.end(); ++it) {
        if (it.value() == socket) {
            m_outgoingConnections.erase(it);
            break;
        }
    }

    socket->deleteLater();
}

// === Outgoing connections + messaging ===

QWebSocket* LANPeerService::getOrCreateConnection(const QString& peerUsername) {
    // Reuse existing connection if open
    if (m_outgoingConnections.contains(peerUsername)) {
        QWebSocket* ws = m_outgoingConnections[peerUsername];
        if (ws->state() == QAbstractSocket::ConnectedState ||
            ws->state() == QAbstractSocket::ConnectingState) {
            return ws;
        }
        // Dead connection, clean up
        ws->deleteLater();
        m_outgoingConnections.remove(peerUsername);
    }

    if (!m_peers.contains(peerUsername)) return nullptr;

    const PeerInfo& peer = m_peers[peerUsername];
    auto* ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(ws, &QWebSocket::connected, [this, peerUsername, ws]() {
        // Identify ourselves to the peer
        QJsonObject identify;
        identify["type"] = "identify";
        identify["username"] = m_username;
        ws->sendTextMessage(QJsonDocument(identify).toJson(QJsonDocument::Compact));

        // Flush any queued messages
        flushPendingMessages(peerUsername);
    });

    connect(ws, &QWebSocket::textMessageReceived,
            this, &LANPeerService::onPeerTextMessage);
    connect(ws, &QWebSocket::binaryMessageReceived,
            this, &LANPeerService::onPeerBinaryMessage);
    connect(ws, &QWebSocket::disconnected,
            this, &LANPeerService::onPeerDisconnected);
    connect(ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            [this, peerUsername](QAbstractSocket::SocketError error) {
        Q_UNUSED(error);
        qDebug() << "Connection error to peer" << peerUsername;
    });

    QString url = QString("ws://%1:%2").arg(peer.address.toString()).arg(peer.wsPort);
    ws->open(QUrl(url));
    m_outgoingConnections.insert(peerUsername, ws);

    return ws;
}

void LANPeerService::sendTyping(const QString& to) {
    if (!m_peers.contains(to)) return;

    QJsonObject msg;
    msg["type"] = "typing";
    msg["from"] = m_username;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendFileData(const QString& to, const QString& fileName, const QByteArray& data) {
    if (!m_peers.contains(to)) return;

    QJsonObject msg;
    msg["type"] = "file_data";
    msg["from"] = m_username;
    msg["fileName"] = fileName;
    msg["fileSize"] = static_cast<double>(data.size());
    msg["data"] = QString::fromLatin1(data.toBase64());
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendCallOffer(const QString& to, const QString& callId) {
    if (!m_peers.contains(to)) {
        qWarning() << "Cannot send call offer: peer" << to << "not discovered."
                    << "Known peers:" << m_peers.keys();
        return;
    }

    qDebug() << "Sending call_offer to" << to << "callId:" << callId;
    QJsonObject msg;
    msg["type"] = "call_offer";
    msg["from"] = m_username;
    msg["callId"] = callId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendCallAccept(const QString& to, const QString& callId) {
    qDebug() << "Sending call_accept to" << to << "callId:" << callId;
    QJsonObject msg;
    msg["type"] = "call_accept";
    msg["from"] = m_username;
    msg["callId"] = callId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendCallReject(const QString& to, const QString& callId) {
    qDebug() << "Sending call_reject to" << to << "callId:" << callId;
    QJsonObject msg;
    msg["type"] = "call_reject";
    msg["from"] = m_username;
    msg["callId"] = callId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendCallEnd(const QString& to, const QString& callId) {
    qDebug() << "Sending call_end to" << to << "callId:" << callId;
    QJsonObject msg;
    msg["type"] = "call_end";
    msg["from"] = m_username;
    msg["callId"] = callId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendAudioData(const QString& to, const QByteArray& audioData) {
    if (!m_peers.contains(to)) return;

    QWebSocket* ws = getOrCreateConnection(to);
    if (!ws || ws->state() != QAbstractSocket::ConnectedState) return;

    // Binary frame: "AUD\0" + 4-byte seq + PCM data
    static uint32_t seq = 0;
    QByteArray frame;
    frame.append("AUD", 3);
    frame.append('\0');
    uint32_t s = seq++;
    frame.append(reinterpret_cast<const char*>(&s), 4);
    frame.append(audioData);
    ws->sendBinaryMessage(frame);
}

void LANPeerService::sendVideoData(const QString& to, const QByteArray& jpegData) {
    if (!m_peers.contains(to)) return;

    QWebSocket* ws = getOrCreateConnection(to);
    if (!ws || ws->state() != QAbstractSocket::ConnectedState) return;

    // Binary frame: "VID\0" + 4-byte seq + JPEG data
    static uint32_t vseq = 0;
    QByteArray frame;
    frame.append("VID", 3);
    frame.append('\0');
    uint32_t s = vseq++;
    frame.append(reinterpret_cast<const char*>(&s), 4);
    frame.append(jpegData);
    ws->sendBinaryMessage(frame);
}

void LANPeerService::onPeerBinaryMessage(const QByteArray& data) {
    if (data.size() < 8) return;

    auto* socket = qobject_cast<QWebSocket*>(sender());
    QString from;
    if (socket && m_socketToUsername.contains(socket)) {
        from = m_socketToUsername[socket];
    }
    if (from.isEmpty()) return;

    // Check magic header
    if (data[0] == 'A' && data[1] == 'U' && data[2] == 'D') {
        QByteArray audioData = data.mid(8);
        emit audioDataReceived(from, audioData);
    } else if (data[0] == 'V' && data[1] == 'I' && data[2] == 'D') {
        QByteArray jpegData = data.mid(8);
        emit videoDataReceived(from, jpegData);
    } else if (data[0] == 'C' && data[1] == 'A' && data[2] == 'U') {
        // Conference audio: CAU\0 + 4-byte seq + 36-byte confId + audio
        if (data.size() < 48) return;
        QString confId = QString::fromUtf8(data.mid(8, 36));
        QByteArray audioData = data.mid(44);
        emit conferenceAudioReceived(from, confId, audioData);
    } else if (data[0] == 'C' && data[1] == 'V' && data[2] == 'D') {
        // Conference video: CVD\0 + 4-byte seq + 36-byte confId + jpeg
        if (data.size() < 48) return;
        QString confId = QString::fromUtf8(data.mid(8, 36));
        QByteArray jpegData = data.mid(44);
        emit conferenceVideoReceived(from, confId, jpegData);
    }
}

void LANPeerService::sendContactShare(const QString& to, const QString& contactName, const QString& skypeName, const QString& skypeNumber) {
    QJsonObject msg;
    msg["type"] = "contact_share";
    msg["from"] = m_username;
    QJsonObject shared;
    shared["displayName"] = contactName;
    shared["skypeName"] = skypeName;
    shared["skypeNumber"] = skypeNumber;
    msg["sharedContact"] = shared;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendGroupCreate(const QString& to, const QString& groupId, const QString& groupName, const QStringList& members) {
    QJsonObject msg;
    msg["type"] = "group_create";
    msg["from"] = m_username;
    msg["groupId"] = groupId;
    msg["groupName"] = groupName;
    QJsonArray arr;
    for (const QString& m : members) arr.append(m);
    msg["members"] = arr;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendGroupMessage(const QString& to, const QString& groupId, const QString& text) {
    QJsonObject msg;
    msg["type"] = "group_message";
    msg["from"] = m_username;
    msg["groupId"] = groupId;
    msg["text"] = text;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendGroupTyping(const QString& to, const QString& groupId) {
    QJsonObject msg;
    msg["type"] = "group_typing";
    msg["from"] = m_username;
    msg["groupId"] = groupId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendGroupInvite(const QString& to, const QString& groupId, const QString& groupName, const QStringList& members) {
    QJsonObject msg;
    msg["type"] = "group_invite";
    msg["from"] = m_username;
    msg["groupId"] = groupId;
    msg["groupName"] = groupName;
    QJsonArray arr;
    for (const QString& m : members) arr.append(m);
    msg["members"] = arr;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendGroupLeave(const QString& to, const QString& groupId) {
    QJsonObject msg;
    msg["type"] = "group_leave";
    msg["from"] = m_username;
    msg["groupId"] = groupId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendConferenceCreate(const QString& to, const QString& conferenceId, const QStringList& participants) {
    QJsonObject msg;
    msg["type"] = "conf_create";
    msg["from"] = m_username;
    msg["conferenceId"] = conferenceId;
    QJsonArray arr;
    for (const QString& p : participants) arr.append(p);
    msg["participants"] = arr;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendConferenceJoin(const QString& to, const QString& conferenceId) {
    QJsonObject msg;
    msg["type"] = "conf_join";
    msg["from"] = m_username;
    msg["conferenceId"] = conferenceId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendConferenceLeave(const QString& to, const QString& conferenceId) {
    QJsonObject msg;
    msg["type"] = "conf_leave";
    msg["from"] = m_username;
    msg["conferenceId"] = conferenceId;
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendConferenceAudio(const QStringList& participants, const QString& conferenceId, const QByteArray& audioData) {
    static uint32_t caseq = 0;
    QByteArray confIdBytes = conferenceId.toUtf8().left(36);
    confIdBytes = confIdBytes.leftJustified(36, '\0');

    QByteArray frame;
    frame.append("CAU", 3);
    frame.append('\0');
    uint32_t s = caseq++;
    frame.append(reinterpret_cast<const char*>(&s), 4);
    frame.append(confIdBytes);
    frame.append(audioData);

    for (const QString& p : participants) {
        if (p == m_username) continue;
        if (!m_peers.contains(p)) continue;
        QWebSocket* ws = getOrCreateConnection(p);
        if (ws && ws->state() == QAbstractSocket::ConnectedState) {
            ws->sendBinaryMessage(frame);
        }
    }
}

void LANPeerService::sendConferenceVideo(const QStringList& participants, const QString& conferenceId, const QByteArray& jpegData) {
    static uint32_t cvseq = 0;
    QByteArray confIdBytes = conferenceId.toUtf8().left(36);
    confIdBytes = confIdBytes.leftJustified(36, '\0');

    QByteArray frame;
    frame.append("CVD", 3);
    frame.append('\0');
    uint32_t s = cvseq++;
    frame.append(reinterpret_cast<const char*>(&s), 4);
    frame.append(confIdBytes);
    frame.append(jpegData);

    for (const QString& p : participants) {
        if (p == m_username) continue;
        if (!m_peers.contains(p)) continue;
        QWebSocket* ws = getOrCreateConnection(p);
        if (ws && ws->state() == QAbstractSocket::ConnectedState) {
            ws->sendBinaryMessage(frame);
        }
    }
}

void LANPeerService::sendFileOffer(const QString& to, const QString& fileName, qint64 fileSize) {
    if (!m_peers.contains(to)) return;

    QJsonObject msg;
    msg["type"] = "file_offer";
    msg["from"] = m_username;
    msg["fileName"] = fileName;
    msg["fileSize"] = static_cast<double>(fileSize);
    sendJsonToPeer(to, msg);
}

void LANPeerService::sendMessage(const QString& to, const QString& text) {
    if (!m_peers.contains(to)) {
        qDebug() << "Cannot send message: peer" << to << "not found";
        return;
    }

    QJsonObject msg;
    msg["type"] = "message";
    msg["from"] = m_username;
    msg["text"] = text;
    msg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    sendJsonToPeer(to, msg);
}

void LANPeerService::sendJsonToPeer(const QString& peerUsername, const QJsonObject& obj) {
    QWebSocket* ws = getOrCreateConnection(peerUsername);
    if (!ws) return;

    if (ws->state() == QAbstractSocket::ConnectedState) {
        ws->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } else {
        // Queue for when connection establishes
        m_pendingMessages[peerUsername].append(obj);
    }
}

void LANPeerService::flushPendingMessages(const QString& peerUsername) {
    if (!m_pendingMessages.contains(peerUsername)) return;
    if (!m_outgoingConnections.contains(peerUsername)) return;

    QWebSocket* ws = m_outgoingConnections[peerUsername];
    if (ws->state() != QAbstractSocket::ConnectedState) return;

    for (const QJsonObject& msg : m_pendingMessages[peerUsername]) {
        ws->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }
    m_pendingMessages.remove(peerUsername);
}

// === Public API ===

void LANPeerService::setStatus(const QString& status) {
    m_status = status;
    broadcastPresence();
}

void LANPeerService::addContact(const QString& contactName) {
    // In P2P mode, contacts are discovered automatically
    if (m_peers.contains(contactName)) {
        emit contactAdded(contactName);
    }
}

void LANPeerService::requestContacts() {
    emitContactList();
}

QJsonArray LANPeerService::buildContactArray() const {
    QJsonArray arr;
    for (auto it = m_peers.constBegin(); it != m_peers.constEnd(); ++it) {
        QJsonObject obj;
        obj["username"] = it->username;
        obj["displayName"] = it->username;
        obj["status"] = it->status;
        obj["skypeNumber"] = it->skypeNumber;
        arr.append(obj);
    }
    return arr;
}

void LANPeerService::emitContactList() {
    emit contactListReceived(buildContactArray());
}
