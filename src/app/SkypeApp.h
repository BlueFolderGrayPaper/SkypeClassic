#pragma once

#include <QObject>
#include <QMap>
#include <QList>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include "models/Contact.h"
#include "models/Message.h"
#include "windows/LoginWindow.h"
#include "windows/MainWindow.h"
#include "windows/ChatWindow.h"
#include "windows/CallWindow.h"
#include "network/SkypeClient.h"
#include "network/LANPeerService.h"
#include "network/ConferenceManager.h"
#include "windows/ConferenceCallWindow.h"
#include "windows/GroupChatWindow.h"
#include "models/GroupChat.h"

class SkypeApp : public QObject {
    Q_OBJECT

public:
    explicit SkypeApp(QObject* parent = nullptr);
    ~SkypeApp();

    void start();

private slots:
    void onLoginRequested(const QString& username);
    void onContactDoubleClicked(int contactId);
    void onCallContact(int contactId);
    void onMessageSent(int contactId, const QString& text);
    void onQuitRequested();
    void onTypingStarted(int contactId);
    void onViewProfile(int contactId);
    void onRenameContact(int contactId);
    void onBlockContact(int contactId);
    void onRemoveContact(int contactId);

    // Mock simulation
    void simulateIncomingMessage();
    void simulateIncomingCall();

    // Network
    void onServerConnected();
    void onServerDisconnected();
    void onServerLoginResult(bool success, const QString& error);
    void onServerContactList(const QJsonArray& contacts);
    void onServerMessage(const QString& from, const QString& text, const QString& timestamp);
    void onServerPresence(const QString& username, const QString& status);

    // Call signaling
    void onCallOfferReceived(const QString& from, const QString& callId);
    void onCallAcceptReceived(const QString& from, const QString& callId);
    void onCallRejectReceived(const QString& from, const QString& callId);
    void onCallEndReceived(const QString& from, const QString& callId);
    void onAudioDataReceived(const QString& from, const QByteArray& data);
    void onVideoDataReceived(const QString& from, const QByteArray& jpegData);

    // Conference
    void onCallSkypeNumber(const QString& skypeNumber);
    void onSendContactToRecipient(int contactId, int recipientId);
    void onContactShareReceived(const QString& from, const QString& contactName, const QString& skypeName, const QString& skypeNumber);

    // Group chats
    void onCreateGroupChat(const QString& groupName, const QList<int>& memberIds);
    void onGroupCreateReceived(const QString& from, const QString& groupId, const QString& groupName, const QStringList& members);
    void onGroupMessageReceived(const QString& from, const QString& groupId, const QString& text);
    void onGroupTypingReceived(const QString& from, const QString& groupId);
    void onGroupInviteReceived(const QString& from, const QString& groupId, const QString& groupName, const QStringList& members);
    void onGroupLeaveReceived(const QString& from, const QString& groupId);
    void onConferenceCallRequested(const QList<int>& contactIds);
    void onConferenceCreateReceived(const QString& from, const QString& conferenceId, const QStringList& participants);
    void onConferenceJoinReceived(const QString& from, const QString& conferenceId);
    void onConferenceLeaveReceived(const QString& from, const QString& conferenceId);
    void onConferenceAudioReceived(const QString& from, const QString& conferenceId, const QByteArray& data);
    void onConferenceVideoReceived(const QString& from, const QString& conferenceId, const QByteArray& jpegData);

private:
    Contact* findContact(int id);
    Contact* findContactByName(const QString& username);
    ChatWindow* findOrCreateChatWindow(int contactId);
    CallWindow* findCallWindowByCallId(const QString& callId);
    void wireCallWindow(CallWindow* callWin, Contact* contact);
    void setupSystemTray();
    void showMainWindow();
    void startP2PMode();

    LoginWindow* m_loginWindow = nullptr;
    MainWindow* m_mainWindow = nullptr;
    QMap<int, ChatWindow*> m_chatWindows;
    QMap<int, CallWindow*> m_callWindows;
    QMap<QString, ConferenceCallWindow*> m_conferenceWindows;
    QMap<QString, GroupChatWindow*> m_groupChatWindows;
    QMap<QString, GroupChat> m_groupChats;
    ConferenceManager* m_conferenceManager;
    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;

    SkypeClient* m_client;
    LANPeerService* m_lanService;
    bool m_serverMode = false;
    bool m_p2pMode = false;

    QString m_username;
    QString m_password;
    QList<Contact> m_contacts;
    int m_nextContactId = 100;
    QTimer* m_simulationTimer;
    QTimer* m_callSimTimer;
    QStringList m_mockResponses;
};
