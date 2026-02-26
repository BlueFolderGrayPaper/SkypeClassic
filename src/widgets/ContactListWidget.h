#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QList>
#include "models/Contact.h"
#include "widgets/ContactItem.h"

class ContactListWidget : public QWidget {
    Q_OBJECT

public:
    explicit ContactListWidget(QWidget* parent = nullptr);

    void setContacts(const QList<Contact>& contacts);

signals:
    void contactDoubleClicked(int contactId);
    void callRequested(int contactId);
    void sendFileRequested(int contactId);
    void viewProfileRequested(int contactId);
    void renameRequested(int contactId);
    void blockRequested(int contactId);
    void removeRequested(int contactId);
    void sendContactRequested(int contactId);

private:
    void rebuildList();
    QLabel* createGroupHeader(const QString& text);

    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_contentLayout;
    QList<Contact> m_contacts;
};
