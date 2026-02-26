#include "audio/OpusCodec.h"
#include <QDebug>

OpusCodec::OpusCodec(int sampleRate, int channels, int frameSizeMs)
    : m_sampleRate(sampleRate)
    , m_channels(channels)
    , m_frameSizeSamples(sampleRate * frameSizeMs / 1000)
{
    int err;
    m_encoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !m_encoder) {
        qWarning() << "Failed to create Opus encoder:" << opus_strerror(err);
        return;
    }

    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(m_encoder, OPUS_SET_DTX(1));

    m_decoder = opus_decoder_create(sampleRate, channels, &err);
    if (err != OPUS_OK || !m_decoder) {
        qWarning() << "Failed to create Opus decoder:" << opus_strerror(err);
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
        return;
    }

    m_valid = true;
}

OpusCodec::~OpusCodec() {
    if (m_encoder) opus_encoder_destroy(m_encoder);
    if (m_decoder) opus_decoder_destroy(m_decoder);
}

bool OpusCodec::isValid() const { return m_valid; }

QByteArray OpusCodec::encode(const QByteArray& pcmData) {
    if (!m_valid) return {};
    if (pcmData.size() != frameSizeBytes()) {
        qWarning() << "OpusCodec::encode: expected" << frameSizeBytes()
                   << "bytes, got" << pcmData.size();
        return {};
    }

    QByteArray encoded(256, '\0');
    int len = opus_encode(m_encoder,
        reinterpret_cast<const opus_int16*>(pcmData.constData()),
        m_frameSizeSamples,
        reinterpret_cast<unsigned char*>(encoded.data()),
        encoded.size());

    if (len < 0) {
        qWarning() << "Opus encode error:" << opus_strerror(len);
        return {};
    }

    encoded.resize(len);
    return encoded;
}

QByteArray OpusCodec::decode(const QByteArray& opusData) {
    if (!m_valid) return {};

    QByteArray pcm(frameSizeBytes(), '\0');
    int samples = opus_decode(m_decoder,
        reinterpret_cast<const unsigned char*>(opusData.constData()),
        opusData.size(),
        reinterpret_cast<opus_int16*>(pcm.data()),
        m_frameSizeSamples,
        0);

    if (samples < 0) {
        qWarning() << "Opus decode error:" << opus_strerror(samples);
        return {};
    }

    pcm.resize(samples * m_channels * 2);
    return pcm;
}

QByteArray OpusCodec::decodePLC() {
    if (!m_valid) return {};

    QByteArray pcm(frameSizeBytes(), '\0');
    int samples = opus_decode(m_decoder,
        nullptr, 0,
        reinterpret_cast<opus_int16*>(pcm.data()),
        m_frameSizeSamples,
        0);

    if (samples < 0) return {};
    pcm.resize(samples * m_channels * 2);
    return pcm;
}

QList<QByteArray> OpusCodec::encodeBuffer(const QByteArray& pcmData) {
    QList<QByteArray> frames;
    if (!m_valid) return frames;

    m_encodeBuffer.append(pcmData);

    int frameBytes = frameSizeBytes();
    while (m_encodeBuffer.size() >= frameBytes) {
        QByteArray frame = m_encodeBuffer.left(frameBytes);
        m_encodeBuffer.remove(0, frameBytes);

        QByteArray encoded = encode(frame);
        if (!encoded.isEmpty()) {
            frames.append(encoded);
        }
    }

    return frames;
}

void OpusCodec::reset() {
    m_encodeBuffer.clear();
    if (m_encoder) opus_encoder_ctl(m_encoder, OPUS_RESET_STATE);
    if (m_decoder) opus_decoder_ctl(m_decoder, OPUS_RESET_STATE);
}
