#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QCloseEvent>
#include "models/Contact.h"

class AudioStreamManager;
class VideoStreamManager;

class CallWindow : public QWidget {
    Q_OBJECT

public:
    enum CallState { Ringing, Connected, Ended };

    explicit CallWindow(const Contact& contact, bool incoming = false,
                        const QString& callId = QString(), QWidget* parent = nullptr);
    ~CallWindow();

    int contactId() const { return m_contact.id; }
    QString callId() const { return m_callId; }
    CallState state() const { return m_state; }

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void callEnded(int contactId);
    void callAccepted(int contactId, const QString& callId);
    void callRejected(int contactId, const QString& callId);
    void hangUpRequested(int contactId, const QString& callId);
    void audioToSend(int contactId, const QByteArray& data);
    void videoToSend(int contactId, const QByteArray& jpegData);

public slots:
    void onPeerAccepted();
    void onPeerRejected(const QString& reason);
    void onPeerHungUp();
    void playRemoteAudio(const QByteArray& data);
    void displayRemoteVideo(const QByteArray& jpegData);

private slots:
    void onAnswerClicked();
    void onHangUpClicked();
    void onMuteClicked();
    void onHoldClicked();
    void onVideoClicked();
    void onRingTimeout();
    void updateDuration();

private:
    void setupUi();
    void setState(CallState state);
    void startAudio();
    void stopAudio();
    void startVideo();
    void stopVideo();

    Contact m_contact;
    bool m_incoming;
    QString m_callId;
    CallState m_state;

    QLabel* m_statusLabel;
    QLabel* m_contactNameLabel;
    QLabel* m_remoteVideoLabel;
    QLabel* m_localVideoLabel;

    // D_bevel_frame viewport
    QFrame* m_viewportFrame = nullptr;
    QLabel* m_callIconLabel = nullptr;
    QLabel* m_viewportStatusLabel = nullptr;
    QPushButton* m_answerBtn;
    QPushButton* m_hangUpBtn;
    QPushButton* m_muteBtn;
    QPushButton* m_holdBtn;
    QPushButton* m_videoBtn;

    QTimer* m_durationTimer;
    QTimer* m_ringTimer;
    int m_durationSeconds = 0;
    bool m_muted = false;
    bool m_onHold = false;
    bool m_videoEnabled = false;
    bool m_cleanupEmitted = false;

    AudioStreamManager* m_audio = nullptr;
    VideoStreamManager* m_video = nullptr;
};
