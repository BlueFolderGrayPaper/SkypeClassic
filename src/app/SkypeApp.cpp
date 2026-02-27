#include "app/SkypeApp.h"
#include "windows/FileTransferDialog.h"
#include "utils/SoundPlayer.h"

#include <QApplication>
#include <QRandomGenerator>
#include <QPixmap>
#include <QPainter>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QStatusBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>

SkypeApp::SkypeApp(QObject* parent)
    : QObject(parent)
    , m_client(new SkypeClient(this))
    , m_lanService(new LANPeerService(this))
    , m_conferenceManager(new ConferenceManager(this))
    , m_simulationTimer(new QTimer(this))
    , m_callSimTimer(new QTimer(this))
{
    m_mockResponses << "Hey! How are you?"
                     << "That sounds great!"
                     << "LOL :D"
                     << "Sure, let me check on that."
                     << "Can you call me later?"
                     << "I'm busy right now, talk later?"
                     << "Nice! Tell me more."
                     << "Haha, yeah totally."
                     << "Ok, I'll get back to you on that."
                     << "Sounds like a plan!"
                     << "Brb"
                     << ":)";

    connect(m_simulationTimer, &QTimer::timeout, this, &SkypeApp::simulateIncomingMessage);
    connect(m_callSimTimer, &QTimer::timeout, this, &SkypeApp::simulateIncomingCall);

    // Network signals (central server)
    connect(m_client, &SkypeClient::connected, this, &SkypeApp::onServerConnected);
    connect(m_client, &SkypeClient::disconnected, this, &SkypeApp::onServerDisconnected);
    connect(m_client, &SkypeClient::loginResult, this, &SkypeApp::onServerLoginResult);
    connect(m_client, &SkypeClient::contactListReceived, this, &SkypeApp::onServerContactList);
    connect(m_client, &SkypeClient::messageReceived, this, &SkypeApp::onServerMessage);
    connect(m_client, &SkypeClient::presenceChanged, this, &SkypeApp::onServerPresence);

    // P2P LAN signals (same slots — identical signal signatures)
    connect(m_lanService, &LANPeerService::loginResult, this, &SkypeApp::onServerLoginResult);
    connect(m_lanService, &LANPeerService::contactListReceived, this, &SkypeApp::onServerContactList);
    connect(m_lanService, &LANPeerService::messageReceived, this, &SkypeApp::onServerMessage);
    connect(m_lanService, &LANPeerService::presenceChanged, this, &SkypeApp::onServerPresence);
    connect(m_lanService, &LANPeerService::disconnected, this, &SkypeApp::onServerDisconnected);
    connect(m_lanService, &LANPeerService::typingReceived, [this](const QString& from) {
        Contact* contact = findContactByName(from);
        if (!contact) return;
        auto it = m_chatWindows.find(contact->id);
        if (it != m_chatWindows.end()) {
            (*it)->showTypingIndicator(from);
        }
    });

    // Call signaling (P2P)
    connect(m_lanService, &LANPeerService::callOfferReceived, this, &SkypeApp::onCallOfferReceived);
    connect(m_lanService, &LANPeerService::callAcceptReceived, this, &SkypeApp::onCallAcceptReceived);
    connect(m_lanService, &LANPeerService::callRejectReceived, this, &SkypeApp::onCallRejectReceived);
    connect(m_lanService, &LANPeerService::callEndReceived, this, &SkypeApp::onCallEndReceived);
    connect(m_lanService, &LANPeerService::audioDataReceived, this, &SkypeApp::onAudioDataReceived);
    connect(m_lanService, &LANPeerService::videoDataReceived, this, &SkypeApp::onVideoDataReceived);

    // Contact sharing
    connect(m_lanService, &LANPeerService::contactShareReceived, this, &SkypeApp::onContactShareReceived);

    // Group chat signals
    connect(m_lanService, &LANPeerService::groupCreateReceived, this, &SkypeApp::onGroupCreateReceived);
    connect(m_lanService, &LANPeerService::groupMessageReceived, this, &SkypeApp::onGroupMessageReceived);
    connect(m_lanService, &LANPeerService::groupTypingReceived, this, &SkypeApp::onGroupTypingReceived);
    connect(m_lanService, &LANPeerService::groupInviteReceived, this, &SkypeApp::onGroupInviteReceived);
    connect(m_lanService, &LANPeerService::groupLeaveReceived, this, &SkypeApp::onGroupLeaveReceived);

    // Conference signals
    connect(m_lanService, &LANPeerService::conferenceCreateReceived, this, &SkypeApp::onConferenceCreateReceived);
    connect(m_lanService, &LANPeerService::conferenceJoinReceived, this, &SkypeApp::onConferenceJoinReceived);
    connect(m_lanService, &LANPeerService::conferenceLeaveReceived, this, &SkypeApp::onConferenceLeaveReceived);
    connect(m_lanService, &LANPeerService::conferenceAudioReceived, this, &SkypeApp::onConferenceAudioReceived);
    connect(m_lanService, &LANPeerService::conferenceVideoReceived, this, &SkypeApp::onConferenceVideoReceived);

    connect(m_lanService, &LANPeerService::fileOfferReceived,
            [this](const QString& from, const QString& fileName, qint64 fileSize) {
        auto* ftDlg = new FileTransferDialog(from, fileName,
            FileTransferDialog::Receiving, m_mainWindow, QString(), fileSize);
        ftDlg->setAttribute(Qt::WA_DeleteOnClose);
        ftDlg->show();
        ftDlg->raise();
    });

    connect(m_lanService, &LANPeerService::fileDataReceived,
            [this](const QString& from, const QString& fileName, const QByteArray& data) {
        // Save to downloads dir
        QString dlDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/downloads";
        QDir().mkpath(dlDir);
        QString savePath = dlDir + "/" + fileName;

        // Avoid overwriting: append number if exists
        if (QFile::exists(savePath)) {
            QFileInfo fi(savePath);
            int n = 1;
            while (QFile::exists(savePath)) {
                savePath = dlDir + "/" + fi.completeBaseName() + QString("_%1.").arg(n) + fi.suffix();
                n++;
            }
        }

        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
        }

        // Show in chat window
        Contact* contact = findContactByName(from);
        if (contact) {
            auto* chatWin = findOrCreateChatWindow(contact->id);
            chatWin->receiveFileAttachment(from, fileName, savePath);
            chatWin->show();
            chatWin->raise();
        }
    });
}

