#pragma once

#include <QObject>
#include <QMap>
#include <QStringList>

struct ConferenceInfo {
    QString conferenceId;
    QString host;
    QStringList participants; // includes host
    bool active = true;
};

class ConferenceManager : public QObject {
    Q_OBJECT

public:
    explicit ConferenceManager(QObject* parent = nullptr);

    QString createConference(const QString& host, const QStringList& participants);
    bool joinConference(const QString& conferenceId, const QString& username);
    void leaveConference(const QString& conferenceId, const QString& username);
    void endConference(const QString& conferenceId);

    ConferenceInfo* getConference(const QString& conferenceId);
    QStringList getParticipants(const QString& conferenceId) const;
    bool isInConference(const QString& conferenceId, const QString& username) const;

signals:
    void conferenceCreated(const QString& conferenceId);
    void participantJoined(const QString& conferenceId, const QString& username);
    void participantLeft(const QString& conferenceId, const QString& username);
    void conferenceEnded(const QString& conferenceId);

private:
    QMap<QString, ConferenceInfo> m_conferences;
};
