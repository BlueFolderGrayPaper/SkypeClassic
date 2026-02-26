#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QToolBar>
#include "widgets/ContactListWidget.h"
#include "widgets/DialPad.h"
#include "widgets/HistoryList.h"
#include "widgets/StatusSelector.h"
#include "models/Contact.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& username, QWidget* parent = nullptr);

    void setContacts(const QList<Contact>& contacts);

signals:
    void profileUpdated(const QString& displayName, const QString& moodText);
    void contactDoubleClicked(int contactId);
    void callContact(int contactId);
    void sendFileToContact(int contactId);
    void statusChanged(ContactStatus status);
    void quitRequested();
    void contactAdded(const QString& skypeName);
    void viewProfileRequested(int contactId);
    void renameContact(int contactId);
    void blockContact(int contactId);
    void removeContact(int contactId);
    void conferenceCallRequested(const QList<int>& contactIds);
    void callSkypeNumber(const QString& skypeNumber);
    void sendContactToRecipient(int contactId, int recipientId);
    void createGroupChat(const QString& groupName, const QList<int>& memberIds);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

public:
    int pickContact(const QString& title);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    QList<int> pickMultipleContacts(const QString& title);
    void showAccountDialog();

    QString m_username;
    QList<Contact> m_contacts;
    QTabWidget* m_tabs;
    ContactListWidget* m_contactList;
    DialPad* m_dialPad;
    HistoryList* m_historyList;
    StatusSelector* m_statusSelector;
};
