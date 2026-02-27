#include "windows/MainWindow.h"
#include "windows/AddContactDialog.h"
#include "windows/SearchDialog.h"
#include "windows/OptionsDialog.h"
#include "windows/FileTransferDialog.h"
#include "utils/SoundPlayer.h"
#include "utils/CryptoUtils.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QLabel>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QApplication>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QSettings>
#include <QLineEdit>
#include <QTextEdit>
#include <QFile>
#include <QTextStream>
#include <QInputDialog>

MainWindow::MainWindow(const QString& username, QWidget* parent)
    : QMainWindow(parent)
    , m_username(username)
{
    setWindowTitle(QString("Skype - %1").arg(m_username));
    resize(280, 550);
    setMinimumSize(250, 400);

    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();
}

void MainWindow::setupMenuBar() {
    // Keep the menu bar inside the window on macOS (don't use native global menu)
    menuBar()->setNativeMenuBar(false);

    // File
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&My Profile...", [this]() { showAccountDialog(); });
    fileMenu->addSeparator();
    fileMenu->addAction("Sign &Out", [this]() {
        SoundPlayer::instance().play("LOGOUT.WAV");
    });
    auto* quitAction = fileMenu->addAction("&Quit", [this]() {
        emit quitRequested();
    });
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));

    // Account
    auto* accountMenu = menuBar()->addMenu("&Account");
    accountMenu->addAction("Change &Password...", [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Change Password");
        dlg.resize(300, 200);
        dlg.setMinimumSize(260, 180);

        auto* layout = new QVBoxLayout(&dlg);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        layout->addWidget(new QLabel(QString("Change password for <b>%1</b>:").arg(m_username)));
        layout->addSpacing(4);

        layout->addWidget(new QLabel("Current password:"));
        auto* currentPw = new QLineEdit(&dlg);
        currentPw->setEchoMode(QLineEdit::Password);
        layout->addWidget(currentPw);

        layout->addWidget(new QLabel("New password:"));
        auto* newPw = new QLineEdit(&dlg);
        newPw->setEchoMode(QLineEdit::Password);
        layout->addWidget(newPw);

        layout->addWidget(new QLabel("Confirm new password:"));
        auto* confirmPw = new QLineEdit(&dlg);
        confirmPw->setEchoMode(QLineEdit::Password);
        layout->addWidget(confirmPw);

        layout->addStretch();
        auto* btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        auto* okBtn = new QPushButton("OK", &dlg);
        okBtn->setMinimumSize(75, 23);
        connect(okBtn, &QPushButton::clicked, [&]() {
            if (newPw->text().isEmpty()) {
                QMessageBox::warning(&dlg, "Error", "New password cannot be empty.");
                return;
            }
            if (newPw->text() != confirmPw->text()) {
                QMessageBox::warning(&dlg, "Error", "Passwords do not match.");
                return;
            }
            // Verify current password if one is stored
            QSettings s("SkypeClassic", "SkypeClassic");
            QString stored = s.value("account/password").toString();
            if (!stored.isEmpty() && !CryptoUtils::verifyPassword(currentPw->text(), stored)) {
                QMessageBox::warning(&dlg, "Error", "Current password is incorrect.");
                return;
            }
            s.setValue("account/password", CryptoUtils::hashAndStore(newPw->text()));
            QMessageBox::information(&dlg, "Password Changed", "Your password has been changed.");
            dlg.accept();
        });
        btnLayout->addWidget(okBtn);
        auto* cancelBtn = new QPushButton("Cancel", &dlg);
        cancelBtn->setMinimumSize(75, 23);
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);

        dlg.exec();
    });
    accountMenu->addAction("&Manage Account...", [this]() { showAccountDialog(); });

    // Contacts
    auto* contactsMenu = menuBar()->addMenu("&Contacts");
    contactsMenu->addAction("&Add a Contact...", [this]() {
        AddContactDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && !dlg.skypeName().isEmpty()) {
            emit contactAdded(dlg.skypeName());
            SoundPlayer::instance().play("INCOMING_AUTH.WAV");
            QMessageBox::information(this, "Contact Added",
                QString("Contact request sent to '%1'.").arg(dlg.skypeName()));
        }
    });
    contactsMenu->addAction("&Search for Skype Users...", [this]() {
        SearchDialog dlg(m_contacts, this);
        connect(&dlg, &SearchDialog::contactAdded, [this](const QString& name) {
            emit contactAdded(name);
        });
        dlg.exec();
    });
    contactsMenu->addSeparator();
    contactsMenu->addAction("Create &Group Chat...", [this]() {
        QList<int> ids = pickMultipleContacts("Create Group Chat");
        if (ids.size() < 2) {
            if (!ids.isEmpty())
                QMessageBox::information(this, "Group Chat", "Select at least 2 contacts for a group chat.");
            return;
        }
        // Ask for group name
        bool ok;
        QString name = QInputDialog::getText(this, "Group Chat Name",
            "Enter a name for the group:", QLineEdit::Normal, "Group Chat", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            emit createGroupChat(name.trimmed(), ids);
        }
    });
    contactsMenu->addSeparator();
    contactsMenu->addAction("&Import Contacts...", [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Import Contacts",
            QString(), "Text Files (*.txt);;All Files (*)");
        if (file.isEmpty()) return;
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Import Contacts", "Could not open file.");
            return;
        }
        int count = 0;
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                emit contactAdded(line);
                count++;
            }
        }
        QMessageBox::information(this, "Import Contacts",
            QString("Imported %1 contact(s).").arg(count));
    });

    // Tools
    auto* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("&Options...", [this]() {
        OptionsDialog dlg(this);
        dlg.exec();
    });
    toolsMenu->addSeparator();
    toolsMenu->addAction("&Send File...", [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Send File");
        if (!file.isEmpty()) {
            auto* ftDlg = new FileTransferDialog("Contact", QString(),
                FileTransferDialog::Sending, this, file);
            ftDlg->show();
        }
    });
    toolsMenu->addAction("&Export Contacts...", [this]() {
        if (m_contacts.isEmpty()) {
            QMessageBox::information(this, "Export Contacts", "No contacts to export.");
            return;
        }
        QString file = QFileDialog::getSaveFileName(this, "Export Contacts",
            "contacts.txt", "Text Files (*.txt);;All Files (*)");
        if (file.isEmpty()) return;
        QFile f(file);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Export Contacts", "Could not save file.");
            return;
        }
        QTextStream out(&f);
        for (const auto& c : m_contacts) {
            out << c.skypeName << "\n";
        }
        QMessageBox::information(this, "Export Contacts",
            QString("Exported %1 contact(s).").arg(m_contacts.size()));
    });

    // Call
    auto* callMenu = menuBar()->addMenu("Ca&ll");
    callMenu->addAction("&Make a Call...", [this]() {
        int id = pickContact("Make a Call");
        if (id >= 0) emit callContact(id);
    });
    callMenu->addAction("&Answer Call", [this]() {
        QMessageBox::information(this, "Answer Call",
            "No incoming calls at this time.");
    });
    callMenu->addAction("&Hang Up", [this]() {
        QMessageBox::information(this, "Hang Up",
            "No active calls to hang up.");
    });
    callMenu->addAction("&Conference Call...", [this]() {
        QList<int> ids = pickMultipleContacts("Conference Call");
        if (ids.size() >= 2) {
            emit conferenceCallRequested(ids);
        } else if (!ids.isEmpty()) {
            QMessageBox::information(this, "Conference Call",
                "Select at least 2 contacts for a conference call.");
        }
    });
    callMenu->addSeparator();
    callMenu->addAction("&Mute Microphone", [this]() {
        QMessageBox::information(this, "Mute",
            "No active call to mute.");
    });

    // Help
    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&Getting Started", [this]() {
        QMessageBox::information(this, "Getting Started",
            "<b>Welcome to Skype!</b><br><br>"
            "1. Add contacts using Contacts > Add a Contact<br>"
            "2. Double-click a contact to start chatting<br>"
            "3. Right-click a contact to call or send files<br>"
            "4. Use the Dial tab to make SkypeOut calls<br>"
            "5. Change your status using the dropdown at the bottom");
    });
    helpMenu->addAction("&Help Topics...", [this]() {
        QMessageBox::information(this, "Help Topics",
            "<b>Skype Help</b><br><br>"
            "<b>Contacts:</b> Add contacts via Contacts menu or toolbar.<br>"
            "<b>Chat:</b> Double-click a contact to open a chat.<br>"
            "<b>Call:</b> Right-click a contact and select Call.<br>"
            "<b>Send Files:</b> Use the toolbar or right-click a contact.<br>"
            "<b>Status:</b> Change your status at the bottom of the window.<br>"
            "<b>Account:</b> Manage your profile via File > My Profile.");
    });
    helpMenu->addSeparator();
    helpMenu->addAction("&Check for Updates...", [this]() {
        QMessageBox::information(this, "Check for Updates",
            "You are running Skype version 1.0.0.106.\n\n"
            "No updates are available at this time.");
    });
    helpMenu->addSeparator();
    helpMenu->addAction("&About Skype", [this]() {
        QMessageBox about(this);
        about.setWindowTitle("About Skype");
        about.setText(
            "<b>Skype</b> version 1.0.0.106<br><br>"
            "Copyright 2003-2004 Skype Technologies S.A.<br><br>"
            "The whole world can talk free.<br><br>"
            "<small>This is a UI recreation of Skype 1.x (2003-2005).</small>"
        );
        about.setIcon(QMessageBox::Information);
        about.exec();
    });
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    toolbar->addAction("Chat", [this]() {
        int id = pickContact("Chat with Contact");
        if (id >= 0) emit contactDoubleClicked(id);
    });
    toolbar->addAction("Call", [this]() {
        int id = pickContact("Call Contact");
        if (id >= 0) emit callContact(id);
    });
    toolbar->addAction("Send File", [this]() {
        int id = pickContact("Send File To");
        if (id >= 0) emit sendFileToContact(id);
    });
    toolbar->addSeparator();
    toolbar->addAction("Add", [this]() {
        AddContactDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && !dlg.skypeName().isEmpty()) {
            emit contactAdded(dlg.skypeName());
        }
    });
    toolbar->addAction("Search", [this]() {
        SearchDialog dlg(m_contacts, this);
        connect(&dlg, &SearchDialog::contactAdded, [this](const QString& name) {
            emit contactAdded(name);
        });
        dlg.exec();
    });
}

