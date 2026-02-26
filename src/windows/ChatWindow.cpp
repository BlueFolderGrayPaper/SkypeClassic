#include "windows/ChatWindow.h"
#include "utils/SoundPlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QDateTime>
#include <QKeyEvent>
#include <QToolBar>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QResizeEvent>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QTextDocument>

// Custom QTextEdit that sends on Enter (Shift+Enter for newline)
class ChatInputEdit : public QTextEdit {
public:
    using QTextEdit::QTextEdit;
    std::function<void()> onEnter;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return && !(event->modifiers() & Qt::ShiftModifier)) {
            if (onEnter) onEnter();
            return;
        }
        QTextEdit::keyPressEvent(event);
    }
};

ChatWindow::ChatWindow(const Contact& contact, QWidget* parent)
    : QWidget(parent)
    , m_contact(contact)
    , m_typingTimer(new QTimer(this))
    , m_ownTypingTimer(new QTimer(this))
{
    setupUi();
    setWindowTitle(QString("Chat with %1").arg(m_contact.displayName));
    resize(380, 320);

    m_typingTimer->setSingleShot(true);
    connect(m_typingTimer, &QTimer::timeout, this, &ChatWindow::hideTypingIndicator);

    m_ownTypingTimer->setSingleShot(true);
    m_ownTypingTimer->setInterval(3000);

    loadHistory();
}

ChatWindow::~ChatWindow() {
    saveHistory();
}

void ChatWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    // Top bar
    auto* topBar = new QWidget(this);
    topBar->setFixedHeight(24);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(4, 1, 4, 1);

    auto* contactLabel = new QLabel(m_contact.displayName, topBar);
    QFont f = contactLabel->font();
    f.setBold(true);
    contactLabel->setFont(f);
    topLayout->addWidget(contactLabel);

    auto* statusLabel = new QLabel(QString("(%1)").arg(m_contact.statusString()), topBar);
    topLayout->addWidget(statusLabel);

    topLayout->addStretch();

    auto* callBtn = new QPushButton("Call", topBar);
    callBtn->setMinimumSize(50, 20);
    connect(callBtn, &QPushButton::clicked, []() {
        SoundPlayer::instance().play("CALL_OUT.WAV");
    });
    topLayout->addWidget(callBtn);

    mainLayout->addWidget(topBar);

    // Splitter: chat history (top) + input (bottom)
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // Chat history
    m_chatHistory = new QTextBrowser(splitter);
    m_chatHistory->setOpenExternalLinks(false);
    m_chatHistory->setOpenLinks(false);
    m_chatHistory->setReadOnly(true);
    connect(m_chatHistory, &QTextBrowser::anchorClicked, this, &ChatWindow::onAnchorClicked);
    m_chatHistory->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_chatHistory, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QMenu menu(m_chatHistory);
        auto* copyAction = menu.addAction("&Copy", [this]() {
            m_chatHistory->copy();
        });
        copyAction->setEnabled(m_chatHistory->textCursor().hasSelection());
        menu.addAction("Select &All", [this]() {
            m_chatHistory->selectAll();
        });
        menu.addSeparator();
        menu.addAction("Clear &History", [this]() {
            m_chatHistory->clear();
            m_messages.clear();
        });
        menu.exec(m_chatHistory->mapToGlobal(pos));
    });

    // Input area
    auto* inputWidget = new QWidget(splitter);
    auto* inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(2);

    auto* chatInput = new ChatInputEdit(inputWidget);
    m_inputEdit = chatInput;

    chatInput->onEnter = [this]() { onSendClicked(); };
    chatInput->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(chatInput, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QMenu menu(m_inputEdit);
        menu.addAction("Cu&t", [this]() { m_inputEdit->cut(); });
        menu.addAction("&Copy", [this]() { m_inputEdit->copy(); });
        menu.addAction("&Paste", [this]() { m_inputEdit->paste(); });
        menu.addSeparator();
        menu.addAction("Select &All", [this]() { m_inputEdit->selectAll(); });
        menu.exec(m_inputEdit->mapToGlobal(pos));
    });
    connect(m_inputEdit, &QTextEdit::textChanged, this, &ChatWindow::onInputChanged);
    connect(m_inputEdit->document(), &QTextDocument::contentsChanged, this, &ChatWindow::adjustInputHeight);

    m_inputEdit->setMinimumHeight(30);
    m_inputEdit->setMaximumHeight(100);

    inputLayout->addWidget(m_inputEdit);

    // Typing indicator
    m_typingLabel = new QLabel(inputWidget);
    m_typingLabel->setStyleSheet("color: #808080; font-size: 10px; padding: 0px 2px;");
    m_typingLabel->setFixedHeight(14);
    m_typingLabel->hide();
    inputLayout->addWidget(m_typingLabel);

    // Send button row
    auto* sendRow = new QHBoxLayout();
    sendRow->addStretch();

    m_attachButton = new QPushButton("Attach", inputWidget);
    m_attachButton->setMinimumSize(55, 21);
    connect(m_attachButton, &QPushButton::clicked, this, &ChatWindow::onAttachClicked);
    sendRow->addWidget(m_attachButton);

    m_sendButton = new QPushButton("&Send", inputWidget);
    m_sendButton->setMinimumSize(55, 21);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    sendRow->addWidget(m_sendButton);
    inputLayout->addLayout(sendRow);

    splitter->addWidget(m_chatHistory);
    splitter->addWidget(inputWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    m_inputEdit->setFocus();
}

