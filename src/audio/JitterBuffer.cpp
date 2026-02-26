#include "audio/JitterBuffer.h"
#include "audio/OpusCodec.h"
#include <QDebug>

JitterBuffer::JitterBuffer(OpusCodec* codec, int targetDepthMs, QObject* parent)
    : QObject(parent)
    , m_codec(codec)
    , m_playoutTimer(new QTimer(this))
{
    int frameSizeSamples = codec ? codec->frameSizeSamples() : 320;
    int sampleRate = 16000;
    m_frameIntervalMs = frameSizeSamples * 1000 / sampleRate;
    m_frameSizeBytes = codec ? codec->frameSizeBytes() : 640;

    m_targetDepthFrames = targetDepthMs / m_frameIntervalMs;
    if (m_targetDepthFrames < 2) m_targetDepthFrames = 2;

    m_playoutTimer->setTimerType(Qt::PreciseTimer);
    m_playoutTimer->setInterval(m_frameIntervalMs);
    connect(m_playoutTimer, &QTimer::timeout, this, &JitterBuffer::onPlayoutTimer);
}

JitterBuffer::~JitterBuffer() {
    stop();
}

void JitterBuffer::pushFrame(const QByteArray& pcmFrame) {
    m_buffer.enqueue(pcmFrame);

    if (m_prebuffering && m_running && m_buffer.size() >= m_targetDepthFrames) {
        m_prebuffering = false;
        m_playoutTimer->start();
    }
}

void JitterBuffer::start() {
    m_running = true;
    m_prebuffering = true;
    // Timer starts once prebuffer fills in pushFrame()
}

void JitterBuffer::stop() {
    m_running = false;
    m_prebuffering = true;
    m_playoutTimer->stop();
    m_buffer.clear();
}

void JitterBuffer::reset() {
    stop();
    m_underruns = 0;
}

int JitterBuffer::currentDepth() const {
    return m_buffer.size();
}

int JitterBuffer::underrunCount() const {
    return m_underruns;
}

void JitterBuffer::onPlayoutTimer() {
    if (m_buffer.isEmpty()) {
        m_underruns++;
        if (m_codec) {
            QByteArray plc = m_codec->decodePLC();
            if (!plc.isEmpty()) {
                emit frameReady(plc);
                return;
            }
        }
        // Fallback: emit silence
        emit frameReady(QByteArray(m_frameSizeBytes, '\0'));
        return;
    }

    emit frameReady(m_buffer.dequeue());
}
