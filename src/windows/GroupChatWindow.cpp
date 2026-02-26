#include "windows/GroupChatWindow.h"
#include "utils/SoundPlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QFont>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QKeyEvent>

GroupChatWindow::GroupChatWindow(const GroupChat& group, const QString& localUser, QWidget* parent)
    : QWidget(parent)
    , m_group(group)
    , m_localUser(localUser)
    , m_typingTimer(new QTimer(this))
    , m_ownTypingTimer(new QTimer(this))
{
    setupUi();
    setWindowTitle(QString("Skype - %1 (%2 members)").arg(m_group.groupName).arg(m_group.members.size()));
    resize(450, 380);
    setMinimumSize(350, 280);

    m_typingTimer->setSingleShot(true);
    connect(m_typingTimer, &QTimer::timeout, this, &GroupChatWindow::onTypingTimeout);

    m_ownTypingTimer->setSingleShot(true);
    m_ownTypingTimer->setInterval(3000);

    loadHistory();
}

GroupChatWindow::~GroupChatWindow() {
    saveHistory();
    qDeleteAll(m_peerTypingTimers);
}

void GroupChatWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Header
    auto* headerLabel = new QLabel(m_group.groupName, this);
    QFont hf = headerLabel->font();
    hf.setBold(true);
    hf.setPixelSize(13);
    headerLabel->setFont(hf);
    mainLayout->addWidget(headerLabel);

    // Content: chat + member list
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Chat area
    auto* chatWidget = new QWidget(splitter);
    auto* chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(2);

    m_chatHistory = new QTextBrowser(chatWidget);
    m_chatHistory->setReadOnly(true);
    m_chatHistory->setOpenExternalLinks(false);
    chatLayout->addWidget(m_chatHistory);

    // Typing indicator
    m_typingLabel = new QLabel("", chatWidget);
    m_typingLabel->setFixedHeight(16);
    QFont tf;
    tf.setPixelSize(10);
    tf.setItalic(true);
    m_typingLabel->setFont(tf);
    m_typingLabel->setStyleSheet("color: #808080;");
    chatLayout->addWidget(m_typingLabel);

    // Input area
    auto* inputRow = new QHBoxLayout();
    m_inputEdit = new QTextEdit(chatWidget);
    m_inputEdit->setFixedHeight(50);
    m_inputEdit->setPlaceholderText("Type a message...");
    connect(m_inputEdit, &QTextEdit::textChanged, this, &GroupChatWindow::onInputChanged);
    inputRow->addWidget(m_inputEdit);

    auto* btnCol = new QVBoxLayout();
    m_sendButton = new QPushButton("&Send", chatWidget);
    m_sendButton->setMinimumSize(55, 23);
    connect(m_sendButton, &QPushButton::clicked, this, &GroupChatWindow::onSendClicked);
    btnCol->addWidget(m_sendButton);
    btnCol->addStretch();
    inputRow->addLayout(btnCol);

    chatLayout->addLayout(inputRow);
    splitter->addWidget(chatWidget);

    // Member panel
    auto* memberWidget = new QWidget(splitter);
    auto* memberLayout = new QVBoxLayout(memberWidget);
    memberLayout->setContentsMargins(0, 0, 0, 0);
    memberLayout->setSpacing(2);

    auto* memberHeader = new QLabel("Members", memberWidget);
    QFont mhf;
    mhf.setBold(true);
    mhf.setPixelSize(11);
    memberHeader->setFont(mhf);
    memberLayout->addWidget(memberHeader);

    m_memberList = new QListWidget(memberWidget);
    m_memberList->setMaximumWidth(140);
    rebuildMemberList();
    memberLayout->addWidget(m_memberList);

    m_inviteButton = new QPushButton("&Invite", memberWidget);
    m_inviteButton->setFixedHeight(23);
    connect(m_inviteButton, &QPushButton::clicked, this, &GroupChatWindow::onInviteClicked);
    memberLayout->addWidget(m_inviteButton);

    m_leaveButton = new QPushButton("&Leave", memberWidget);
    m_leaveButton->setFixedHeight(23);
    connect(m_leaveButton, &QPushButton::clicked, this, &GroupChatWindow::onLeaveClicked);
    memberLayout->addWidget(m_leaveButton);

    splitter->addWidget(memberWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
}

