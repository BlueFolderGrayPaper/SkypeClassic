#include "widgets/HistoryList.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFont>
#include <QListWidgetItem>
#include <QMenu>

HistoryList::HistoryList(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    populateMockHistory();
}

void HistoryList::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        auto* item = m_listWidget->itemAt(pos);
        QMenu menu(m_listWidget);
        if (item) {
            menu.addAction("&Call Back");
            menu.addAction("Send &Message");
            menu.addSeparator();
            menu.addAction("&Remove Entry", [this, item]() {
                delete m_listWidget->takeItem(m_listWidget->row(item));
            });
        }
        menu.addSeparator();
        menu.addAction("Clear &All History", [this]() {
            m_listWidget->clear();
        });
        menu.exec(m_listWidget->mapToGlobal(pos));
    });
    layout->addWidget(m_listWidget);
}

void HistoryList::populateMockHistory() {
    struct HistoryEntry {
        QString type;   // "call_in", "call_out", "call_missed", "chat"
        QString name;
        QString time;
        QString detail;
    };

    QList<HistoryEntry> entries = {
        {"call_out",     "Alice Johnson",    "Today, 2:30 PM",     "Duration: 5:23"},
        {"chat",         "Bob Smith",        "Today, 1:15 PM",     "3 messages"},
        {"call_missed",  "Charlie Brown",    "Today, 11:42 AM",    "Missed call"},
        {"call_in",      "Diana Prince",     "Yesterday, 4:00 PM", "Duration: 12:05"},
        {"chat",         "Helen Troy",       "Yesterday, 2:10 PM", "7 messages"},
        {"call_out",     "Echo / Sound Test","Yesterday, 10:00 AM","Duration: 0:15"},
        {"call_missed",  "Fiona Green",      "Feb 22, 9:30 AM",   "Missed call"},
        {"chat",         "Alice Johnson",    "Feb 21, 8:00 PM",   "12 messages"},
    };

    for (auto& entry : entries) {
        QString icon;
        QColor textColor = Qt::black;

        if (entry.type == "call_in") {
            icon = QString::fromUtf8("\xe2\x86\x90");  // ←
        } else if (entry.type == "call_out") {
            icon = QString::fromUtf8("\xe2\x86\x92");  // →
        } else if (entry.type == "call_missed") {
            icon = QString::fromUtf8("\xe2\x9c\x97");  // ✗
            textColor = QColor("#D14836");
        } else {
            icon = QString::fromUtf8("\xe2\x9c\x89");  // ✉
        }

        QString text = QString("%1  %2\n     %3 — %4")
            .arg(icon, entry.name, entry.time, entry.detail);

        auto* item = new QListWidgetItem(text, m_listWidget);
        item->setForeground(textColor);

        QFont f = item->font();
        f.setPointSize(8);
        item->setFont(f);
        item->setSizeHint(QSize(0, 38));
    }
}