void MainWindow::setupCentralWidget() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // User info bar
    auto* userBar = new QWidget(central);
    userBar->setFixedHeight(22);
    auto* userLayout = new QHBoxLayout(userBar);
    userLayout->setContentsMargins(4, 1, 4, 1);

    auto* nameLabel = new QLabel(m_username, userBar);
    QFont f = nameLabel->font();
    f.setBold(true);
    nameLabel->setFont(f);
    userLayout->addWidget(nameLabel);
    userLayout->addStretch();
    layout->addWidget(userBar);

    // Tab widget
    m_tabs = new QTabWidget(central);

    // Contacts tab
    m_contactList = new ContactListWidget();
    m_tabs->addTab(m_contactList, "Contacts");
    connect(m_contactList, &ContactListWidget::contactDoubleClicked,
            this, &MainWindow::contactDoubleClicked);
    connect(m_contactList, &ContactListWidget::callRequested,
            this, &MainWindow::callContact);
    connect(m_contactList, &ContactListWidget::sendFileRequested,
            this, &MainWindow::sendFileToContact);
    connect(m_contactList, &ContactListWidget::viewProfileRequested,
            this, &MainWindow::viewProfileRequested);
    connect(m_contactList, &ContactListWidget::renameRequested,
            this, &MainWindow::renameContact);
    connect(m_contactList, &ContactListWidget::blockRequested,
            this, &MainWindow::blockContact);
    connect(m_contactList, &ContactListWidget::removeRequested,
            this, &MainWindow::removeContact);
    connect(m_contactList, &ContactListWidget::sendContactRequested, [this](int contactId) {
        int recipientId = pickContact("Send Contact To");
        if (recipientId >= 0 && recipientId != contactId) {
            emit sendContactToRecipient(contactId, recipientId);
        }
    });

    // Dial tab
    m_dialPad = new DialPad();
    m_tabs->addTab(m_dialPad, "Dial");
    connect(m_dialPad, &DialPad::callSkypeNumber,
            this, &MainWindow::callSkypeNumber);

    // History tab
    m_historyList = new HistoryList();
    m_tabs->addTab(m_historyList, "History");

    layout->addWidget(m_tabs);

    // Status selector at bottom
    m_statusSelector = new StatusSelector(central);
    connect(m_statusSelector, &StatusSelector::statusChanged,
            this, &MainWindow::statusChanged);
    layout->addWidget(m_statusSelector);

    setCentralWidget(central);
}

