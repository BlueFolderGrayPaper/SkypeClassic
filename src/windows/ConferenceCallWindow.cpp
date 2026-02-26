#include "windows/ConferenceCallWindow.h"
#include "audio/AudioStreamManager.h"
#include "audio/VideoStreamManager.h"
#include "utils/SoundPlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPixmap>

ConferenceCallWindow::ConferenceCallWindow(const QString& conferenceId,
                                             const QString& localUser,
                                             const QStringList& participants,
                                             QWidget* parent)
    : QWidget(parent)
    , m_conferenceId(conferenceId)
    , m_localUser(localUser)
    , m_participants(participants)
    , m_durationTimer(new QTimer(this))
    , m_audio(new AudioStreamManager(this))
    , m_video(new VideoStreamManager(this))
{
    setupUi();
    setWindowTitle(QString("Skype - Conference Call (%1 participants)").arg(m_participants.size()));
    resize(480, 400);
    setMinimumSize(360, 300);

    connect(m_durationTimer, &QTimer::timeout, this, &ConferenceCallWindow::updateDuration);

    // Forward captured audio
    connect(m_audio, &AudioStreamManager::audioCaptured, [this](const QByteArray& data) {
        if (!m_muted) {
            emit audioToSend(m_conferenceId, data);
        }
    });

    // Forward captured video
    connect(m_video, &VideoStreamManager::frameCaptured, [this](const QByteArray& jpegData) {
        if (m_videoEnabled) {
            emit videoToSend(m_conferenceId, jpegData);
        }
    });

    // Show local preview
    connect(m_video, &VideoStreamManager::localFrameReady, [this](const QImage& image) {
        if (m_localVideoLabel && m_localVideoLabel->isVisible()) {
            m_localVideoLabel->setPixmap(QPixmap::fromImage(image).scaled(
                m_localVideoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
        }
    });

    // Start audio immediately
    m_audio->startCapture();
    m_audio->startPlayback();
    m_durationTimer->start(1000);

    SoundPlayer::instance().play("CALL_IN.WAV");
}

ConferenceCallWindow::~ConferenceCallWindow() {
    m_audio->stopCapture();
    m_audio->stopPlayback();
    m_video->stopCapture();
    m_durationTimer->stop();
}

void ConferenceCallWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Status
    m_statusLabel = new QLabel(QString("Conference Call - %1 participants").arg(m_participants.size()), this);
    QFont f = m_statusLabel->font();
    f.setBold(true);
    f.setPixelSize(13);
    m_statusLabel->setFont(f);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    // Duration
    m_durationLabel = new QLabel("00:00", this);
    m_durationLabel->setAlignment(Qt::AlignCenter);
    QFont df = m_durationLabel->font();
    df.setPixelSize(16);
    df.setBold(true);
    m_durationLabel->setFont(df);
    mainLayout->addWidget(m_durationLabel);

    // Video grid
    auto* gridWidget = new QWidget(this);
    m_videoGrid = new QGridLayout(gridWidget);
    m_videoGrid->setSpacing(4);
    mainLayout->addWidget(gridWidget, 1);

    // Build initial grid
    rebuildVideoGrid();

    // Local preview
    m_localVideoLabel = new QLabel(this);
    m_localVideoLabel->setAlignment(Qt::AlignCenter);
    m_localVideoLabel->setFixedSize(120, 90);
    m_localVideoLabel->setStyleSheet("background: #222; border: 1px solid #555;");
    m_localVideoLabel->setVisible(false);
    mainLayout->addWidget(m_localVideoLabel, 0, Qt::AlignRight);

    // Separator
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // Buttons
    auto* btnLayout = new QHBoxLayout();

    m_muteBtn = new QPushButton("M&ute", this);
    m_muteBtn->setMinimumSize(75, 23);
    connect(m_muteBtn, &QPushButton::clicked, this, &ConferenceCallWindow::onMuteClicked);
    btnLayout->addWidget(m_muteBtn);

    m_videoBtn = new QPushButton("&Video", this);
    m_videoBtn->setMinimumSize(75, 23);
    connect(m_videoBtn, &QPushButton::clicked, this, &ConferenceCallWindow::onVideoClicked);
    btnLayout->addWidget(m_videoBtn);

    btnLayout->addStretch();

    m_leaveBtn = new QPushButton("&Leave", this);
    m_leaveBtn->setMinimumSize(75, 23);
    connect(m_leaveBtn, &QPushButton::clicked, this, &ConferenceCallWindow::onLeaveClicked);
    btnLayout->addWidget(m_leaveBtn);

    mainLayout->addLayout(btnLayout);
}

void ConferenceCallWindow::rebuildVideoGrid() {
    // Clear existing grid
    QLayoutItem* item;
    while ((item = m_videoGrid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_videoLabels.clear();
    m_nameLabels.clear();

    // Create a slot for each remote participant
    QStringList remoteParticipants;
    for (const QString& p : m_participants) {
        if (p != m_localUser) remoteParticipants.append(p);
    }

    int cols = remoteParticipants.size() <= 2 ? 2 : 3;
    for (int i = 0; i < remoteParticipants.size(); ++i) {
        const QString& name = remoteParticipants[i];

        auto* container = new QWidget();
        auto* vl = new QVBoxLayout(container);
        vl->setContentsMargins(2, 2, 2, 2);
        vl->setSpacing(2);

        auto* videoLabel = new QLabel();
        videoLabel->setAlignment(Qt::AlignCenter);
        videoLabel->setMinimumSize(120, 90);
        videoLabel->setStyleSheet("background: #000;");
        videoLabel->setText(name.left(1).toUpper());
        QFont vf;
        vf.setPixelSize(24);
        vf.setBold(true);
        videoLabel->setFont(vf);
        vl->addWidget(videoLabel);

        auto* nameLabel = new QLabel(name);
        nameLabel->setAlignment(Qt::AlignCenter);
        QFont nf;
        nf.setPixelSize(10);
        nameLabel->setFont(nf);
        vl->addWidget(nameLabel);

        int row = i / cols;
        int col = i % cols;
        m_videoGrid->addWidget(container, row, col);

        m_videoLabels.insert(name, videoLabel);
        m_nameLabels.insert(name, nameLabel);
    }
}

void ConferenceCallWindow::addParticipant(const QString& username) {
    if (m_participants.contains(username)) return;
    m_participants.append(username);
    rebuildVideoGrid();
    m_statusLabel->setText(QString("Conference Call - %1 participants").arg(m_participants.size()));
    setWindowTitle(QString("Skype - Conference Call (%1 participants)").arg(m_participants.size()));
}

void ConferenceCallWindow::removeParticipant(const QString& username) {
    m_participants.removeAll(username);
    rebuildVideoGrid();
    m_statusLabel->setText(QString("Conference Call - %1 participants").arg(m_participants.size()));
    setWindowTitle(QString("Skype - Conference Call (%1 participants)").arg(m_participants.size()));
}

void ConferenceCallWindow::playRemoteAudio(const QString& from, const QByteArray& data) {
    Q_UNUSED(from);
    // Mix all audio into the single output (simple: just play each stream as it arrives)
    m_audio->playAudioData(data);
}

void ConferenceCallWindow::displayRemoteVideo(const QString& from, const QByteArray& jpegData) {
    if (!m_videoLabels.contains(from)) return;

    QPixmap pix;
    pix.loadFromData(jpegData, "JPEG");
    if (!pix.isNull()) {
        m_videoLabels[from]->setPixmap(pix.scaled(
            m_videoLabels[from]->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ConferenceCallWindow::onLeaveClicked() {
    m_audio->stopCapture();
    m_audio->stopPlayback();
    m_video->stopCapture();
    m_durationTimer->stop();
    emit leaveRequested(m_conferenceId);
    SoundPlayer::instance().play("HANGUP.WAV");
    close();
}

void ConferenceCallWindow::onMuteClicked() {
    m_muted = !m_muted;
    m_audio->setMuted(m_muted);
    m_muteBtn->setText(m_muted ? "Un&mute" : "M&ute");
}

void ConferenceCallWindow::onVideoClicked() {
    if (!m_videoEnabled) {
        m_videoEnabled = true;
        m_videoBtn->setText("No &Video");
        m_video->startCapture();
        m_localVideoLabel->setVisible(true);
    } else {
        m_videoEnabled = false;
        m_videoBtn->setText("&Video");
        m_video->stopCapture();
        m_localVideoLabel->setVisible(false);
    }
}

void ConferenceCallWindow::updateDuration() {
    m_durationSeconds++;
    int mins = m_durationSeconds / 60;
    int secs = m_durationSeconds % 60;
    m_durationLabel->setText(QString("%1:%2")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0')));
}