SkypeApp::~SkypeApp() {
    delete m_trayIcon;
    delete m_trayMenu;
    delete m_loginWindow;
    delete m_mainWindow;
    qDeleteAll(m_chatWindows);
    qDeleteAll(m_callWindows);
    qDeleteAll(m_conferenceWindows);
    qDeleteAll(m_groupChatWindows);
}

void SkypeApp::setupSystemTray() {
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#0078D4"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 14, 14);
    QFont f("Arial", 9, QFont::Bold);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(pix.rect(), Qt::AlignCenter, "S");
    p.end();

    m_trayIcon = new QSystemTrayIcon(QIcon(pix));
    m_trayIcon->setToolTip(QString("Skype - %1 (Online)").arg(m_username));

    m_trayMenu = new QMenu();
    auto addTrayStatus = [this](const QString& label, const QString& statusStr) {
        m_trayMenu->addAction(label, [this, statusStr]() {
            if (m_serverMode) m_client->setStatus(statusStr);
            else if (m_p2pMode) m_lanService->setStatus(statusStr);
            m_trayIcon->setToolTip(QString("Skype - %1 (%2)").arg(m_username, statusStr));
        });
    };
    addTrayStatus("Online", "Online");
    addTrayStatus("Away", "Away");
    addTrayStatus("Not Available", "Not Available");
    addTrayStatus("Do Not Disturb", "Do Not Disturb");
    addTrayStatus("Invisible", "Invisible");
    addTrayStatus("Offline", "Offline");
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Open Skype", [this]() {
        if (m_mainWindow) {
            m_mainWindow->show();
            m_mainWindow->raise();
            m_mainWindow->activateWindow();
        }
    });
    m_trayMenu->addAction("Quit", [this]() { onQuitRequested(); });

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            if (m_mainWindow) {
                m_mainWindow->show();
                m_mainWindow->raise();
                m_mainWindow->activateWindow();
            }
        }
    });

    m_trayIcon->show();
}

void SkypeApp::start() {
    m_loginWindow = new LoginWindow();
    connect(m_loginWindow, &LoginWindow::loginRequested, this, &SkypeApp::onLoginRequested);
    m_loginWindow->show();
}

void SkypeApp::onLoginRequested(const QString& username) {
    m_username = username;
    m_password = ""; // LoginWindow doesn't expose password yet

    // Try to connect to server
    m_client->connectToServer("127.0.0.1", 33033);

    // Set a timeout — if server doesn't respond in 2 seconds, try P2P LAN mode
    QTimer::singleShot(2000, [this]() {
        if (!m_serverMode && !m_mainWindow) {
            qDebug() << "Server not available, trying P2P LAN mode";
            startP2PMode();
        }
    });
}

void SkypeApp::startP2PMode() {
    // Set m_p2pMode BEFORE start() because start() emits loginResult
    // synchronously, which triggers showMainWindow() — which checks m_p2pMode
    // for the status bar message.
    m_p2pMode = true;
    if (m_lanService->start(m_username)) {
        // loginResult signal will fire from LANPeerService,
        // triggering onServerLoginResult -> showMainWindow
    } else {
        m_p2pMode = false;
        // P2P failed, fall back to offline mock mode
        qDebug() << "P2P failed, starting in offline mode";
        m_contacts = Contact::createMockContacts();
        showMainWindow();
        m_simulationTimer->start(15000 + QRandomGenerator::global()->bounded(15000));
        m_callSimTimer->setSingleShot(true);
        m_callSimTimer->start(45000 + QRandomGenerator::global()->bounded(45000));
    }
}