void MainWindow::setupStatusBar() {
    statusBar()->showMessage("Ready");
}

void MainWindow::setContacts(const QList<Contact>& contacts) {
    m_contacts = contacts;
    m_contactList->setContacts(contacts);
}

int MainWindow::pickContact(const QString& title) {
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.resize(220, 300);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(6, 6, 6, 6);

    auto* label = new QLabel("Select a contact:", &dlg);
    layout->addWidget(label);

    auto* list = new QListWidget(&dlg);
    for (const auto& c : m_contacts) {
        if (c.isOnline()) {
            auto* item = new QListWidgetItem(c.displayName, list);
            item->setData(Qt::UserRole, c.id);
        }
    }
    // Add offline contacts below
    for (const auto& c : m_contacts) {
        if (!c.isOnline()) {
            auto* item = new QListWidgetItem(c.displayName + "  (Offline)", list);
            item->setData(Qt::UserRole, c.id);
            item->setForeground(QColor("#808080"));
        }
    }
    layout->addWidget(list);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* okBtn = new QPushButton("OK", &dlg);
    okBtn->setMinimumSize(75, 23);
    okBtn->setEnabled(false);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    auto* cancelBtn = new QPushButton("Cancel", &dlg);
    cancelBtn->setMinimumSize(75, 23);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(list, &QListWidget::currentRowChanged, [okBtn](int row) {
        okBtn->setEnabled(row >= 0);
    });
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted && list->currentItem()) {
        return list->currentItem()->data(Qt::UserRole).toInt();
    }
    return -1;
}

