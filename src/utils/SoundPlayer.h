#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QString>
#include <QMap>

class SoundPlayer : public QObject {
    Q_OBJECT

public:
    static SoundPlayer& instance();

    void play(const QString& soundName);

private:
    explicit SoundPlayer(QObject* parent = nullptr);
    QMediaPlayer* m_player;
};