void SkypeApp::showMainWindow() {
    m_loginWindow->hide();

    m_mainWindow = new MainWindow(m_username);
    m_mainWindow->setContacts(m_contacts);

    connect(m_mainWindow, &MainWindow::contactDoubleClicked,
            this, &SkypeApp::onContactDoubleClicked);
    connect(m_mainWindow, &MainWindow::callContact,
            this, &SkypeApp::onCallContact);
    connect(m_mainWindow, &MainWindow::quitRequested,
            this, &SkypeApp::onQuitRequested);

    connect(m_mainWindow, &MainWindow::statusChanged, [this](ContactStatus status) {
        QString statusStr;
        switch (status) {
            case ContactStatus::Online:       statusStr = "Online"; break;
            case ContactStatus::Away:         statusStr = "Away"; break;
            case ContactStatus::NotAvailable: statusStr = "Not Available"; break;
            case ContactStatus::DoNotDisturb: statusStr = "Do Not Disturb"; break;
            case ContactStatus::Invisible:    statusStr = "Invisible"; break;
            case ContactStatus::Offline:      statusStr = "Offline"; break;
        }
        if (m_serverMode) m_client->setStatus(statusStr);
        else if (m_p2pMode) m_lanService->setStatus(statusStr);

        if (m_trayIcon) {
            m_trayIcon->setToolTip(QString("Skype - %1 (%2)").arg(m_username, statusStr));
        }
    });

    connect(m_mainWindow, &MainWindow::contactAdded, [this](const QString& skypeName) {
        if (m_serverMode) m_client->addContact(skypeName);
        else if (m_p2pMode) m_lanService->addContact(skypeName);
    });

    connect(m_mainWindow, &MainWindow::conferenceCallRequested,
            this, &SkypeApp::onConferenceCallRequested);
    connect(m_mainWindow, &MainWindow::callSkypeNumber,
            this, &SkypeApp::onCallSkypeNumber);
    connect(m_mainWindow, &MainWindow::sendContactToRecipient,
            this, &SkypeApp::onSendContactToRecipient);
    connect(m_mainWindow, &MainWindow::createGroupChat,
            this, &SkypeApp::onCreateGroupChat);
    connect(m_mainWindow, &MainWindow::viewProfileRequested,
            this, &SkypeApp::onViewProfile);
    connect(m_mainWindow, &MainWindow::renameContact,
            this, &SkypeApp::onRenameContact);
    connect(m_mainWindow, &MainWindow::blockContact,
            this, &SkypeApp::onBlockContact);
    connect(m_mainWindow, &MainWindow::removeContact,
            this, &SkypeApp::onRemoveContact);

    connect(m_mainWindow, &MainWindow::profileUpdated, [this](const QString& displayName, const QString& moodText) {
        Q_UNUSED(moodText);
        if (!displayName.isEmpty()) {
            m_mainWindow->setWindowTitle(QString("Skype\u2122 - %1").arg(displayName));
            if (m_trayIcon) {
                // Preserve current status in tray tooltip
                QString tip = m_trayIcon->toolTip();
                int parenPos = tip.lastIndexOf('(');
                QString statusPart = (parenPos >= 0) ? tip.mid(parenPos) : "(Online)";
                m_trayIcon->setToolTip(QString("Skype - %1 %2").arg(displayName, statusPart));
            }
        }
    });

    connect(m_mainWindow, &MainWindow::sendFileToContact, [this](int contactId) {
        Contact* contact = findContact(contactId);
        if (!contact) return;
        QString file = QFileDialog::getOpenFileName(m_mainWindow, "Send File");
        if (!file.isEmpty()) {
            QFileInfo fi(file);
            auto* ftDlg = new FileTransferDialog(contact->displayName, QString(),
                FileTransferDialog::Sending, m_mainWindow, file);
            ftDlg->setAttribute(Qt::WA_DeleteOnClose);
            ftDlg->show();

            // Notify the peer so they get the receive prompt
            if (m_p2pMode) {
                m_lanService->sendFileOffer(contact->skypeName, fi.fileName(), fi.size());
            }
        }
    });

    m_mainWindow->show();
    setupSystemTray();

    SoundPlayer::instance().play("LOGIN.WAV");

    if (m_serverMode) {
        m_mainWindow->statusBar()->showMessage("Connected to server");
    } else if (m_p2pMode) {
        m_mainWindow->statusBar()->showMessage("LAN P2P mode - discovering peers...");
    } else {
        m_mainWindow->statusBar()->showMessage("Offline mode (no server)");
    }
}

// === Network handlers ===

void SkypeApp::onServerConnected() {
    m_serverMode = true;
    m_client->login(m_username, m_password);
}

void SkypeApp::onServerDisconnected() {
    if (m_mainWindow) {
        m_mainWindow->statusBar()->showMessage("Disconnected from server");
    }
}

void SkypeApp::onServerLoginResult(bool success, const QString& error) {
    if (success) {
        if (!m_mainWindow) {
            m_contacts.clear();
            showMainWindow();
        }
        if (m_serverMode) {
            m_client->requestContacts();
        } else if (m_p2pMode) {
            m_lanService->requestContacts();
        }
    } else {
        QMessageBox::warning(m_loginWindow, "Login Failed", error);
        m_serverMode = false;
        m_p2pMode = false;
    }
}

void SkypeApp::onServerContactList(const QJsonArray& contacts) {
    m_contacts.clear();
    int id = 1;
    for (auto val : contacts) {
        QJsonObject obj = val.toObject();
        Contact c;
        c.id = id++;
        c.displayName = obj["displayName"].toString();
        c.skypeName = obj["username"].toString();
        c.skypeNumber = obj["skypeNumber"].toString();
        QString statusStr = obj["status"].toString();
        if (statusStr == "Online") c.status = ContactStatus::Online;
        else if (statusStr == "Away") c.status = ContactStatus::Away;
        else if (statusStr == "Not Available") c.status = ContactStatus::NotAvailable;
        else if (statusStr == "Do Not Disturb") c.status = ContactStatus::DoNotDisturb;
        else if (statusStr == "Invisible") c.status = ContactStatus::Invisible;
        else c.status = ContactStatus::Offline;
        m_contacts.append(c);
    }
    m_nextContactId = id;

    if (m_mainWindow) {
        m_mainWindow->setContacts(m_contacts);
    }
}

void SkypeApp::onServerMessage(const QString& from, const QString& text, const QString& timestamp) {
    Q_UNUSED(timestamp);

    Contact* contact = findContactByName(from);
    if (!contact) {
        // Unknown sender — add them
        Contact c;
        c.id = m_nextContactId++;
        c.displayName = from;
        c.skypeName = from;
        c.status = ContactStatus::Online;
        m_contacts.append(c);
        contact = &m_contacts.last();
        if (m_mainWindow) m_mainWindow->setContacts(m_contacts);
    }

    if (contact->blocked) return;

    auto* chatWindow = findOrCreateChatWindow(contact->id);
    chatWindow->receiveMessage(contact->displayName, text);
    chatWindow->show();
    chatWindow->raise();
}

