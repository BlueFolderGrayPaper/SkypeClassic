#include "audio/VideoStreamManager.h"

#include <QCameraInfo>
#include <QBuffer>
#include <QDebug>

// === VideoFrameGrabber ===

VideoFrameGrabber::VideoFrameGrabber(QObject* parent)
    : QAbstractVideoSurface(parent)
{
}

QList<QVideoFrame::PixelFormat> VideoFrameGrabber::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType type) const
{
    Q_UNUSED(type);
    return {
        QVideoFrame::Format_RGB32,
        QVideoFrame::Format_ARGB32,
        QVideoFrame::Format_ARGB32_Premultiplied,
        QVideoFrame::Format_RGB24,
        QVideoFrame::Format_BGR24,
        QVideoFrame::Format_YUV420P,
        QVideoFrame::Format_YUYV,
        QVideoFrame::Format_NV12,
        QVideoFrame::Format_NV21
    };
}

bool VideoFrameGrabber::present(const QVideoFrame& frame) {
    if (!frame.isValid()) return false;

    QVideoFrame f(frame);
    if (!f.map(QAbstractVideoBuffer::ReadOnly)) return false;

    QImage::Format fmt = QVideoFrame::imageFormatFromPixelFormat(f.pixelFormat());
    QImage image;

    if (fmt != QImage::Format_Invalid) {
        image = QImage(f.bits(), f.width(), f.height(), f.bytesPerLine(), fmt).copy();
    } else {
        // Try converting via RGB32
        image = QImage(f.bits(), f.width(), f.height(), QImage::Format_RGB32).copy();
    }

    f.unmap();

    if (!image.isNull()) {
        emit frameAvailable(image);
    }

    return true;
}

// === VideoStreamManager ===

VideoStreamManager::VideoStreamManager(QObject* parent)
    : QObject(parent)
    , m_captureTimer(new QTimer(this))
{
    m_captureTimer->setInterval(100); // 10 fps
    connect(m_captureTimer, &QTimer::timeout, this, &VideoStreamManager::onCaptureTimer);
}

VideoStreamManager::~VideoStreamManager() {
    stopCapture();
}

void VideoStreamManager::startCapture() {
    if (m_capturing) return;

    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    if (cameras.isEmpty()) {
        qWarning() << "No camera available";
        return;
    }

    m_camera = new QCamera(cameras.first(), this);
    m_grabber = new VideoFrameGrabber(this);

    connect(m_grabber, &VideoFrameGrabber::frameAvailable,
            this, &VideoStreamManager::onFrameAvailable);

    m_camera->setViewfinder(m_grabber);
    m_camera->start();
    m_captureTimer->start();
    m_capturing = true;

    qDebug() << "Video capture started:" << cameras.first().description();
}

void VideoStreamManager::stopCapture() {
    if (!m_capturing) return;
    m_capturing = false;
    m_captureTimer->stop();

    if (m_camera) {
        m_camera->stop();
        delete m_camera;
        m_camera = nullptr;
    }

    if (m_grabber) {
        delete m_grabber;
        m_grabber = nullptr;
    }
}

void VideoStreamManager::onFrameAvailable(const QImage& image) {
    m_lastFrame = image;
}

void VideoStreamManager::onCaptureTimer() {
    if (m_lastFrame.isNull()) return;

    // Scale to 320x240 and emit locally for preview
    QImage scaled = m_lastFrame.scaled(320, 240, Qt::KeepAspectRatio, Qt::FastTransformation);
    emit localFrameReady(scaled);

    // Compress to JPEG and emit for network sending
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "JPEG", 50);
    buffer.close();

    emit frameCaptured(jpegData);
}
