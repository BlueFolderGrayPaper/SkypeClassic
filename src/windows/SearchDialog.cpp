#include "windows/SearchDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QListWidgetItem>
#include <QMessageBox>

SearchDialog::SearchDialog(const QList<Contact>& contacts, QWidget* parent)
    : QDialog(parent)
    , m_contacts(contacts)
{
    setupUi();
    setWindowTitle("Search for Skype Users");
    resize(360, 340);
}

void SearchDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto* infoLabel = new QLabel("Search your contacts:", this);
    mainLayout->addWidget(infoLabel);

    // Search bar
    auto* searchLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setFixedHeight(21);
    m_searchEdit->setPlaceholderText("Name or Skype Name");
    searchLayout->addWidget(m_searchEdit);

    m_searchBtn = new QPushButton("&Search", this);
    m_searchBtn->setMinimumSize(80, 23);
    connect(m_searchBtn, &QPushButton::clicked, this, &SearchDialog::onSearchClicked);
    searchLayout->addWidget(m_searchBtn);

    mainLayout->addLayout(searchLayout);

    // Results
    auto* resultsLabel = new QLabel("Results:", this);
    mainLayout->addWidget(resultsLabel);

    m_resultsList = new QListWidget(this);
    mainLayout->addWidget(m_resultsList);

    // Separator
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // Bottom buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_addBtn = new QPushButton("&Add Contact", this);
    m_addBtn->setMinimumSize(85, 23);
    m_addBtn->setEnabled(false);
    connect(m_addBtn, &QPushButton::clicked, [this]() {
        auto* item = m_resultsList->currentItem();
        if (item && (item->flags() & Qt::ItemIsSelectable)) {
            m_addedContact = item->data(Qt::UserRole).toString();
            emit contactAdded(m_addedContact);
            QMessageBox::information(this, "Add Contact",
                QString("Contact request sent to %1.").arg(m_addedContact));
        }
    });
    buttonLayout->addWidget(m_addBtn);

    auto* closeBtn = new QPushButton("&Close", this);
    closeBtn->setMinimumSize(75, 23);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);

    mainLayout->addLayout(buttonLayout);

    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchDialog::onSearchClicked);
    connect(m_resultsList, &QListWidget::currentRowChanged, [this](int row) {
        m_addBtn->setEnabled(row >= 0);
    });

    m_searchEdit->setFocus();
}

void SearchDialog::onSearchClicked() {
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) return;

    m_resultsList->clear();

    QString lowerQuery = query.toLower();
    for (const auto& c : m_contacts) {
        if (c.displayName.toLower().contains(lowerQuery) ||
            c.skypeName.toLower().contains(lowerQuery)) {
            QString statusStr = c.statusString();
            auto* item = new QListWidgetItem(
                QString("%1  (%2)  -  %3").arg(c.displayName, c.skypeName, statusStr),
                m_resultsList);
            item->setData(Qt::UserRole, c.skypeName);
            if (!c.isOnline()) {
                item->setForeground(QColor("#808080"));
            }
        }
    }

    if (m_resultsList->count() == 0) {
        m_resultsList->addItem("No users found.");
        m_resultsList->item(0)->setFlags(Qt::NoItemFlags);
    }
}
