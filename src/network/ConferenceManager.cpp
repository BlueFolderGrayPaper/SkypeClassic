#include "network/ConferenceManager.h"
#include <QUuid>

ConferenceManager::ConferenceManager(QObject* parent)
    : QObject(parent)
{
}

QString ConferenceManager::createConference(const QString& host, const QStringList& participants) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ConferenceInfo info;
    info.conferenceId = id;
    info.host = host;
    info.participants = participants;
    if (!info.participants.contains(host)) {
        info.participants.prepend(host);
    }
    info.active = true;

    m_conferences.insert(id, info);
    emit conferenceCreated(id);
    return id;
}

bool ConferenceManager::joinConference(const QString& conferenceId, const QString& username) {
    if (!m_conferences.contains(conferenceId)) return false;

    auto& conf = m_conferences[conferenceId];
    if (!conf.active) return false;
    if (conf.participants.contains(username)) return true;

    conf.participants.append(username);
    emit participantJoined(conferenceId, username);
    return true;
}

void ConferenceManager::leaveConference(const QString& conferenceId, const QString& username) {
    if (!m_conferences.contains(conferenceId)) return;

    auto& conf = m_conferences[conferenceId];
    conf.participants.removeAll(username);
    emit participantLeft(conferenceId, username);

    if (conf.participants.isEmpty()) {
        conf.active = false;
        emit conferenceEnded(conferenceId);
        m_conferences.remove(conferenceId);
    }
}

void ConferenceManager::endConference(const QString& conferenceId) {
    if (!m_conferences.contains(conferenceId)) return;

    m_conferences[conferenceId].active = false;
    emit conferenceEnded(conferenceId);
    m_conferences.remove(conferenceId);
}

ConferenceInfo* ConferenceManager::getConference(const QString& conferenceId) {
    if (m_conferences.contains(conferenceId))
        return &m_conferences[conferenceId];
    return nullptr;
}

QStringList ConferenceManager::getParticipants(const QString& conferenceId) const {
    if (m_conferences.contains(conferenceId))
        return m_conferences[conferenceId].participants;
    return {};
}

bool ConferenceManager::isInConference(const QString& conferenceId, const QString& username) const {
    if (m_conferences.contains(conferenceId))
        return m_conferences[conferenceId].participants.contains(username);
    return false;
}
