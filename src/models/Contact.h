#pragma once

#include <QString>
#include <QList>

enum class ContactStatus {
    Online,
    Away,
    NotAvailable,
    DoNotDisturb,
    Invisible,
    Offline
};

struct Contact {
    int id;
    QString displayName;
    QString skypeName;
    QString skypeNumber;
    ContactStatus status;
    QString moodText;
    bool blocked = false;

    bool isOnline() const {
        return status != ContactStatus::Offline;
    }

    QString statusString() const;
    static QList<Contact> createMockContacts();
};
