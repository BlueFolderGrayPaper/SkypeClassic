#include "windows/CallWindow.h"
#include "audio/AudioStreamManager.h"
#include "audio/VideoStreamManager.h"
#include "utils/SoundPlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPixmap>
#include <QDebug>

CallWindow::CallWindow(const Contact& contact, bool incoming, const QString& callId, QWidget* parent)
    : QWidget(parent)
    , m_contact(contact)
    , m_incoming(incoming)
    , m_callId(callId)
    , m_state(Ringing)
    , m_durationTimer(new QTimer(this))
    , m_ringTimer(new QTimer(this))
    , m_audio(new AudioStreamManager(this))
    , m_video(new VideoStreamManager(this))
{
    setupUi();
    setWindowTitle(QString("Skype - Call with %1").arg(m_contact.displayName));
    resize(270, 280);
    setMinimumSize(220, 200);

    connect(m_durationTimer, &QTimer::timeout, this, &CallWindow::updateDuration);

    // Ring timeout — 30 seconds for outgoing, 60 for incoming
    m_ringTimer->setSingleShot(true);
    m_ringTimer->setInterval(m_incoming ? 60000 : 30000);
    connect(m_ringTimer, &QTimer::timeout, this, &CallWindow::onRingTimeout);

    // Forward captured audio
    connect(m_audio, &AudioStreamManager::audioCaptured, [this](const QByteArray& data) {
        if (m_state == Connected && !m_onHold) {
            emit audioToSend(m_contact.id, data);
        }
    });

    // Forward captured video
    connect(m_video, &VideoStreamManager::frameCaptured, [this](const QByteArray& jpegData) {
        if (m_state == Connected && !m_onHold && m_videoEnabled) {
            emit videoToSend(m_contact.id, jpegData);
        }
    });

    // Show local preview
    connect(m_video, &VideoStreamManager::localFrameReady, [this](const QImage& image) {
        if (m_localVideoLabel->isVisible()) {
            m_localVideoLabel->setPixmap(QPixmap::fromImage(image).scaled(
                m_localVideoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
        }
    });

    setState(Ringing);

    if (m_incoming) {
        SoundPlayer::instance().play("CALL_IN.WAV");
    } else {
        SoundPlayer::instance().play("CALL_OUT.WAV");
    }
}

CallWindow::~CallWindow() {
    stopAudio();
    stopVideo();
    m_durationTimer->stop();
    m_ringTimer->stop();
}

void CallWindow::closeEvent(QCloseEvent* event) {
    // If call is still active (user closed via X button), hang up first
    if (m_state != Ended) {
        if (m_state == Ringing && m_incoming) {
            emit callRejected(m_contact.id, m_callId);
        } else {
            emit hangUpRequested(m_contact.id, m_callId);
        }
        setState(Ended);
    }
    // Ensure cleanup signal is emitted exactly once
    if (!m_cleanupEmitted) {
        m_cleanupEmitted = true;
        emit callEnded(m_contact.id);
    }
    event->accept();
}

void CallWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 6);
    mainLayout->setSpacing(4);

    // ── Top section: name + status on gray ──
    m_contactNameLabel = new QLabel(m_contact.displayName, this);
    QFont nf("Tahoma", -1);
    nf.setPixelSize(11);
    nf.setBold(true);
    m_contactNameLabel->setFont(nf);
    mainLayout->addWidget(m_contactNameLabel);

    m_statusLabel = new QLabel("Calling...", this);
    QFont sf("Tahoma", -1);
    sf.setPixelSize(10);
    m_statusLabel->setFont(sf);
    m_statusLabel->setStyleSheet("color: #808080;");
    mainLayout->addWidget(m_statusLabel);

    // ── Etched separator ──
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep1);

    // ── Dark viewport (3D inset beveled frame) ──
    m_viewportFrame = new QFrame(this);
    m_viewportFrame->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_viewportFrame->setLineWidth(2);
    m_viewportFrame->setStyleSheet("QFrame { background-color: #444444; }");

    auto* vpLayout = new QVBoxLayout(m_viewportFrame);
    vpLayout->setContentsMargins(0, 0, 0, 0);
    vpLayout->setSpacing(2);
    vpLayout->addStretch();

    // Large icon label (phone/pause/X)
    m_callIconLabel = new QLabel(this);
    m_callIconLabel->setAlignment(Qt::AlignCenter);
    QFont iconFont("Tahoma", -1);
    iconFont.setPixelSize(48);
    iconFont.setBold(true);
    m_callIconLabel->setFont(iconFont);
    m_callIconLabel->setStyleSheet("color: #FFFFFF; background: transparent;");
    vpLayout->addWidget(m_callIconLabel);

    // Status text inside viewport
    m_viewportStatusLabel = new QLabel(this);
    m_viewportStatusLabel->setAlignment(Qt::AlignCenter);
    QFont vsf("Tahoma", -1);
    vsf.setPixelSize(11);
    m_viewportStatusLabel->setFont(vsf);
    m_viewportStatusLabel->setStyleSheet("color: #AAAAAA; background: transparent;");
    vpLayout->addWidget(m_viewportStatusLabel);

    vpLayout->addStretch();

    // Remote video (hidden, replaces icon when active)
    m_remoteVideoLabel = new QLabel(m_viewportFrame);
    m_remoteVideoLabel->setAlignment(Qt::AlignCenter);
    m_remoteVideoLabel->setMinimumSize(320, 240);
    m_remoteVideoLabel->setStyleSheet("background: #000;");
    m_remoteVideoLabel->setVisible(false);
    vpLayout->addWidget(m_remoteVideoLabel);

    // Local video preview (overlaid, bottom-right of viewport)
    m_localVideoLabel = new QLabel(m_viewportFrame);
    m_localVideoLabel->setAlignment(Qt::AlignCenter);
    m_localVideoLabel->setFixedSize(120, 90);
    m_localVideoLabel->setStyleSheet("background: #222; border: 1px solid #555;");
    m_localVideoLabel->setVisible(false);

    mainLayout->addWidget(m_viewportFrame, 1);

    // ── Buttons row (single row on gray) ──
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(4);

    m_answerBtn = new QPushButton("&Answer", this);
    m_answerBtn->setMinimumSize(56, 20);
    m_answerBtn->setVisible(m_incoming);
    connect(m_answerBtn, &QPushButton::clicked, this, &CallWindow::onAnswerClicked);
    btnRow->addWidget(m_answerBtn);

    m_muteBtn = new QPushButton("M&ute", this);
    m_muteBtn->setMinimumSize(56, 20);
    m_muteBtn->setEnabled(false);
    connect(m_muteBtn, &QPushButton::clicked, this, &CallWindow::onMuteClicked);
    btnRow->addWidget(m_muteBtn);

    m_holdBtn = new QPushButton("Ho&ld", this);
    m_holdBtn->setMinimumSize(56, 20);
    m_holdBtn->setEnabled(false);
    connect(m_holdBtn, &QPushButton::clicked, this, &CallWindow::onHoldClicked);
    btnRow->addWidget(m_holdBtn);

    m_videoBtn = new QPushButton("&Video", this);
    m_videoBtn->setMinimumSize(56, 20);
    m_videoBtn->setEnabled(false);
    connect(m_videoBtn, &QPushButton::clicked, this, &CallWindow::onVideoClicked);
    btnRow->addWidget(m_videoBtn);

    btnRow->addStretch();

    m_hangUpBtn = new QPushButton("&Hang Up", this);
    m_hangUpBtn->setMinimumSize(60, 20);
    connect(m_hangUpBtn, &QPushButton::clicked, this, &CallWindow::onHangUpClicked);
    btnRow->addWidget(m_hangUpBtn);

    mainLayout->addLayout(btnRow);
}

