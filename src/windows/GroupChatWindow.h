#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QTimer>
#include <QMap>
#include "models/GroupChat.h"

class GroupChatWindow : public QWidget {
    Q_OBJECT

public:
    explicit GroupChatWindow(const GroupChat& group, const QString& localUser,
                              QWidget* parent = nullptr);
    ~GroupChatWindow();

    QString groupId() const { return m_group.groupId; }

    void addMember(const QString& username);
    void removeMember(const QString& username);
    void receiveMessage(const QString& sender, const QString& text);
    void showTypingIndicator(const QString& sender);

signals:
    void messageSent(const QString& groupId, const QString& text);
    void typingStarted(const QString& groupId);
    void leaveGroup(const QString& groupId);
    void inviteMember(const QString& groupId, int contactId);

private slots:
    void onSendClicked();
    void onInputChanged();
    void onTypingTimeout();
    void onLeaveClicked();
    void onInviteClicked();

private:
    void setupUi();
    void appendMessage(const QString& sender, const QString& text, bool isLocal);
    void rebuildMemberList();
    void loadHistory();
    void saveHistory();

    GroupChat m_group;
    QString m_localUser;

    QTextBrowser* m_chatHistory;
    QTextEdit* m_inputEdit;
    QPushButton* m_sendButton;
    QPushButton* m_leaveButton;
    QPushButton* m_inviteButton;
    QLabel* m_typingLabel;
    QListWidget* m_memberList;
    QTimer* m_typingTimer;
    QTimer* m_ownTypingTimer;
    QMap<QString, QTimer*> m_peerTypingTimers;
};
