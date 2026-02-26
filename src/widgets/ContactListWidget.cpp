#include "widgets/ContactListWidget.h"

#include <QLabel>
#include <QFont>

ContactListWidget::ContactListWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_scrollContent = new QWidget();
    m_scrollContent->setStyleSheet("background-color: white;");
    m_contentLayout = new QVBoxLayout(m_scrollContent);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    m_contentLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);
    outerLayout->addWidget(m_scrollArea);
}

void ContactListWidget::setContacts(const QList<Contact>& contacts) {
    m_contacts = contacts;
    rebuildList();
}

QLabel* ContactListWidget::createGroupHeader(const QString& text) {
    auto* label = new QLabel(text, m_scrollContent);
    label->setObjectName("groupHeader");
    QFont f("Tahoma", -1);
    f.setPixelSize(11);
    f.setBold(true);
    label->setFont(f);
    label->setFixedHeight(18);
    label->setStyleSheet(
        "color: #000000; padding: 1px 4px; "
        "background-color: #D4D0C8; "
        "border-bottom: 1px solid #808080; "
        "border-top: 1px solid #808080;"
    );
    return label;
}

void ContactListWidget::rebuildList() {
    // Clear existing items
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Separate online and offline contacts
    QList<Contact> online, offline;
    for (auto& c : m_contacts) {
        if (c.isOnline()) {
            online.append(c);
        } else {
            offline.append(c);
        }
    }

    // Online section
    m_contentLayout->addWidget(createGroupHeader(
        QString("Online (%1/%2)").arg(online.size()).arg(m_contacts.size())));

    for (auto& c : online) {
        auto* item = new ContactItem(c, m_scrollContent);
        connect(item, &ContactItem::doubleClicked, this, &ContactListWidget::contactDoubleClicked);
        connect(item, &ContactItem::callRequested, this, &ContactListWidget::callRequested);
        connect(item, &ContactItem::sendFileRequested, this, &ContactListWidget::sendFileRequested);
        connect(item, &ContactItem::viewProfileRequested, this, &ContactListWidget::viewProfileRequested);
        connect(item, &ContactItem::renameRequested, this, &ContactListWidget::renameRequested);
        connect(item, &ContactItem::blockRequested, this, &ContactListWidget::blockRequested);
        connect(item, &ContactItem::removeRequested, this, &ContactListWidget::removeRequested);
        connect(item, &ContactItem::sendContactRequested, this, &ContactListWidget::sendContactRequested);
        m_contentLayout->addWidget(item);
    }

    // Offline section
    m_contentLayout->addWidget(createGroupHeader(
        QString("Offline (%1/%2)").arg(offline.size()).arg(m_contacts.size())));

    for (auto& c : offline) {
        auto* item = new ContactItem(c, m_scrollContent);
        connect(item, &ContactItem::doubleClicked, this, &ContactListWidget::contactDoubleClicked);
        connect(item, &ContactItem::callRequested, this, &ContactListWidget::callRequested);
        connect(item, &ContactItem::sendFileRequested, this, &ContactListWidget::sendFileRequested);
        connect(item, &ContactItem::viewProfileRequested, this, &ContactListWidget::viewProfileRequested);
        connect(item, &ContactItem::renameRequested, this, &ContactListWidget::renameRequested);
        connect(item, &ContactItem::blockRequested, this, &ContactListWidget::blockRequested);
        connect(item, &ContactItem::removeRequested, this, &ContactListWidget::removeRequested);
        connect(item, &ContactItem::sendContactRequested, this, &ContactListWidget::sendContactRequested);
        m_contentLayout->addWidget(item);
    }

    m_contentLayout->addStretch();
}