void SkypeApp::onServerPresence(const QString& username, const QString& status) {
    Contact* contact = findContactByName(username);
    if (contact) {
        if (status == "Online") contact->status = ContactStatus::Online;
        else if (status == "Away") contact->status = ContactStatus::Away;
        else if (status == "Not Available") contact->status = ContactStatus::NotAvailable;
        else if (status == "Do Not Disturb") contact->status = ContactStatus::DoNotDisturb;
        else if (status == "Invisible") contact->status = ContactStatus::Invisible;
        else contact->status = ContactStatus::Offline;

        if (m_mainWindow) {
            m_mainWindow->setContacts(m_contacts);
        }

        if (status == "Online") {
            SoundPlayer::instance().play("ONLINE.WAV");
        } else if (status == "Offline") {
            SoundPlayer::instance().play("OFFLINE.WAV");
        }
    }
}

// === Contact/Chat/Call handlers ===

void SkypeApp::onContactDoubleClicked(int contactId) {
    auto* chatWindow = findOrCreateChatWindow(contactId);
    chatWindow->show();
    chatWindow->raise();
    chatWindow->activateWindow();
}

void SkypeApp::onCallContact(int contactId) {
    if (m_callWindows.contains(contactId)) {
        m_callWindows[contactId]->raise();
        return;
    }

    Contact* contact = findContact(contactId);
    if (!contact) return;

    // Check if peer is reachable before creating call window
    if (m_p2pMode && !m_lanService->isRunning()) {
        QMessageBox::warning(m_mainWindow, "Call Failed",
            "P2P service is not running. Cannot place call.");
        return;
    }

    QString callId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto* callWin = new CallWindow(*contact, false, callId);
    m_callWindows.insert(contactId, callWin);
    wireCallWindow(callWin, contact);

    // Send call offer to peer
    if (m_p2pMode) {
        qDebug() << "Sending call offer to" << contact->skypeName << "callId:" << callId;
        m_lanService->sendCallOffer(contact->skypeName, callId);
    }

    callWin->show();
}

void SkypeApp::wireCallWindow(CallWindow* callWin, Contact* contact) {
    Q_UNUSED(contact);

    // hangUpRequested: user hung up or timed out — tell peer, but keep window open
    connect(callWin, &CallWindow::hangUpRequested, [this](int id, const QString& callId) {
        Contact* c = findContact(id);
        if (c && m_p2pMode) {
            m_lanService->sendCallEnd(c->skypeName, callId);
        }
    });

    // callEnded: user clicked "Close" after seeing "Call ended" — cleanup only
    connect(callWin, &CallWindow::callEnded, [this](int id) {
        if (m_callWindows.contains(id)) {
            m_callWindows[id]->deleteLater();
            m_callWindows.remove(id);
        }
    });

    connect(callWin, &CallWindow::callAccepted, [this](int cId, const QString& cCallId) {
        Contact* c = findContact(cId);
        if (c && m_p2pMode) {
            m_lanService->sendCallAccept(c->skypeName, cCallId);
        }
    });

    connect(callWin, &CallWindow::callRejected, [this](int cId, const QString& cCallId) {
        Contact* c = findContact(cId);
        if (c && m_p2pMode) {
            m_lanService->sendCallReject(c->skypeName, cCallId);
        }
    });

    connect(callWin, &CallWindow::audioToSend, [this](int cId, const QByteArray& data) {
        Contact* c = findContact(cId);
        if (c && m_p2pMode) {
            m_lanService->sendAudioData(c->skypeName, data);
        }
    });

    connect(callWin, &CallWindow::videoToSend, [this](int cId, const QByteArray& jpegData) {
        Contact* c = findContact(cId);
        if (c && m_p2pMode) {
            m_lanService->sendVideoData(c->skypeName, jpegData);
        }
    });
}

// Call signaling handlers

void SkypeApp::onCallOfferReceived(const QString& from, const QString& callId) {
    Contact* contact = findContactByName(from);
    if (!contact) {
        // Unknown caller — add them
        Contact c;
        c.id = m_nextContactId++;
        c.displayName = from;
        c.skypeName = from;
        c.status = ContactStatus::Online;
        m_contacts.append(c);
        contact = &m_contacts.last();
        if (m_mainWindow) m_mainWindow->setContacts(m_contacts);
    }

    // Silently reject calls from blocked contacts
    if (contact->blocked) {
        if (m_p2pMode) m_lanService->sendCallReject(from, callId);
        return;
    }

    if (m_callWindows.contains(contact->id)) {
        // Already in a call with this person — reject
        if (m_p2pMode) m_lanService->sendCallReject(from, callId);
        return;
    }

    auto* callWin = new CallWindow(*contact, true, callId);
    m_callWindows.insert(contact->id, callWin);
    wireCallWindow(callWin, contact);

    callWin->show();
    callWin->raise();

    if (m_trayIcon) {
        m_trayIcon->showMessage("Incoming Call",
            QString("%1 is calling you").arg(contact->displayName),
            QSystemTrayIcon::Information, 5000);
    }
}

void SkypeApp::onCallAcceptReceived(const QString& from, const QString& callId) {
    Q_UNUSED(callId);
    Contact* contact = findContactByName(from);
    if (!contact) return;
    if (m_callWindows.contains(contact->id)) {
        m_callWindows[contact->id]->onPeerAccepted();
    }
}

void SkypeApp::onCallRejectReceived(const QString& from, const QString& callId) {
    Q_UNUSED(callId);
    Contact* contact = findContactByName(from);
    if (!contact) return;
    if (m_callWindows.contains(contact->id)) {
        m_callWindows[contact->id]->onPeerRejected("Call rejected");
    }
}

