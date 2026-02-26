#pragma once

#include <QByteArray>
#include <QList>
#include <opus/opus.h>

class OpusCodec {
public:
    OpusCodec(int sampleRate = 16000, int channels = 1, int frameSizeMs = 20);
    ~OpusCodec();

    bool isValid() const;

    // Encode one frame of PCM (frameSizeBytes() bytes expected)
    QByteArray encode(const QByteArray& pcmData);

    // Decode one Opus packet back to PCM
    QByteArray decode(const QByteArray& opusData);

    // Packet loss concealment — synthesize a frame when data is missing
    QByteArray decodePLC();

    // Encode an arbitrary-length PCM buffer into multiple Opus frames.
    // Handles buffering partial frames internally.
    QList<QByteArray> encodeBuffer(const QByteArray& pcmData);

    int frameSizeSamples() const { return m_frameSizeSamples; }
    int frameSizeBytes() const { return m_frameSizeSamples * m_channels * 2; }

    void reset();

private:
    OpusEncoder* m_encoder = nullptr;
    OpusDecoder* m_decoder = nullptr;

    int m_sampleRate;
    int m_channels;
    int m_frameSizeSamples;

    QByteArray m_encodeBuffer;
    bool m_valid = false;
};