void ChatWindow::onSendClicked() {
    QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    appendMessage("Me", text, true);
    emit messageSent(m_contact.id, text);
    m_inputEdit->clear();

    SoundPlayer::instance().play("IM_SENT.WAV");
}

void ChatWindow::onAttachClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Attach File");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    QFileInfo fi(filePath);

    // 5MB limit for inline attachments
    if (fi.size() > 5 * 1024 * 1024) {
        appendMessage("System", "File too large (max 5 MB).", false);
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        appendMessage("System", "Could not read file.", false);
        return;
    }

    QByteArray data = file.readAll();
    QString fileName = fi.fileName();

    // Show in our own chat
    appendMessage("Me", QString("[File: %1 (%2 KB)]").arg(fileName).arg(fi.size() / 1024), true);

    emit fileAttached(m_contact.id, fileName, data);
    SoundPlayer::instance().play("IM_SENT.WAV");
}

void ChatWindow::receiveMessage(const QString& sender, const QString& text) {
    hideTypingIndicator();
    appendMessage(sender, text, false);
    SoundPlayer::instance().play("IM.WAV");
}

void ChatWindow::receiveFileAttachment(const QString& sender, const QString& fileName, const QString& savedPath) {
    hideTypingIndicator();

    QSettings settings("SkypeClassic", "SkypeClassic");
    bool showTimestamps = settings.value("general/showTimestamps", true).toBool();

    QString timeStr = QDateTime::currentDateTime().toString("h:mm AP");
    QString link = QString("<a href='file:///%1'>%2</a>").arg(
        savedPath.toHtmlEscaped(), fileName.toHtmlEscaped());

    if (showTimestamps) {
        m_chatHistory->append(
            QString("<span style='color: #000000; font-weight: bold;'>[%1] %2:</span> [File] %3")
                .arg(timeStr, sender.toHtmlEscaped(), link));
    } else {
        m_chatHistory->append(
            QString("<span style='color: #000000; font-weight: bold;'>%1:</span> [File] %2")
                .arg(sender.toHtmlEscaped(), link));
    }

    SoundPlayer::instance().play("IM.WAV");
}

void ChatWindow::onAnchorClicked(const QUrl& url) {
    QDesktopServices::openUrl(url);
}

void ChatWindow::showTypingIndicator(const QString& sender) {
    m_typingLabel->setText(QString("%1 is typing...").arg(sender));
    m_typingLabel->show();
    m_typingTimer->start(5000);
}

void ChatWindow::hideTypingIndicator() {
    m_typingLabel->hide();
    m_typingTimer->stop();
}

void ChatWindow::onTypingTimeout() {
    hideTypingIndicator();
}

void ChatWindow::onInputChanged() {
    if (m_inputEdit->toPlainText().trimmed().isEmpty()) return;

    if (!m_ownTypingTimer->isActive()) {
        emit typingStarted(m_contact.id);
    }
    m_ownTypingTimer->start();
}

