#include "widgets/ContactItem.h"

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>

ContactItem::ContactItem(const Contact& contact, QWidget* parent)
    : QWidget(parent)
    , m_contact(contact)
{
    setFixedHeight(m_contact.moodText.isEmpty() ? 20 : 32);
    setMinimumWidth(180);
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
}

void ContactItem::setContact(const Contact& contact) {
    m_contact = contact;
    setFixedHeight(m_contact.moodText.isEmpty() ? 20 : 32);
    update();
}

void ContactItem::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // Background — hard selection highlight, no smooth hover
    if (m_hovered) {
        p.fillRect(rect(), QColor("#0A246A"));
    } else {
        p.fillRect(rect(), Qt::white);
    }

    // Status icon — small hard-edged square, not a circle
    QColor statusColor;
    switch (m_contact.status) {
        case ContactStatus::Online:       statusColor = QColor("#008000"); break;
        case ContactStatus::Away:         statusColor = QColor("#CCCC00"); break;
        case ContactStatus::NotAvailable: statusColor = QColor("#CC8800"); break;
        case ContactStatus::DoNotDisturb: statusColor = QColor("#CC0000"); break;
        case ContactStatus::Invisible:    statusColor = QColor("#808080"); break;
        case ContactStatus::Offline:      statusColor = QColor("#C0C0C0"); break;
    }

    int iconY = m_contact.moodText.isEmpty() ? 5 : 4;
    p.setPen(QColor("#404040"));
    p.setBrush(statusColor);
    p.drawRect(4, iconY, 9, 9);

    // Display name
    QFont nameFont("Tahoma", -1);
    nameFont.setPixelSize(11);
    if (m_contact.isOnline()) {
        nameFont.setBold(true);
    }
    p.setFont(nameFont);

    if (m_contact.blocked) {
        nameFont.setStrikeOut(true);
        p.setFont(nameFont);
    }

    if (m_hovered) {
        p.setPen(Qt::white);
    } else if (m_contact.blocked) {
        p.setPen(QColor("#CC0000"));
    } else {
        p.setPen(m_contact.isOnline() ? Qt::black : QColor("#808080"));
    }

    int textX = 20;
    int textY = m_contact.moodText.isEmpty() ? 14 : 13;
    p.drawText(textX, textY, m_contact.displayName);

    // Mood text
    if (!m_contact.moodText.isEmpty()) {
        QFont moodFont("Tahoma", -1);
        moodFont.setPixelSize(10);
        p.setFont(moodFont);
        if (m_hovered) {
            p.setPen(QColor("#AAAACC"));
        } else {
            p.setPen(QColor("#808080"));
        }
        p.drawText(textX, 26, m_contact.moodText);
    }

    // Bottom separator line
    p.setPen(QColor("#E0E0E0"));
    p.drawLine(0, height() - 1, width(), height() - 1);
}

void ContactItem::mouseDoubleClickEvent(QMouseEvent*) {
    emit doubleClicked(m_contact.id);
}

void ContactItem::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.addAction("Send &Message", [this]() { emit doubleClicked(m_contact.id); });
    menu.addAction("&Call", [this]() { emit callRequested(m_contact.id); });
    menu.addSeparator();
    menu.addAction("Send &File...", [this]() { emit sendFileRequested(m_contact.id); });
    menu.addAction("Send Contact &To...", [this]() { emit sendContactRequested(m_contact.id); });
    menu.addSeparator();
    menu.addAction("View &Profile", [this]() { emit viewProfileRequested(m_contact.id); });
    menu.addAction("&Rename", [this]() { emit renameRequested(m_contact.id); });
    menu.addSeparator();
    menu.addAction(m_contact.blocked ? "&Unblock This User" : "&Block This User",
                   [this]() { emit blockRequested(m_contact.id); });
    menu.addAction("Remove from Contact &List", [this]() { emit removeRequested(m_contact.id); });
    menu.exec(event->globalPos());
}

void ContactItem::enterEvent(QEvent*) {
    m_hovered = true;
    update();
}

void ContactItem::leaveEvent(QEvent*) {
    m_hovered = false;
    update();
}