QList<int> MainWindow::pickMultipleContacts(const QString& title) {
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.resize(240, 340);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(6, 6, 6, 6);

    layout->addWidget(new QLabel("Select contacts (Ctrl+click for multiple):"));

    auto* list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::MultiSelection);
    for (const auto& c : m_contacts) {
        auto* item = new QListWidgetItem(c.displayName, list);
        item->setData(Qt::UserRole, c.id);
        if (!c.isOnline()) {
            item->setForeground(QColor("#808080"));
        }
    }
    layout->addWidget(list);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* okBtn = new QPushButton("OK", &dlg);
    okBtn->setMinimumSize(75, 23);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    auto* cancelBtn = new QPushButton("Cancel", &dlg);
    cancelBtn->setMinimumSize(75, 23);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    QList<int> result;
    if (dlg.exec() == QDialog::Accepted) {
        for (auto* item : list->selectedItems()) {
            result.append(item->data(Qt::UserRole).toInt());
        }
    }
    return result;
}

void MainWindow::showAccountDialog() {
    QSettings s("SkypeClassic", "SkypeClassic");

    QDialog dlg(this);
    dlg.setWindowTitle("My Account");
    dlg.resize(360, 340);
    dlg.setMinimumSize(300, 280);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* title = new QLabel(QString("<b>Account: %1</b>").arg(m_username));
    layout->addWidget(title);

    // Skype Number (read-only)
    QString skypeNum = s.value("account/skypeNumber", "").toString();
    if (!skypeNum.isEmpty()) {
        auto* numLabel = new QLabel(QString("Skype Number: <b>%1</b>").arg(skypeNum));
        layout->addWidget(numLabel);
    }

    auto* line = new QFrame(&dlg);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    // Display Name
    layout->addWidget(new QLabel("Full Name:"));
    auto* displayNameEdit = new QLineEdit(&dlg);
    displayNameEdit->setText(s.value("account/displayName", "").toString());
    layout->addWidget(displayNameEdit);

    // Mood Text
    layout->addWidget(new QLabel("Mood Text:"));
    auto* moodEdit = new QLineEdit(&dlg);
    moodEdit->setText(s.value("account/moodText", "").toString());
    moodEdit->setPlaceholderText("");
    layout->addWidget(moodEdit);

    // Email
    layout->addWidget(new QLabel("E-mail:"));
    auto* emailEdit = new QLineEdit(&dlg);
    emailEdit->setText(s.value("account/email", "").toString());
    emailEdit->setPlaceholderText("");
    layout->addWidget(emailEdit);

    // Phone
    layout->addWidget(new QLabel("Phone Number:"));
    auto* phoneEdit = new QLineEdit(&dlg);
    phoneEdit->setText(s.value("account/phone", "").toString());
    phoneEdit->setPlaceholderText("");
    layout->addWidget(phoneEdit);

    // Location
    layout->addWidget(new QLabel("Location:"));
    auto* locationEdit = new QLineEdit(&dlg);
    locationEdit->setText(s.value("account/location", "").toString());
    locationEdit->setPlaceholderText("");
    layout->addWidget(locationEdit);

    layout->addStretch();

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* saveBtn = new QPushButton("&Save", &dlg);
    saveBtn->setMinimumSize(75, 23);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        s.setValue("account/displayName", displayNameEdit->text());
        s.setValue("account/moodText", moodEdit->text());
        s.setValue("account/email", emailEdit->text());
        s.setValue("account/phone", phoneEdit->text());
        s.setValue("account/location", locationEdit->text());
        emit profileUpdated(displayNameEdit->text(), moodEdit->text());
        dlg.accept();
    });
    btnLayout->addWidget(saveBtn);

    auto* cancelBtn = new QPushButton("Cancel", &dlg);
    cancelBtn->setMinimumSize(75, 23);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    layout->addLayout(btnLayout);

    dlg.exec();
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    // Scale font based on window width relative to default 280px
    double scale = qBound(0.85, width() / 280.0, 1.6);
    int fontSize = qRound(11 * scale);

    // Update tab font
    QFont tabFont("Tahoma", -1);
    tabFont.setPixelSize(fontSize);
    m_tabs->setFont(tabFont);

    // Update status selector font
    m_statusSelector->setFont(tabFont);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore();
}
