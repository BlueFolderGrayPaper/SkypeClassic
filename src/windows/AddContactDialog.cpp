#include "windows/AddContactDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGroupBox>

AddContactDialog::AddContactDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle("Add a Contact");
    resize(320, 240);
    setMinimumSize(280, 200);
}

void AddContactDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto* infoLabel = new QLabel(
        "Type the Skype Name of the person you want\n"
        "to add to your contact list.", this);
    mainLayout->addWidget(infoLabel);

    // Skype Name field
    auto* nameLabel = new QLabel("&Skype Name:", this);
    mainLayout->addWidget(nameLabel);

    m_skypeNameEdit = new QLineEdit(this);
    m_skypeNameEdit->setFixedHeight(21);
    nameLabel->setBuddy(m_skypeNameEdit);
    mainLayout->addWidget(m_skypeNameEdit);

    mainLayout->addSpacing(4);

    // Greeting message
    auto* msgLabel = new QLabel("Say &hello (optional):", this);
    mainLayout->addWidget(msgLabel);

    m_messageEdit = new QLineEdit(this);
    m_messageEdit->setFixedHeight(21);
    msgLabel->setBuddy(m_messageEdit);
    mainLayout->addWidget(m_messageEdit);

    mainLayout->addStretch();

    // Separator
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto* addBtn = new QPushButton("&Add", this);
    addBtn->setMinimumSize(75, 23);
    addBtn->setDefault(true);
    connect(addBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(addBtn);

    auto* cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setMinimumSize(75, 23);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);

    m_skypeNameEdit->setFocus();
}

QString AddContactDialog::skypeName() const {
    return m_skypeNameEdit->text().trimmed();
}
