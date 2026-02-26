#pragma once

#include <QObject>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QByteArray>
#include <QTimer>

class OpusCodec;
class JitterBuffer;

class AudioStreamManager : public QObject {
    Q_OBJECT

public:
    explicit AudioStreamManager(QObject* parent = nullptr);
    ~AudioStreamManager();

    void startCapture();
    void stopCapture();
    void startPlayback();
    void stopPlayback();
    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }
    void playAudioData(const QByteArray& data);

signals:
    void audioCaptured(const QByteArray& data);

private slots:
    void onCaptureReady();
    void onJitterFrameReady(const QByteArray& pcmData);

private:
    QAudioFormat m_format;
    QAudioInput* m_audioInput = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    QIODevice* m_inputDevice = nullptr;
    QIODevice* m_outputDevice = nullptr;
    bool m_muted = false;
    bool m_capturing = false;
    bool m_playing = false;

    OpusCodec* m_codec = nullptr;
    JitterBuffer* m_jitterBuffer = nullptr;
};
