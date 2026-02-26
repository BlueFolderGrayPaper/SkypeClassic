#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include <QTimer>
#include "models/Contact.h"
#include "models/Message.h"

class ChatWindow : public QWidget {
    Q_OBJECT

public:
    explicit ChatWindow(const Contact& contact, QWidget* parent = nullptr);
    ~ChatWindow();

    int contactId() const { return m_contact.id; }

signals:
    void messageSent(int contactId, const QString& text);
    void typingStarted(int contactId);
    void fileAttached(int contactId, const QString& fileName, const QByteArray& fileData);

public slots:
    void receiveMessage(const QString& sender, const QString& text);
    void receiveFileAttachment(const QString& sender, const QString& fileName, const QString& savedPath);
    void showTypingIndicator(const QString& sender);
    void hideTypingIndicator();

private slots:
    void onSendClicked();
    void onAttachClicked();
    void onTypingTimeout();
    void onInputChanged();
    void onAnchorClicked(const QUrl& url);
    void adjustInputHeight();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void appendMessage(const QString& sender, const QString& text, bool isOutgoing);
    void loadHistory();
    void saveHistory();

    Contact m_contact;
    QTextBrowser* m_chatHistory;
    QTextEdit* m_inputEdit;
    QPushButton* m_sendButton;
    QPushButton* m_attachButton;
    QLabel* m_typingLabel;
    QTimer* m_typingTimer;
    QTimer* m_ownTypingTimer;
    QList<Message> m_messages;
};