void CallWindow::setState(CallState state) {
    m_state = state;

    switch (state) {
        case Ringing:
            m_statusLabel->setText(m_incoming ? "Incoming call..." : "Calling...");
            m_callIconLabel->setText(QString::fromUtf8("\xe2\x98\x8e")); // ☎
            m_viewportStatusLabel->setText(m_incoming ? "Incoming call..." : "Calling...");
            m_muteBtn->setEnabled(false);
            m_holdBtn->setEnabled(false);
            m_videoBtn->setEnabled(false);
            m_ringTimer->start();
            break;
        case Connected:
            m_ringTimer->stop();
            m_statusLabel->setText("Connected  -  00:00");
            m_callIconLabel->setText("");
            m_viewportStatusLabel->setText("Connected");
            m_answerBtn->setVisible(false);
            m_muteBtn->setEnabled(true);
            m_holdBtn->setEnabled(true);
            m_videoBtn->setEnabled(true);
            m_durationSeconds = 0;
            m_durationTimer->start(1000);
            startAudio();
            qDebug() << "Call connected with" << m_contact.displayName;
            break;
        case Ended:
            m_ringTimer->stop();
            m_statusLabel->setText("Call ended");
            m_callIconLabel->setText(QString::fromUtf8("\xe2\x9c\x95")); // ✕
            m_viewportStatusLabel->setText("Call ended");
            m_durationTimer->stop();
            m_answerBtn->setVisible(false);
            m_muteBtn->setEnabled(false);
            m_holdBtn->setEnabled(false);
            m_videoBtn->setEnabled(false);
            m_hangUpBtn->setText("&Close");
            stopAudio();
            stopVideo();
            SoundPlayer::instance().play("HANGUP.WAV");
            break;
    }
}

void CallWindow::startAudio() {
    qDebug() << "Starting audio capture and playback";
    m_audio->startCapture();
    m_audio->startPlayback();
}

void CallWindow::stopAudio() {
    m_audio->stopCapture();
    m_audio->stopPlayback();
}

void CallWindow::startVideo() {
    m_video->startCapture();
    m_callIconLabel->setVisible(false);
    m_viewportStatusLabel->setVisible(false);
    m_remoteVideoLabel->setVisible(true);
    m_localVideoLabel->setVisible(true);
    resize(480, 400);
}

