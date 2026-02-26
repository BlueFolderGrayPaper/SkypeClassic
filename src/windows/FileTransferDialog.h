#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

class FileTransferDialog : public QDialog {
    Q_OBJECT

public:
    enum Direction { Sending, Receiving };

    explicit FileTransferDialog(const QString& contactName,
                                 const QString& fileName,
                                 Direction direction,
                                 QWidget* parent = nullptr,
                                 const QString& filePath = QString(),
                                 qint64 fileSize = 0);

private slots:
    void onAcceptClicked();
    void onDeclineClicked();
    void onCancelClicked();
    void updateProgress();

private:
    void setupUi();
    QString formatSize(qint64 bytes) const;

    QString m_contactName;
    QString m_fileName;
    QString m_filePath;
    qint64 m_fileSize;
    Direction m_direction;

    QLabel* m_statusLabel;
    QLabel* m_fileLabel;
    QLabel* m_sizeLabel;
    QProgressBar* m_progressBar;
    QPushButton* m_acceptBtn;
    QPushButton* m_declineBtn;
    QPushButton* m_cancelBtn;
    QPushButton* m_closeBtn;

    QTimer* m_progressTimer;
    int m_progress = 0;
};