void GroupChatWindow::appendMessage(const QString& sender, const QString& text, bool isLocal) {
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = isLocal ? "#0000CC" : "#CC0000";
    m_chatHistory->append(
        QString("<span style='color:#808080;'>[%1]</span> "
                "<b style='color:%2;'>%3:</b> %4")
            .arg(time, color, sender.toHtmlEscaped(), text.toHtmlEscaped()));
}

void GroupChatWindow::receiveMessage(const QString& sender, const QString& text) {
    appendMessage(sender, text, sender == m_localUser);
    SoundPlayer::instance().play("IM_RECEIVED.WAV");
}

void GroupChatWindow::showTypingIndicator(const QString& sender) {
    if (sender == m_localUser) return;

    // Create or reset per-peer typing timer
    if (!m_peerTypingTimers.contains(sender)) {
        auto* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(5000);
        connect(timer, &QTimer::timeout, [this, sender]() {
            m_peerTypingTimers.remove(sender);
            // Rebuild typing label
            QStringList typers;
            for (auto it = m_peerTypingTimers.begin(); it != m_peerTypingTimers.end(); ++it) {
                typers.append(it.key());
            }
            if (typers.isEmpty()) m_typingLabel->setText("");
            else m_typingLabel->setText(typers.join(", ") + " typing...");
        });
        m_peerTypingTimers.insert(sender, timer);
    }
    m_peerTypingTimers[sender]->start();

    QStringList typers;
    for (auto it = m_peerTypingTimers.begin(); it != m_peerTypingTimers.end(); ++it) {
        typers.append(it.key());
    }
    m_typingLabel->setText(typers.join(", ") + " typing...");
}

void GroupChatWindow::addMember(const QString& username) {
    if (m_group.members.contains(username)) return;
    m_group.members.append(username);
    rebuildMemberList();
    setWindowTitle(QString("Skype - %1 (%2 members)").arg(m_group.groupName).arg(m_group.members.size()));
    m_chatHistory->append(QString("<i style='color:#808080;'>%1 joined the group</i>").arg(username));
}

void GroupChatWindow::removeMember(const QString& username) {
    m_group.members.removeAll(username);
    rebuildMemberList();
    setWindowTitle(QString("Skype - %1 (%2 members)").arg(m_group.groupName).arg(m_group.members.size()));
    m_chatHistory->append(QString("<i style='color:#808080;'>%1 left the group</i>").arg(username));
}

void GroupChatWindow::rebuildMemberList() {
    m_memberList->clear();
    for (const QString& m : m_group.members) {
        QString label = m;
        if (m == m_group.creator) label += " (host)";
        if (m == m_localUser) label += " (you)";
        m_memberList->addItem(label);
    }
}

void GroupChatWindow::onSendClicked() {
    QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    m_inputEdit->clear();
    appendMessage(m_localUser, text, true);
    emit messageSent(m_group.groupId, text);
    SoundPlayer::instance().play("IM_SENT.WAV");
}

void GroupChatWindow::onInputChanged() {
    if (!m_ownTypingTimer->isActive()) {
        emit typingStarted(m_group.groupId);
    }
    m_ownTypingTimer->start();
}

void GroupChatWindow::onTypingTimeout() {
    m_typingLabel->setText("");
}

void GroupChatWindow::onLeaveClicked() {
    emit leaveGroup(m_group.groupId);
    close();
}

void GroupChatWindow::onInviteClicked() {
    emit inviteMember(m_group.groupId, -1); // -1 signals "pick from SkypeApp"
}

void GroupChatWindow::loadHistory() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history";
    QString path = dir + "/group_" + m_group.groupId + ".txt";
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            m_chatHistory->append(in.readLine());
        }
    }
}

void GroupChatWindow::saveHistory() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history";
    QDir().mkpath(dir);
    QString path = dir + "/group_" + m_group.groupId + ".txt";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_chatHistory->toPlainText();
    }
}
