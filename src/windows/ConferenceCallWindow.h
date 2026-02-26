#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QGridLayout>
#include <QMap>

class AudioStreamManager;
class VideoStreamManager;

class ConferenceCallWindow : public QWidget {
    Q_OBJECT

public:
    explicit ConferenceCallWindow(const QString& conferenceId,
                                   const QString& localUser,
                                   const QStringList& participants,
                                   QWidget* parent = nullptr);
    ~ConferenceCallWindow();

    QString conferenceId() const { return m_conferenceId; }
    void addParticipant(const QString& username);
    void removeParticipant(const QString& username);

signals:
    void leaveRequested(const QString& conferenceId);
    void audioToSend(const QString& conferenceId, const QByteArray& data);
    void videoToSend(const QString& conferenceId, const QByteArray& jpegData);

public slots:
    void playRemoteAudio(const QString& from, const QByteArray& data);
    void displayRemoteVideo(const QString& from, const QByteArray& jpegData);

private slots:
    void onLeaveClicked();
    void onMuteClicked();
    void onVideoClicked();
    void updateDuration();

private:
    void setupUi();
    void rebuildVideoGrid();

    QString m_conferenceId;
    QString m_localUser;
    QStringList m_participants;

    QGridLayout* m_videoGrid;
    QMap<QString, QLabel*> m_videoLabels;
    QMap<QString, QLabel*> m_nameLabels;

    QLabel* m_statusLabel;
    QLabel* m_durationLabel;
    QLabel* m_localVideoLabel;
    QPushButton* m_leaveBtn;
    QPushButton* m_muteBtn;
    QPushButton* m_videoBtn;

    QTimer* m_durationTimer;
    int m_durationSeconds = 0;
    bool m_muted = false;
    bool m_videoEnabled = false;

    AudioStreamManager* m_audio = nullptr;
    VideoStreamManager* m_video = nullptr;
};
