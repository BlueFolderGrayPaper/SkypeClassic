#pragma once

#include <QWidget>
#include <QLabel>
#include "models/Contact.h"

class ContactItem : public QWidget {
    Q_OBJECT

public:
    explicit ContactItem(const Contact& contact, QWidget* parent = nullptr);

    const Contact& contact() const { return m_contact; }
    void setContact(const Contact& contact);

signals:
    void doubleClicked(int contactId);
    void callRequested(int contactId);
    void sendFileRequested(int contactId);
    void viewProfileRequested(int contactId);
    void renameRequested(int contactId);
    void blockRequested(int contactId);
    void removeRequested(int contactId);
    void sendContactRequested(int contactId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    Contact m_contact;
    bool m_hovered = false;
};
