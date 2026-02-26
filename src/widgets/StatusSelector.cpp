#include "widgets/StatusSelector.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>

StatusSelector::StatusSelector(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    populateStatuses();
}

void StatusSelector::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);

    m_combo = new QComboBox(this);
    m_combo->setMinimumHeight(22);
    layout->addWidget(m_combo);

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        emit statusChanged(static_cast<ContactStatus>(m_combo->itemData(index).toInt()));
    });
}

QIcon StatusSelector::statusIcon(ContactStatus status) const {
    QPixmap pix(12, 12);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QColor color;
    switch (status) {
        case ContactStatus::Online:       color = QColor("#4B9E4B"); break;
        case ContactStatus::Away:         color = QColor("#E8A317"); break;
        case ContactStatus::NotAvailable: color = QColor("#E8A317"); break;
        case ContactStatus::DoNotDisturb: color = QColor("#D14836"); break;
        case ContactStatus::Invisible:    color = QColor("#999999"); break;
        case ContactStatus::Offline:      color = QColor("#BBBBBB"); break;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(1, 1, 10, 10);
    p.end();

    return QIcon(pix);
}

void StatusSelector::populateStatuses() {
    struct StatusEntry { ContactStatus status; QString label; };
    QList<StatusEntry> entries = {
        {ContactStatus::Online,       "Online"},
        {ContactStatus::Away,         "Away"},
        {ContactStatus::NotAvailable, "Not Available"},
        {ContactStatus::DoNotDisturb, "Do Not Disturb"},
        {ContactStatus::Invisible,    "Invisible"},
        {ContactStatus::Offline,      "Offline"},
    };

    for (auto& e : entries) {
        m_combo->addItem(statusIcon(e.status), e.label, static_cast<int>(e.status));
    }
}

ContactStatus StatusSelector::currentStatus() const {
    return static_cast<ContactStatus>(m_combo->currentData().toInt());
}
