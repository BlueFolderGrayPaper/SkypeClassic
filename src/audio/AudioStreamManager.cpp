#include "audio/AudioStreamManager.h"
#include "audio/OpusCodec.h"
#include "audio/JitterBuffer.h"

#include <QAudioDeviceInfo>
#include <QDebug>

AudioStreamManager::AudioStreamManager(QObject* parent)
    : QObject(parent)
    , m_codec(new OpusCodec(16000, 1, 20))
    , m_jitterBuffer(new JitterBuffer(m_codec, 60, this))
{
    m_format.setSampleRate(16000);
    m_format.setChannelCount(1);
    m_format.setSampleSize(16);
    m_format.setCodec("audio/pcm");
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setSampleType(QAudioFormat::SignedInt);

    if (!m_codec->isValid()) {
        qWarning() << "Opus codec initialization failed -- audio calls will not work";
    }

    connect(m_jitterBuffer, &JitterBuffer::frameReady,
            this, &AudioStreamManager::onJitterFrameReady);
}

AudioStreamManager::~AudioStreamManager() {
    stopCapture();
    stopPlayback();
    delete m_codec;
}

void AudioStreamManager::startCapture() {
    if (m_capturing) return;

    QAudioDeviceInfo inputDevice = QAudioDeviceInfo::defaultInputDevice();
    if (inputDevice.isNull()) {
        qWarning() << "No audio input device available";
        return;
    }

    QAudioFormat format = m_format;
    if (!inputDevice.isFormatSupported(format)) {
        format = inputDevice.nearestFormat(format);
        qDebug() << "Using nearest audio format:" << format.sampleRate() << "Hz";
    }

    m_audioInput = new QAudioInput(inputDevice, format, this);
    m_audioInput->setBufferSize(1600); // ~50ms at 16kHz 16-bit mono
    m_inputDevice = m_audioInput->start();

    if (m_inputDevice) {
        connect(m_inputDevice, &QIODevice::readyRead, this, &AudioStreamManager::onCaptureReady);
        m_capturing = true;
        m_codec->reset();
        qDebug() << "Audio capture started";
    }
}

void AudioStreamManager::stopCapture() {
    if (!m_capturing) return;
    m_capturing = false;

    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }
    m_inputDevice = nullptr;
}

void AudioStreamManager::startPlayback() {
    if (m_playing) return;

    QAudioDeviceInfo outputDevice = QAudioDeviceInfo::defaultOutputDevice();
    if (outputDevice.isNull()) {
        qWarning() << "No audio output device available";
        return;
    }

    QAudioFormat format = m_format;
    if (!outputDevice.isFormatSupported(format)) {
        format = outputDevice.nearestFormat(format);
    }

    m_audioOutput = new QAudioOutput(outputDevice, format, this);
    m_audioOutput->setBufferSize(8000); // ~250ms buffer
    m_outputDevice = m_audioOutput->start();
    m_playing = true;

    m_jitterBuffer->start();

    qDebug() << "Audio playback started";
}

void AudioStreamManager::stopPlayback() {
    if (!m_playing) return;
    m_playing = false;

    m_jitterBuffer->stop();

    if (m_audioOutput) {
        m_audioOutput->stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
    m_outputDevice = nullptr;
}

void AudioStreamManager::setMuted(bool muted) {
    m_muted = muted;
}

void AudioStreamManager::playAudioData(const QByteArray& data) {
    if (!m_codec->isValid()) return;

    QByteArray pcm = m_codec->decode(data);
    if (!pcm.isEmpty()) {
        m_jitterBuffer->pushFrame(pcm);
    }
}

void AudioStreamManager::onCaptureReady() {
    if (!m_inputDevice || !m_capturing) return;

    QByteArray data = m_inputDevice->readAll();
    if (data.isEmpty() || m_muted) return;

    QList<QByteArray> opusFrames = m_codec->encodeBuffer(data);
    for (const QByteArray& frame : opusFrames) {
        emit audioCaptured(frame);
    }
}

void AudioStreamManager::onJitterFrameReady(const QByteArray& pcmData) {
    if (m_outputDevice && m_playing) {
        m_outputDevice->write(pcmData);
    }
}
