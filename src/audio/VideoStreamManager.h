#pragma once

#include <QObject>
#include <QCamera>
#include <QAbstractVideoSurface>
#include <QVideoFrame>
#include <QTimer>
#include <QImage>
#include <QByteArray>

class VideoFrameGrabber : public QAbstractVideoSurface {
    Q_OBJECT

public:
    explicit VideoFrameGrabber(QObject* parent = nullptr);

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override;

    bool present(const QVideoFrame& frame) override;

signals:
    void frameAvailable(const QImage& image);
};

class VideoStreamManager : public QObject {
    Q_OBJECT

public:
    explicit VideoStreamManager(QObject* parent = nullptr);
    ~VideoStreamManager();

    void startCapture();
    void stopCapture();
    bool isCapturing() const { return m_capturing; }

signals:
    void frameCaptured(const QByteArray& jpegData);
    void localFrameReady(const QImage& image);

private slots:
    void onFrameAvailable(const QImage& image);
    void onCaptureTimer();

private:
    QCamera* m_camera = nullptr;
    VideoFrameGrabber* m_grabber = nullptr;
    QTimer* m_captureTimer = nullptr;
    QImage m_lastFrame;
    bool m_capturing = false;
};
