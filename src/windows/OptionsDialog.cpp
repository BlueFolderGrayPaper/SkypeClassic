#include "windows/OptionsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGroupBox>
#include <QPushButton>
#include <QLineEdit>

OptionsDialog::OptionsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
    setWindowTitle("Skype - Options");
    resize(500, 380);
}

void OptionsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto* topLayout = new QHBoxLayout();

    m_categoryList = new QListWidget(this);
    m_categoryList->setFixedWidth(130);
    m_categoryList->addItem("General");
    m_categoryList->addItem("Privacy");
    m_categoryList->addItem("Notifications");
    m_categoryList->addItem("Sound Devices");
    m_categoryList->addItem("Advanced");
    m_categoryList->setCurrentRow(0);
    topLayout->addWidget(m_categoryList);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createGeneralPage());
    m_pages->addWidget(createPrivacyPage());
    m_pages->addWidget(createNotificationsPage());
    m_pages->addWidget(createSoundDevicesPage());
    m_pages->addWidget(createAdvancedPage());
    topLayout->addWidget(m_pages);

    mainLayout->addLayout(topLayout);

    connect(m_categoryList, &QListWidget::currentRowChanged,
            m_pages, &QStackedWidget::setCurrentIndex);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto* okBtn = new QPushButton("OK", this);
    okBtn->setMinimumSize(75, 23);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &OptionsDialog::onOk);
    buttonLayout->addWidget(okBtn);

    auto* cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setMinimumSize(75, 23);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    m_applyBtn = new QPushButton("Apply", this);
    m_applyBtn->setMinimumSize(75, 23);
    m_applyBtn->setEnabled(false);
    connect(m_applyBtn, &QPushButton::clicked, this, &OptionsDialog::onApply);
    buttonLayout->addWidget(m_applyBtn);

    mainLayout->addLayout(buttonLayout);
}

QWidget* OptionsDialog::createGeneralPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("<b>General Settings</b>", page));
    layout->addSpacing(8);

    auto* startupGroup = new QGroupBox("Startup", page);
    auto* sgLayout = new QVBoxLayout(startupGroup);
    m_startOnBoot = new QCheckBox("Start Skype when I start Windows", startupGroup);
    m_startMinimized = new QCheckBox("Launch Skype minimized", startupGroup);
    sgLayout->addWidget(m_startOnBoot);
    sgLayout->addWidget(m_startMinimized);
    layout->addWidget(startupGroup);

    auto* chatGroup = new QGroupBox("Chat", page);
    auto* cgLayout = new QVBoxLayout(chatGroup);
    m_showEmoticons = new QCheckBox("Show emoticons", chatGroup);
    m_showTimestamps = new QCheckBox("Show timestamps in chat", chatGroup);
    cgLayout->addWidget(m_showEmoticons);
    cgLayout->addWidget(m_showTimestamps);
    layout->addWidget(chatGroup);

    connect(m_startOnBoot, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_startMinimized, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_showEmoticons, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_showTimestamps, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);

    layout->addStretch();
    return page;
}

QWidget* OptionsDialog::createPrivacyPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("<b>Privacy Settings</b>", page));
    layout->addSpacing(8);

    auto* callGroup = new QGroupBox("Allow calls from...", page);
    auto* clLayout = new QVBoxLayout(callGroup);
    m_callsFromAnyone = new QRadioButton("anyone", callGroup);
    m_callsFromContacts = new QRadioButton("only people in my Contact list", callGroup);
    clLayout->addWidget(m_callsFromAnyone);
    clLayout->addWidget(m_callsFromContacts);
    layout->addWidget(callGroup);

    auto* chatGroup = new QGroupBox("Allow chats from...", page);
    auto* chLayout = new QVBoxLayout(chatGroup);
    m_chatsFromAnyone = new QRadioButton("anyone", chatGroup);
    m_chatsFromContacts = new QRadioButton("only people in my Contact list", chatGroup);
    chLayout->addWidget(m_chatsFromAnyone);
    chLayout->addWidget(m_chatsFromContacts);
    layout->addWidget(chatGroup);

    m_showWebStatus = new QCheckBox("Allow my online status to be shown on the web", page);
    layout->addWidget(m_showWebStatus);

    connect(m_callsFromAnyone, &QRadioButton::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_chatsFromAnyone, &QRadioButton::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_showWebStatus, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);

    layout->addStretch();
    return page;
}