void SkypeApp::onCallEndReceived(const QString& from, const QString& callId) {
    Q_UNUSED(callId);
    Contact* contact = findContactByName(from);
    if (!contact) return;
    if (m_callWindows.contains(contact->id)) {
        m_callWindows[contact->id]->onPeerHungUp();
    }
}

void SkypeApp::onAudioDataReceived(const QString& from, const QByteArray& data) {
    Contact* contact = findContactByName(from);
    if (!contact) return;
    if (m_callWindows.contains(contact->id)) {
        m_callWindows[contact->id]->playRemoteAudio(data);
    }
}

void SkypeApp::onVideoDataReceived(const QString& from, const QByteArray& jpegData) {
    Contact* contact = findContactByName(from);
    if (!contact) return;
    if (m_callWindows.contains(contact->id)) {
        m_callWindows[contact->id]->displayRemoteVideo(jpegData);
    }
}

CallWindow* SkypeApp::findCallWindowByCallId(const QString& callId) {
    for (auto it = m_callWindows.begin(); it != m_callWindows.end(); ++it) {
        if ((*it)->callId() == callId) return *it;
    }
    return nullptr;
}

// === Skype Number dialing ===

void SkypeApp::onCallSkypeNumber(const QString& skypeNumber) {
    // Look up the contact by Skype Number
    Contact* contact = nullptr;
    for (auto& c : m_contacts) {
        if (c.skypeNumber == skypeNumber) {
            contact = &c;
            break;
        }
    }

    if (!contact) {
        QMessageBox::information(m_mainWindow, "Skype Number Not Found",
            QString("No contact found with Skype Number %1.\n\n"
                    "Make sure the user is online on your network.").arg(skypeNumber));
        return;
    }

    onCallContact(contact->id);
}

// === Contact sharing ===

void SkypeApp::onSendContactToRecipient(int contactId, int recipientId) {
    Contact* contact = findContact(contactId);
    Contact* recipient = findContact(recipientId);
    if (!contact || !recipient) return;

    if (m_p2pMode) {
        m_lanService->sendContactShare(recipient->skypeName,
            contact->displayName, contact->skypeName, contact->skypeNumber);
    }

    QMessageBox::information(m_mainWindow, "Contact Sent",
        QString("Sent %1's contact info to %2.").arg(contact->displayName, recipient->displayName));
}

