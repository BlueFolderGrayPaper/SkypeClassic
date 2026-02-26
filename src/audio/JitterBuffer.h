#pragma once

#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <QTimer>

class OpusCodec;

class JitterBuffer : public QObject {
    Q_OBJECT

public:
    explicit JitterBuffer(OpusCodec* codec, int targetDepthMs = 60, QObject* parent = nullptr);
    ~JitterBuffer();

    void pushFrame(const QByteArray& pcmFrame);
    void start();
    void stop();
    void reset();

    int currentDepth() const;
    int underrunCount() const;

signals:
    void frameReady(const QByteArray& pcmData);

private slots:
    void onPlayoutTimer();

private:
    OpusCodec* m_codec;
    QQueue<QByteArray> m_buffer;
    QTimer* m_playoutTimer;

    int m_targetDepthFrames;
    int m_frameIntervalMs;
    int m_frameSizeBytes;
    int m_underruns = 0;
    bool m_running = false;
    bool m_prebuffering = true;
};