QWidget* OptionsDialog::createNotificationsPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("<b>Notifications</b>", page));
    layout->addSpacing(8);

    auto* eventsGroup = new QGroupBox("Play sounds for these events:", page);
    auto* egLayout = new QVBoxLayout(eventsGroup);

    m_sndIncomingCall = new QCheckBox("Incoming call", eventsGroup);
    m_sndMessage = new QCheckBox("Incoming chat message", eventsGroup);
    m_sndOnline = new QCheckBox("Contact comes online", eventsGroup);
    m_sndTransfer = new QCheckBox("File transfer complete", eventsGroup);
    m_sndContactRequest = new QCheckBox("Contact request received", eventsGroup);

    egLayout->addWidget(m_sndIncomingCall);
    egLayout->addWidget(m_sndMessage);
    egLayout->addWidget(m_sndOnline);
    egLayout->addWidget(m_sndTransfer);
    egLayout->addWidget(m_sndContactRequest);
    layout->addWidget(eventsGroup);

    connect(m_sndIncomingCall, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_sndMessage, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_sndOnline, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_sndTransfer, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);
    connect(m_sndContactRequest, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);

    layout->addStretch();
    return page;
}

QWidget* OptionsDialog::createSoundDevicesPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("<b>Sound Devices</b>", page));
    layout->addSpacing(8);

    layout->addWidget(new QLabel("Audio In (Microphone):", page));
    m_audioIn = new QComboBox(page);
    m_audioIn->addItem("System default device");
    m_audioIn->setFixedHeight(21);
    layout->addWidget(m_audioIn);

    layout->addSpacing(6);

    layout->addWidget(new QLabel("Audio Out (Speakers):", page));
    m_audioOut = new QComboBox(page);
    m_audioOut->addItem("System default device");
    m_audioOut->setFixedHeight(21);
    layout->addWidget(m_audioOut);

    layout->addSpacing(6);

    layout->addWidget(new QLabel("Ringing:", page));
    m_ringDevice = new QComboBox(page);
    m_ringDevice->addItem("System default device");
    m_ringDevice->setFixedHeight(21);
    layout->addWidget(m_ringDevice);

    layout->addSpacing(10);

    m_autoAdjustMic = new QCheckBox("Automatically adjust microphone settings", page);
    layout->addWidget(m_autoAdjustMic);

    connect(m_autoAdjustMic, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);

    layout->addStretch();
    return page;
}

QWidget* OptionsDialog::createAdvancedPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("<b>Advanced Settings</b>", page));
    layout->addSpacing(8);

    auto* connGroup = new QGroupBox("Connection", page);
    auto* cnLayout = new QVBoxLayout(connGroup);

    auto* portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("Use port", connGroup));
    m_portSpin = new QSpinBox(connGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setFixedWidth(70);
    portLayout->addWidget(m_portSpin);
    portLayout->addWidget(new QLabel("for incoming connections", connGroup));
    portLayout->addStretch();
    cnLayout->addLayout(portLayout);

    m_useAltPorts = new QCheckBox("Use port 80 and 443 as alternatives", connGroup);
    cnLayout->addWidget(m_useAltPorts);

    auto* proxyLayout = new QHBoxLayout();
    proxyLayout->addWidget(new QLabel("HTTPS proxy host:", connGroup));
    auto* proxyEdit = new QLineEdit(connGroup);
    proxyEdit->setFixedHeight(21);
    proxyLayout->addWidget(proxyEdit);
    proxyLayout->addWidget(new QLabel("Port:", connGroup));
    auto* proxyPort = new QSpinBox(connGroup);
    proxyPort->setRange(1, 65535);
    proxyPort->setValue(8080);
    proxyPort->setFixedWidth(70);
    proxyLayout->addWidget(proxyPort);
    cnLayout->addLayout(proxyLayout);

    layout->addWidget(connGroup);

    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &OptionsDialog::onSettingChanged);
    connect(m_useAltPorts, &QCheckBox::toggled, this, &OptionsDialog::onSettingChanged);

    layout->addStretch();
    return page;
}