void SkypeApp::onContactShareReceived(const QString& from, const QString& contactName,
                                        const QString& skypeName, const QString& skypeNumber) {
    SoundPlayer::instance().play("INCOMING_CONTACTS.WAV");

    auto result = QMessageBox::question(m_mainWindow, "Contact Shared",
        QString("%1 wants to share a contact with you:\n\n"
                "Name: %2\nSkype Name: %3\nSkype Number: %4\n\n"
                "Add this contact?")
            .arg(from, contactName, skypeName,
                 skypeNumber.isEmpty() ? "(none)" : skypeNumber),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (result == QMessageBox::Yes) {
        // Check if we already have this contact
        Contact* existing = findContactByName(skypeName);
        if (existing) {
            QMessageBox::information(m_mainWindow, "Contact Exists",
                QString("%1 is already in your contact list.").arg(contactName));
            return;
        }

        Contact c;
        c.id = m_nextContactId++;
        c.displayName = contactName;
        c.skypeName = skypeName;
        c.skypeNumber = skypeNumber;
        c.status = ContactStatus::Offline;
        m_contacts.append(c);

        if (m_mainWindow) m_mainWindow->setContacts(m_contacts);
        if (m_p2pMode) m_lanService->addContact(skypeName);
    }
}

// === Group chat handlers ===

void SkypeApp::onCreateGroupChat(const QString& groupName, const QList<int>& memberIds) {
    GroupChat group;
    group.groupId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    group.groupName = groupName;
    group.creator = m_username;
    group.members.append(m_username);

    for (int id : memberIds) {
        Contact* c = findContact(id);
        if (c) group.members.append(c->skypeName);
    }

    m_groupChats.insert(group.groupId, group);

    auto* win = new GroupChatWindow(group, m_username);
    m_groupChatWindows.insert(group.groupId, win);

    connect(win, &GroupChatWindow::messageSent, [this](const QString& gId, const QString& text) {
        if (!m_groupChats.contains(gId)) return;
        const GroupChat& g = m_groupChats[gId];
        for (const QString& m : g.members) {
            if (m != m_username && m_p2pMode) {
                m_lanService->sendGroupMessage(m, gId, text);
            }
        }
    });

    connect(win, &GroupChatWindow::typingStarted, [this](const QString& gId) {
        if (!m_groupChats.contains(gId)) return;
        const GroupChat& g = m_groupChats[gId];
        for (const QString& m : g.members) {
            if (m != m_username && m_p2pMode) {
                m_lanService->sendGroupTyping(m, gId);
            }
        }
    });

    connect(win, &GroupChatWindow::leaveGroup, [this](const QString& gId) {
        if (!m_groupChats.contains(gId)) return;
        const GroupChat& g = m_groupChats[gId];
        for (const QString& m : g.members) {
            if (m != m_username && m_p2pMode) {
                m_lanService->sendGroupLeave(m, gId);
            }
        }
        m_groupChats.remove(gId);
        m_groupChatWindows.remove(gId);
    });

    connect(win, &GroupChatWindow::inviteMember, [this](const QString& gId, int) {
        if (!m_groupChats.contains(gId)) return;
        if (!m_mainWindow) return;
        int contactId = m_mainWindow->pickContact("Invite to Group");
        if (contactId < 0) return;
        Contact* c = findContact(contactId);
        if (!c) return;

        m_groupChats[gId].members.append(c->skypeName);
        if (m_groupChatWindows.contains(gId)) {
            m_groupChatWindows[gId]->addMember(c->skypeName);
        }
        if (m_p2pMode) {
            m_lanService->sendGroupInvite(c->skypeName, gId,
                m_groupChats[gId].groupName, m_groupChats[gId].members);
        }
    });

    // Send group creation to all members
    if (m_p2pMode) {
        for (const QString& m : group.members) {
            if (m != m_username) {
                m_lanService->sendGroupCreate(m, group.groupId, group.groupName, group.members);
            }
        }
    }

    win->show();
}

void SkypeApp::onGroupCreateReceived(const QString& from, const QString& groupId,
                                       const QString& groupName, const QStringList& members) {
    if (m_groupChatWindows.contains(groupId)) return;

    GroupChat group;
    group.groupId = groupId;
    group.groupName = groupName;
    group.creator = from;
    group.members = members;
    m_groupChats.insert(groupId, group);

    auto* win = new GroupChatWindow(group, m_username);
    m_groupChatWindows.insert(groupId, win);

    connect(win, &GroupChatWindow::messageSent, [this](const QString& gId, const QString& text) {
        if (!m_groupChats.contains(gId)) return;
        const GroupChat& g = m_groupChats[gId];
        for (const QString& m : g.members) {
            if (m != m_username && m_p2pMode) {
                m_lanService->sendGroupMessage(m, gId, text);
            }
        }
    });

    connect(win, &GroupChatWindow::typingStarted, [this](const QString& gId) {
        if (!m_groupChats.contains(gId)) return;
        const GroupChat& g = m_groupChats[gId];
        for (const QString& m : g.members) {
            if (m != m_username && m_p2pMode) {
                m_lanService->sendGroupTyping(m, gId);
            }
        }
    });

    connect(win, &GroupChatWindow::leaveGroup, [this](const QString& gId) {
        if (!m_groupChats.contains(gId)) return;
        const GroupChat& g = m_groupChats[gId];
        for (const QString& m : g.members) {
            if (m != m_username && m_p2pMode) {
                m_lanService->sendGroupLeave(m, gId);
            }
        }
        m_groupChats.remove(gId);
        m_groupChatWindows.remove(gId);
    });

    connect(win, &GroupChatWindow::inviteMember, [this](const QString& gId, int) {
        if (!m_groupChats.contains(gId)) return;
        if (!m_mainWindow) return;
        int contactId = m_mainWindow->pickContact("Invite to Group");
        if (contactId < 0) return;
        Contact* c = findContact(contactId);
        if (!c) return;

        m_groupChats[gId].members.append(c->skypeName);
        if (m_groupChatWindows.contains(gId)) {
            m_groupChatWindows[gId]->addMember(c->skypeName);
        }
        if (m_p2pMode) {
            m_lanService->sendGroupInvite(c->skypeName, gId,
                m_groupChats[gId].groupName, m_groupChats[gId].members);
        }
    });

    win->show();
    win->raise();
    SoundPlayer::instance().play("IM_RECEIVED.WAV");
}

void SkypeApp::onGroupMessageReceived(const QString& from, const QString& groupId, const QString& text) {
    if (m_groupChatWindows.contains(groupId)) {
        m_groupChatWindows[groupId]->receiveMessage(from, text);
        m_groupChatWindows[groupId]->show();
        m_groupChatWindows[groupId]->raise();
    }
}

void SkypeApp::onGroupTypingReceived(const QString& from, const QString& groupId) {
    if (m_groupChatWindows.contains(groupId)) {
        m_groupChatWindows[groupId]->showTypingIndicator(from);
    }
}

void SkypeApp::onGroupInviteReceived(const QString& from, const QString& groupId,
                                       const QString& groupName, const QStringList& members) {
    // Same as group create for the invited user
    onGroupCreateReceived(from, groupId, groupName, members);
}

void SkypeApp::onGroupLeaveReceived(const QString& from, const QString& groupId) {
    if (m_groupChats.contains(groupId)) {
        m_groupChats[groupId].members.removeAll(from);
    }
    if (m_groupChatWindows.contains(groupId)) {
        m_groupChatWindows[groupId]->removeMember(from);
    }
}

// === Conference handlers ===

void SkypeApp::onConferenceCallRequested(const QList<int>& contactIds) {
    QStringList participants;
    participants.append(m_username);

    for (int id : contactIds) {
        Contact* c = findContact(id);
        if (c) participants.append(c->skypeName);
    }

    QString confId = m_conferenceManager->createConference(m_username, participants);

    auto* confWin = new ConferenceCallWindow(confId, m_username, participants);
    m_conferenceWindows.insert(confId, confWin);

    connect(confWin, &ConferenceCallWindow::leaveRequested, [this](const QString& cId) {
        // Notify all participants
        ConferenceInfo* info = m_conferenceManager->getConference(cId);
        if (info && m_p2pMode) {
            for (const QString& p : info->participants) {
                if (p != m_username) {
                    m_lanService->sendConferenceLeave(p, cId);
                }
            }
        }
        m_conferenceManager->leaveConference(cId, m_username);
        m_conferenceWindows.remove(cId);
    });

    connect(confWin, &ConferenceCallWindow::audioToSend, [this](const QString& cId, const QByteArray& data) {
        ConferenceInfo* info = m_conferenceManager->getConference(cId);
        if (info && m_p2pMode) {
            m_lanService->sendConferenceAudio(info->participants, cId, data);
        }
    });

    connect(confWin, &ConferenceCallWindow::videoToSend, [this](const QString& cId, const QByteArray& jpegData) {
        ConferenceInfo* info = m_conferenceManager->getConference(cId);
        if (info && m_p2pMode) {
            m_lanService->sendConferenceVideo(info->participants, cId, jpegData);
        }
    });

    // Send conference invitation to all remote participants
    if (m_p2pMode) {
        for (const QString& p : participants) {
            if (p != m_username) {
                m_lanService->sendConferenceCreate(p, confId, participants);
            }
        }
    }

    confWin->show();
}

void SkypeApp::onConferenceCreateReceived(const QString& from, const QString& conferenceId, const QStringList& participants) {
    if (m_conferenceWindows.contains(conferenceId)) return;

    m_conferenceManager->createConference(from, participants);
    // Override the ID to match the sender's
    ConferenceInfo* info = m_conferenceManager->getConference(conferenceId);
    if (!info) {
        // Create with the exact same ID
        ConferenceInfo ci;
        ci.conferenceId = conferenceId;
        ci.host = from;
        ci.participants = participants;
        ci.active = true;
    }

    auto* confWin = new ConferenceCallWindow(conferenceId, m_username, participants);
    m_conferenceWindows.insert(conferenceId, confWin);

    connect(confWin, &ConferenceCallWindow::leaveRequested, [this](const QString& cId) {
        ConferenceInfo* ci = m_conferenceManager->getConference(cId);
        if (ci && m_p2pMode) {
            for (const QString& p : ci->participants) {
                if (p != m_username) {
                    m_lanService->sendConferenceLeave(p, cId);
                }
            }
        }
        m_conferenceManager->leaveConference(cId, m_username);
        m_conferenceWindows.remove(cId);
    });

    connect(confWin, &ConferenceCallWindow::audioToSend, [this](const QString& cId, const QByteArray& data) {
        ConferenceInfo* ci = m_conferenceManager->getConference(cId);
        if (ci && m_p2pMode) {
            m_lanService->sendConferenceAudio(ci->participants, cId, data);
        }
    });

    connect(confWin, &ConferenceCallWindow::videoToSend, [this](const QString& cId, const QByteArray& jpegData) {
        ConferenceInfo* ci = m_conferenceManager->getConference(cId);
        if (ci && m_p2pMode) {
            m_lanService->sendConferenceVideo(ci->participants, cId, jpegData);
        }
    });

    confWin->show();
    confWin->raise();

    if (m_trayIcon) {
        m_trayIcon->showMessage("Conference Call",
            QString("%1 invited you to a conference call").arg(from),
            QSystemTrayIcon::Information, 5000);
    }
}

void SkypeApp::onConferenceJoinReceived(const QString& from, const QString& conferenceId) {
    m_conferenceManager->joinConference(conferenceId, from);
    if (m_conferenceWindows.contains(conferenceId)) {
        m_conferenceWindows[conferenceId]->addParticipant(from);
    }
}

void SkypeApp::onConferenceLeaveReceived(const QString& from, const QString& conferenceId) {
    m_conferenceManager->leaveConference(conferenceId, from);
    if (m_conferenceWindows.contains(conferenceId)) {
        m_conferenceWindows[conferenceId]->removeParticipant(from);
    }
}

void SkypeApp::onConferenceAudioReceived(const QString& from, const QString& conferenceId, const QByteArray& data) {
    if (m_conferenceWindows.contains(conferenceId)) {
        m_conferenceWindows[conferenceId]->playRemoteAudio(from, data);
    }
}

void SkypeApp::onConferenceVideoReceived(const QString& from, const QString& conferenceId, const QByteArray& jpegData) {
    if (m_conferenceWindows.contains(conferenceId)) {
        m_conferenceWindows[conferenceId]->displayRemoteVideo(from, jpegData);
    }
}

void SkypeApp::onMessageSent(int contactId, const QString& text) {
    if (m_serverMode) {
        Contact* contact = findContact(contactId);
        if (contact) {
            m_client->sendMessage(contact->skypeName, text);
        }
    } else if (m_p2pMode) {
        Contact* contact = findContact(contactId);
        if (contact) {
            m_lanService->sendMessage(contact->skypeName, text);
        }
    } else {
        // Offline mock reply
        int delay = 2000 + QRandomGenerator::global()->bounded(3000);
        QTimer::singleShot(delay, [this, contactId]() {
            auto it = m_chatWindows.find(contactId);
            if (it == m_chatWindows.end()) return;

            Contact* contact = findContact(contactId);
            if (!contact || !contact->isOnline()) return;

            QString response = m_mockResponses[QRandomGenerator::global()->bounded(m_mockResponses.size())];
            (*it)->receiveMessage(contact->displayName, response);
        });
    }
}

void SkypeApp::simulateIncomingMessage() {
    if (m_serverMode) return; // Server handles real messages

    QList<int> onlineChatIds;
    for (auto it = m_chatWindows.begin(); it != m_chatWindows.end(); ++it) {
        Contact* c = findContact(it.key());
        if (c && c->isOnline() && it.value()->isVisible()) {
            onlineChatIds.append(it.key());
        }
    }

    if (!onlineChatIds.isEmpty()) {
        int contactId = onlineChatIds[QRandomGenerator::global()->bounded(onlineChatIds.size())];
        Contact* contact = findContact(contactId);
        if (contact) {
            QString msg = m_mockResponses[QRandomGenerator::global()->bounded(m_mockResponses.size())];
            m_chatWindows[contactId]->receiveMessage(contact->displayName, msg);
        }
    }

    m_simulationTimer->setInterval(15000 + QRandomGenerator::global()->bounded(15000));
}

void SkypeApp::simulateIncomingCall() {
    if (m_serverMode) return;

    QList<Contact*> onlineContacts;
    for (auto& c : m_contacts) {
        if (c.isOnline() && c.id != 1) {
            onlineContacts.append(&c);
        }
    }

    if (onlineContacts.isEmpty()) return;

    Contact* caller = onlineContacts[QRandomGenerator::global()->bounded(onlineContacts.size())];
    if (m_callWindows.contains(caller->id)) return;

    QString callId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* callWin = new CallWindow(*caller, true, callId);
    m_callWindows.insert(caller->id, callWin);
    wireCallWindow(callWin, caller);

    callWin->show();
    callWin->raise();

    if (m_trayIcon) {
        m_trayIcon->showMessage("Incoming Call",
            QString("%1 is calling you").arg(caller->displayName),
            QSystemTrayIcon::Information, 5000);
    }
}

void SkypeApp::onQuitRequested() {
    if (m_p2pMode) {
        m_lanService->stop();
    }
    SoundPlayer::instance().play("LOGOUT.WAV");
    QTimer::singleShot(500, qApp, &QApplication::quit);
}

void SkypeApp::onTypingStarted(int contactId) {
    Contact* contact = findContact(contactId);
    if (!contact) return;

    if (m_p2pMode) {
        m_lanService->sendTyping(contact->skypeName);
    }
}

void SkypeApp::onViewProfile(int contactId) {
    Contact* contact = findContact(contactId);
    if (!contact) return;

    QMessageBox profile(m_mainWindow);
    profile.setWindowTitle(QString("Profile - %1").arg(contact->displayName));
    profile.setText(
        QString("<b>%1</b><br><br>"
                "Skype Name: %2<br>"
                "Skype Number: %3<br>"
                "Status: %4<br>"
                "Mood: %5")
            .arg(contact->displayName,
                 contact->skypeName,
                 contact->skypeNumber.isEmpty() ? "(none)" : contact->skypeNumber,
                 contact->statusString(),
                 contact->moodText.isEmpty() ? "(none)" : contact->moodText)
    );
    profile.setIcon(QMessageBox::Information);
    profile.exec();
}

void SkypeApp::onRenameContact(int contactId) {
    Contact* contact = findContact(contactId);
    if (!contact) return;

    bool ok;
    QString newName = QInputDialog::getText(m_mainWindow, "Rename Contact",
        QString("Enter a new display name for %1:").arg(contact->displayName),
        QLineEdit::Normal, contact->displayName, &ok);
    if (ok && !newName.trimmed().isEmpty()) {
        contact->displayName = newName.trimmed();
        m_mainWindow->setContacts(m_contacts);
    }
}

void SkypeApp::onBlockContact(int contactId) {
    Contact* contact = findContact(contactId);
    if (!contact) return;

    if (contact->blocked) {
        // Unblock
        auto result = QMessageBox::question(m_mainWindow, "Unblock User",
            QString("Unblock %1?").arg(contact->displayName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result == QMessageBox::Yes) {
            contact->blocked = false;
            m_mainWindow->setContacts(m_contacts);
        }
        return;
    }

    auto result = QMessageBox::question(m_mainWindow, "Block User",
        QString("Are you sure you want to block %1?\n\n"
                "They will not be able to contact you.").arg(contact->displayName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        contact->blocked = true;
        m_mainWindow->setContacts(m_contacts);

        // Close any open chat window for this contact
        if (m_chatWindows.contains(contactId)) {
            m_chatWindows[contactId]->close();
            m_chatWindows[contactId]->deleteLater();
            m_chatWindows.remove(contactId);
        }

        // Close any active call with this contact
        if (m_callWindows.contains(contactId)) {
            m_callWindows[contactId]->close();
        }
    }
}

void SkypeApp::onRemoveContact(int contactId) {
    Contact* contact = findContact(contactId);
    if (!contact) return;

    auto result = QMessageBox::question(m_mainWindow, "Remove Contact",
        QString("Are you sure you want to remove %1 from your contact list?")
            .arg(contact->displayName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        // Close any open chat/call windows for this contact
        if (m_chatWindows.contains(contactId)) {
            m_chatWindows[contactId]->close();
            m_chatWindows[contactId]->deleteLater();
            m_chatWindows.remove(contactId);
        }
        if (m_callWindows.contains(contactId)) {
            m_callWindows[contactId]->close();
            m_callWindows[contactId]->deleteLater();
            m_callWindows.remove(contactId);
        }

        for (int i = 0; i < m_contacts.size(); ++i) {
            if (m_contacts[i].id == contactId) {
                m_contacts.removeAt(i);
                break;
            }
        }
        m_mainWindow->setContacts(m_contacts);
    }
}

Contact* SkypeApp::findContact(int id) {
    for (auto& c : m_contacts) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

Contact* SkypeApp::findContactByName(const QString& username) {
    for (auto& c : m_contacts) {
        if (c.skypeName == username) return &c;
    }
    return nullptr;
}

ChatWindow* SkypeApp::findOrCreateChatWindow(int contactId) {
    auto it = m_chatWindows.find(contactId);
    if (it != m_chatWindows.end()) {
        return *it;
    }

    Contact* contact = findContact(contactId);
    if (!contact) return nullptr;

    auto* chatWindow = new ChatWindow(*contact);
    connect(chatWindow, &ChatWindow::messageSent, this, &SkypeApp::onMessageSent);
    connect(chatWindow, &ChatWindow::typingStarted, this, &SkypeApp::onTypingStarted);
    connect(chatWindow, &ChatWindow::fileAttached, [this](int cId, const QString& fileName, const QByteArray& data) {
        Contact* c = findContact(cId);
        if (!c) return;
        if (m_p2pMode) {
            m_lanService->sendFileData(c->skypeName, fileName, data);
        }
    });
    m_chatWindows.insert(contactId, chatWindow);

    return chatWindow;
}
