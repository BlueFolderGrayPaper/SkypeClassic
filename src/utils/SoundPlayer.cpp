#include "utils/SoundPlayer.h"
#include <QUrl>
#include <QDebug>

SoundPlayer& SoundPlayer::instance() {
    static SoundPlayer s;
    return s;
}

SoundPlayer::SoundPlayer(QObject* parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
{
}

void SoundPlayer::play(const QString& soundName) {
    QString path = QString("qrc:/sounds/%1").arg(soundName);
    m_player->setMedia(QUrl(path));
    m_player->setVolume(80);
    m_player->play();
    qDebug() << "Playing sound:" << path;
}