void OptionsDialog::loadSettings() {
    QSettings s("SkypeClassic", "SkypeClassic");

    m_startOnBoot->setChecked(s.value("general/startOnBoot", false).toBool());
    m_startMinimized->setChecked(s.value("general/startMinimized", false).toBool());
    m_showEmoticons->setChecked(s.value("general/showEmoticons", true).toBool());
    m_showTimestamps->setChecked(s.value("general/showTimestamps", true).toBool());

    m_callsFromAnyone->setChecked(s.value("privacy/callsFromAnyone", true).toBool());
    m_callsFromContacts->setChecked(!s.value("privacy/callsFromAnyone", true).toBool());
    m_chatsFromAnyone->setChecked(s.value("privacy/chatsFromAnyone", true).toBool());
    m_chatsFromContacts->setChecked(!s.value("privacy/chatsFromAnyone", true).toBool());
    m_showWebStatus->setChecked(s.value("privacy/showWebStatus", false).toBool());

    m_sndIncomingCall->setChecked(s.value("notifications/incomingCall", true).toBool());
    m_sndMessage->setChecked(s.value("notifications/message", true).toBool());
    m_sndOnline->setChecked(s.value("notifications/online", true).toBool());
    m_sndTransfer->setChecked(s.value("notifications/transfer", true).toBool());
    m_sndContactRequest->setChecked(s.value("notifications/contactRequest", false).toBool());

    m_autoAdjustMic->setChecked(s.value("sound/autoAdjustMic", true).toBool());

    m_portSpin->setValue(s.value("advanced/port", 33033).toInt());
    m_useAltPorts->setChecked(s.value("advanced/useAltPorts", false).toBool());

    m_applyBtn->setEnabled(false);
}

void OptionsDialog::saveSettings() {
    QSettings s("SkypeClassic", "SkypeClassic");

    s.setValue("general/startOnBoot", m_startOnBoot->isChecked());
    s.setValue("general/startMinimized", m_startMinimized->isChecked());
    s.setValue("general/showEmoticons", m_showEmoticons->isChecked());
    s.setValue("general/showTimestamps", m_showTimestamps->isChecked());

    s.setValue("privacy/callsFromAnyone", m_callsFromAnyone->isChecked());
    s.setValue("privacy/chatsFromAnyone", m_chatsFromAnyone->isChecked());
    s.setValue("privacy/showWebStatus", m_showWebStatus->isChecked());

    s.setValue("notifications/incomingCall", m_sndIncomingCall->isChecked());
    s.setValue("notifications/message", m_sndMessage->isChecked());
    s.setValue("notifications/online", m_sndOnline->isChecked());
    s.setValue("notifications/transfer", m_sndTransfer->isChecked());
    s.setValue("notifications/contactRequest", m_sndContactRequest->isChecked());

    s.setValue("sound/autoAdjustMic", m_autoAdjustMic->isChecked());

    s.setValue("advanced/port", m_portSpin->value());
    s.setValue("advanced/useAltPorts", m_useAltPorts->isChecked());

    m_applyBtn->setEnabled(false);
}

void OptionsDialog::onSettingChanged() {
    if (m_applyBtn) m_applyBtn->setEnabled(true);
}

void OptionsDialog::onApply() {
    saveSettings();
}

void OptionsDialog::onOk() {
    saveSettings();
    accept();
}