void CallWindow::stopVideo() {
    m_video->stopCapture();
    m_videoEnabled = false;
    m_remoteVideoLabel->setVisible(false);
    m_localVideoLabel->setVisible(false);
    m_callIconLabel->setVisible(true);
    m_viewportStatusLabel->setVisible(true);
    m_videoBtn->setText("&Video");
    if (m_state == Connected) {
        resize(270, 280);
    }
}

void CallWindow::onAnswerClicked() {
    qDebug() << "Answering call from" << m_contact.displayName << "callId:" << m_callId;
    setState(Connected);
    emit callAccepted(m_contact.id, m_callId);
}

void CallWindow::onHangUpClicked() {
    if (m_state == Ended) {
        // Window is showing "Call ended" — user clicks Close to dismiss
        if (!m_cleanupEmitted) {
            m_cleanupEmitted = true;
            emit callEnded(m_contact.id);
        }
        close();
    } else {
        // Active call or ringing — hang up / decline
        if (m_state == Ringing && m_incoming) {
            emit callRejected(m_contact.id, m_callId);
        } else {
            // Outgoing ringing or connected — tell peer we're ending
            emit hangUpRequested(m_contact.id, m_callId);
        }
        setState(Ended);
        // DON'T emit callEnded here — let user see "Call ended" and click Close
    }
}

void CallWindow::onPeerAccepted() {
    qDebug() << "Peer accepted call:" << m_contact.displayName;
    if (m_state == Ringing) {
        setState(Connected);
    }
}

void CallWindow::onPeerRejected(const QString& reason) {
    Q_UNUSED(reason);
    qDebug() << "Peer rejected call:" << m_contact.displayName;
    if (m_state == Ringing) {
        setState(Ended);
        m_statusLabel->setText("Call rejected");
        m_viewportStatusLabel->setText("Call rejected");
    }
}

void CallWindow::onPeerHungUp() {
    qDebug() << "Peer hung up:" << m_contact.displayName;
    if (m_state != Ended) {
        setState(Ended);
    }
}

void CallWindow::onRingTimeout() {
    if (m_state == Ringing) {
        bool wasIncoming = m_incoming;
        if (!wasIncoming) {
            emit hangUpRequested(m_contact.id, m_callId);
        }
        setState(Ended);
        QString text = wasIncoming ? "Missed call" : "No answer";
        m_statusLabel->setText(text);
        m_viewportStatusLabel->setText(text);
    }
}

void CallWindow::playRemoteAudio(const QByteArray& data) {
    if (m_state == Connected && !m_onHold) {
        m_audio->playAudioData(data);
    }
}

void CallWindow::displayRemoteVideo(const QByteArray& jpegData) {
    if (m_state != Connected) return;

    // Show remote video panel if not already visible
    if (!m_remoteVideoLabel->isVisible()) {
        m_remoteVideoLabel->setVisible(true);
        if (width() < 480) resize(480, 400);
    }

    QPixmap pix;
    pix.loadFromData(jpegData, "JPEG");
    if (!pix.isNull()) {
        m_remoteVideoLabel->setPixmap(pix.scaled(
            m_remoteVideoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void CallWindow::onMuteClicked() {
    m_muted = !m_muted;
    m_audio->setMuted(m_muted);
    m_muteBtn->setText(m_muted ? "Un&mute" : "M&ute");
}

void CallWindow::onHoldClicked() {
    m_onHold = !m_onHold;
    m_holdBtn->setText(m_onHold ? "Res&ume" : "Ho&ld");
    if (m_onHold) {
        int mins = m_durationSeconds / 60;
        int secs = m_durationSeconds % 60;
        QString dur = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
        m_statusLabel->setText(QString("On Hold  -  %1").arg(dur));
        m_callIconLabel->setText(QString::fromUtf8("\xe2\x8f\xb8")); // ⏸
        m_viewportStatusLabel->setText("On Hold");
        m_audio->stopCapture();
        if (m_videoEnabled) m_video->stopCapture();
        SoundPlayer::instance().play("HOLD.WAV");
    } else {
        m_statusLabel->setText("Connected  -  00:00");
        m_callIconLabel->setText("");
        m_viewportStatusLabel->setText("Connected");
        m_audio->startCapture();
        if (m_videoEnabled) m_video->startCapture();
        SoundPlayer::instance().play("RESUME.WAV");
    }
}

void CallWindow::onVideoClicked() {
    if (!m_videoEnabled) {
        m_videoEnabled = true;
        m_videoBtn->setText("No &Video");
        startVideo();
    } else {
        stopVideo();
    }
}

void CallWindow::updateDuration() {
    m_durationSeconds++;
    int mins = m_durationSeconds / 60;
    int secs = m_durationSeconds % 60;
    QString dur = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));

    if (m_onHold) {
        m_statusLabel->setText(QString("On Hold  -  %1").arg(dur));
    } else {
        m_statusLabel->setText(QString("Connected  -  %1").arg(dur));
    }
}