void ChatWindow::appendMessage(const QString& sender, const QString& text, bool isOutgoing) {
    Message msg;
    msg.sender = sender;
    msg.text = text;
    msg.timestamp = QDateTime::currentDateTime();
    msg.isOutgoing = isOutgoing;
    m_messages.append(msg);

    QSettings settings("SkypeClassic", "SkypeClassic");
    bool showTimestamps = settings.value("general/showTimestamps", true).toBool();

    QString displayText = text.toHtmlEscaped();
    QString timeStr = msg.timestamp.toString("h:mm AP");
    QString color = isOutgoing ? "#00008B" : "#000000";

    if (showTimestamps) {
        m_chatHistory->append(
            QString("<span style='color: %1; font-weight: bold;'>[%2] %3:</span> %4")
                .arg(color, timeStr, sender.toHtmlEscaped(), displayText)
        );
    } else {
        m_chatHistory->append(
            QString("<span style='color: %1; font-weight: bold;'>%2:</span> %3")
                .arg(color, sender.toHtmlEscaped(), displayText)
        );
    }
}

void ChatWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    double scale = qBound(0.85, width() / 380.0, 1.6);
    int fontSize = qRound(11 * scale);

    QFont f("Tahoma", -1);
    f.setPixelSize(fontSize);
    m_inputEdit->setFont(f);

    // Scale buttons
    int btnW = qRound(55 * scale);
    int btnH = qRound(21 * scale);
    m_sendButton->setMinimumSize(btnW, btnH);
    m_attachButton->setMinimumSize(btnW, btnH);
}

void ChatWindow::loadHistory() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir + "/history");
    QString histFile = dataDir + "/history/" + m_contact.skypeName + ".txt";

    QFile file(histFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        // Format: timestamp|sender|isOutgoing|text
        QStringList parts = line.split('|');
        if (parts.size() < 4) continue;

        QDateTime ts = QDateTime::fromString(parts[0], Qt::ISODate);
        QString sender = parts[1];
        bool isOutgoing = parts[2] == "1";
        QString text = parts.mid(3).join('|'); // In case text contained |

        Message msg;
        msg.timestamp = ts;
        msg.sender = sender;
        msg.isOutgoing = isOutgoing;
        msg.text = text;
        m_messages.append(msg);

        QSettings settings("SkypeClassic", "SkypeClassic");
        bool showTimestamps = settings.value("general/showTimestamps", true).toBool();

        QString displayText = text.toHtmlEscaped();

        QString timeStr = ts.toString("h:mm AP");
        QString color = isOutgoing ? "#00008B" : "#000000";

        if (showTimestamps) {
            m_chatHistory->append(
                QString("<span style='color: %1; font-weight: bold;'>[%2] %3:</span> %4")
                    .arg(color, timeStr, sender.toHtmlEscaped(), displayText));
        } else {
            m_chatHistory->append(
                QString("<span style='color: %1; font-weight: bold;'>%2:</span> %3")
                    .arg(color, sender.toHtmlEscaped(), displayText));
        }
    }

    if (!m_messages.isEmpty()) {
        m_chatHistory->append("<hr><span style='color: #808080; font-size: 10px;'>--- Previous messages ---</span><hr>");
    }
}

void ChatWindow::adjustInputHeight() {
    QTextDocument* doc = m_inputEdit->document();
    int docHeight = qRound(doc->size().height());
    int margins = m_inputEdit->contentsMargins().top() + m_inputEdit->contentsMargins().bottom() + 6;
    int newHeight = qBound(30, docHeight + margins, 100);
    m_inputEdit->setFixedHeight(newHeight);
}

void ChatWindow::saveHistory() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir + "/history");
    QString histFile = dataDir + "/history/" + m_contact.skypeName + ".txt";

    QFile file(histFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    for (const Message& msg : m_messages) {
        out << msg.timestamp.toString(Qt::ISODate) << '|'
            << msg.sender << '|'
            << (msg.isOutgoing ? "1" : "0") << '|'
            << msg.text << '\n';
    }
}
