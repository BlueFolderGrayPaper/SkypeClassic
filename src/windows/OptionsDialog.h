#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QRadioButton>
#include <QSettings>

class OptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget* parent = nullptr);

private slots:
    void onApply();
    void onOk();
    void onSettingChanged();

private:
    void setupUi();
    QWidget* createGeneralPage();
    QWidget* createPrivacyPage();
    QWidget* createNotificationsPage();
    QWidget* createSoundDevicesPage();
    QWidget* createAdvancedPage();

    void loadSettings();
    void saveSettings();

    QListWidget* m_categoryList;
    QStackedWidget* m_pages;
    QPushButton* m_applyBtn = nullptr;

    // General
    QCheckBox* m_startOnBoot = nullptr;
    QCheckBox* m_startMinimized = nullptr;
    QCheckBox* m_showEmoticons = nullptr;
    QCheckBox* m_showTimestamps = nullptr;

    // Privacy
    QRadioButton* m_callsFromAnyone = nullptr;
    QRadioButton* m_callsFromContacts = nullptr;
    QRadioButton* m_chatsFromAnyone = nullptr;
    QRadioButton* m_chatsFromContacts = nullptr;
    QCheckBox* m_showWebStatus = nullptr;

    // Notifications
    QCheckBox* m_sndIncomingCall = nullptr;
    QCheckBox* m_sndMessage = nullptr;
    QCheckBox* m_sndOnline = nullptr;
    QCheckBox* m_sndTransfer = nullptr;
    QCheckBox* m_sndContactRequest = nullptr;

    // Sound Devices
    QComboBox* m_audioIn = nullptr;
    QComboBox* m_audioOut = nullptr;
    QComboBox* m_ringDevice = nullptr;
    QCheckBox* m_autoAdjustMic = nullptr;

    // Advanced
    QSpinBox* m_portSpin = nullptr;
    QCheckBox* m_useAltPorts = nullptr;
};
