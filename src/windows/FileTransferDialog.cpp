#include "windows/FileTransferDialog.h"
#include "utils/SoundPlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFileInfo>
#include <QFileDialog>

FileTransferDialog::FileTransferDialog(const QString& contactName,
                                         const QString& fileName,
                                         Direction direction,
                                         QWidget* parent,
                                         const QString& filePath,
                                         qint64 fileSize)
    : QDialog(parent)
    , m_contactName(contactName)
    , m_fileName(fileName)
    , m_filePath(filePath)
    , m_fileSize(fileSize)
    , m_direction(direction)
    , m_progressTimer(new QTimer(this))
{
    // If we have a file path, get real file info
    if (!m_filePath.isEmpty() && m_fileSize == 0) {
        QFileInfo fi(m_filePath);
        m_fileSize = fi.size();
        if (m_fileName.isEmpty()) {
            m_fileName = fi.fileName();
        }
    }

    setupUi();
    setWindowTitle("File Transfer");
    resize(320, 200);
    setMinimumSize(280, 170);

    connect(m_progressTimer, &QTimer::timeout, this, &FileTransferDialog::updateProgress);

    if (direction == Receiving) {
        SoundPlayer::instance().play("INCOMING_FILE.WAV");
    }
}

void FileTransferDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    if (m_direction == Receiving) {
        m_statusLabel = new QLabel(
            QString("%1 wants to send you a file:").arg(m_contactName), this);
    } else {
        m_statusLabel = new QLabel(
            QString("Sending file to %1:").arg(m_contactName), this);
    }
    mainLayout->addWidget(m_statusLabel);

    m_fileLabel = new QLabel(m_fileName, this);
    QFont f = m_fileLabel->font();
    f.setBold(true);
    m_fileLabel->setFont(f);
    mainLayout->addWidget(m_fileLabel);

    m_sizeLabel = new QLabel(this);
    if (m_fileSize > 0) {
        m_sizeLabel->setText(QString("Size: %1").arg(formatSize(m_fileSize)));
    }
    mainLayout->addWidget(m_sizeLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(18);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    mainLayout->addStretch();

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_acceptBtn = new QPushButton("&Accept", this);
    m_acceptBtn->setMinimumSize(75, 23);
    m_acceptBtn->setVisible(m_direction == Receiving);
    connect(m_acceptBtn, &QPushButton::clicked, this, &FileTransferDialog::onAcceptClicked);
    buttonLayout->addWidget(m_acceptBtn);

    m_declineBtn = new QPushButton("&Decline", this);
    m_declineBtn->setMinimumSize(75, 23);
    m_declineBtn->setVisible(m_direction == Receiving);
    connect(m_declineBtn, &QPushButton::clicked, this, &FileTransferDialog::onDeclineClicked);
    buttonLayout->addWidget(m_declineBtn);

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setMinimumSize(75, 23);
    m_cancelBtn->setVisible(m_direction == Sending);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FileTransferDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelBtn);

    m_closeBtn = new QPushButton("&Close", this);
    m_closeBtn->setMinimumSize(75, 23);
    m_closeBtn->setVisible(false);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(buttonLayout);

    if (m_direction == Sending) {
        m_progressBar->setVisible(true);
        m_progressTimer->start(100);
    }
}

void FileTransferDialog::onAcceptClicked() {
    // Ask where to save the file
    QString savePath = QFileDialog::getSaveFileName(this, "Save File As",
        m_fileName);
    if (savePath.isEmpty()) return;

    m_acceptBtn->setVisible(false);
    m_declineBtn->setVisible(false);
    m_cancelBtn->setVisible(true);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(QString("Receiving file from %1:").arg(m_contactName));
    m_progressTimer->start(100);
}

void FileTransferDialog::onDeclineClicked() {
    m_statusLabel->setText("File transfer declined.");
    m_acceptBtn->setVisible(false);
    m_declineBtn->setVisible(false);
    m_closeBtn->setVisible(true);
    SoundPlayer::instance().play("FT_FAILED.WAV");
}

void FileTransferDialog::onCancelClicked() {
    m_progressTimer->stop();
    m_statusLabel->setText("File transfer cancelled.");
    m_cancelBtn->setVisible(false);
    m_closeBtn->setVisible(true);
    SoundPlayer::instance().play("FT_FAILED.WAV");
}

void FileTransferDialog::updateProgress() {
    m_progress += 2;
    m_progressBar->setValue(m_progress);

    if (m_fileSize > 0) {
        qint64 transferred = (m_fileSize * m_progress) / 100;
        m_sizeLabel->setText(QString("%1 / %2")
            .arg(formatSize(transferred), formatSize(m_fileSize)));
    }

    if (m_progress >= 100) {
        m_progressTimer->stop();
        m_statusLabel->setText("File transfer complete!");
        m_cancelBtn->setVisible(false);
        m_closeBtn->setVisible(true);
        if (m_fileSize > 0) {
            m_sizeLabel->setText(QString("%1 transferred").arg(formatSize(m_fileSize)));
        }
        SoundPlayer::instance().play("FT_COMPLETE.WAV");
    }
}

QString FileTransferDialog::formatSize(qint64 bytes) const {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024 * 1024));
    return QString("%1 GB").arg(bytes / (1024 * 1024 * 1024));
}
