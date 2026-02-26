#include "windows/LoginWindow.h"
#include "utils/SoundPlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QFont>
#include <QSvgWidget>
#include <QMessageBox>
#include <QDesktopServices>
#include <QRegularExpression>

LoginWindow::LoginWindow(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setWindowTitle("Sign In - Skype");
    resize(280, 400);
    setMinimumSize(250, 350);
}

void LoginWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // Skype logo
    auto* logoWidget = new QSvgWidget(":/icons/skype_logo.svg", this);
    logoWidget->setFixedSize(140, 54);
    mainLayout->addWidget(logoWidget, 0, Qt::AlignCenter);

    mainLayout->addSpacing(4);

    // Separator — etched line like Win2k
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    mainLayout->addSpacing(6);

    // Username
    auto* userLabel = new QLabel("Skype &Name:", this);
    mainLayout->addWidget(userLabel);

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setFixedHeight(21);
    userLabel->setBuddy(m_usernameEdit);
    mainLayout->addWidget(m_usernameEdit);

    mainLayout->addSpacing(4);

    // Password
    auto* passLabel = new QLabel("&Password:", this);
    mainLayout->addWidget(passLabel);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setFixedHeight(21);
    passLabel->setBuddy(m_passwordEdit);
    mainLayout->addWidget(m_passwordEdit);

    mainLayout->addSpacing(4);

    // Checkboxes
    m_rememberCheck = new QCheckBox("&Remember my password", this);
    m_rememberCheck->setChecked(true);
    mainLayout->addWidget(m_rememberCheck);

    auto* autoSignIn = new QCheckBox("S&ign me in when Skype starts", this);
    mainLayout->addWidget(autoSignIn);

    mainLayout->addSpacing(10);

    // Sign In button — plain, no green styling, just a normal button
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_signInButton = new QPushButton("&Sign In", this);
    m_signInButton->setMinimumSize(75, 23);
    m_signInButton->setDefault(true);
    buttonLayout->addWidget(m_signInButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    mainLayout->addSpacing(6);

    // Separator
    auto* line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line2);

    mainLayout->addSpacing(4);

    // Links — real clickable labels
    auto* createAccount = new QLabel("<a href='create' style='color: #0000FF;'>Don't have a Skype Name?</a>", this);
    createAccount->setTextInteractionFlags(Qt::TextBrowserInteraction);
    createAccount->setCursor(Qt::PointingHandCursor);
    connect(createAccount, &QLabel::linkActivated, [this](const QString&) {
        QMessageBox::information(this, "Create Account",
            "In the full version of Skype, this would\n"
            "open the account creation wizard.\n\n"
            "For now, just type any name to sign in.");
    });
    mainLayout->addWidget(createAccount, 0, Qt::AlignCenter);

    auto* forgotPass = new QLabel("<a href='forgot' style='color: #0000FF;'>Forgot your password?</a>", this);
    forgotPass->setTextInteractionFlags(Qt::TextBrowserInteraction);
    forgotPass->setCursor(Qt::PointingHandCursor);
    connect(forgotPass, &QLabel::linkActivated, [this](const QString&) {
        QMessageBox::information(this, "Password Recovery",
            "In the full version of Skype, this would\n"
            "open the password recovery page.\n\n"
            "For now, any password will work.");
    });
    mainLayout->addWidget(forgotPass, 0, Qt::AlignCenter);

    mainLayout->addStretch();

    // Connections
    connect(m_signInButton, &QPushButton::clicked, this, &LoginWindow::onSignInClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::onSignInClicked);
    connect(m_usernameEdit, &QLineEdit::returnPressed, [this]() {
        m_passwordEdit->setFocus();
    });

    m_usernameEdit->setFocus();
}

void LoginWindow::onSignInClicked() {
    QString username = m_usernameEdit->text().trimmed();
    if (username.isEmpty()) {
        m_usernameEdit->setFocus();
        return;
    }

    static QRegularExpression validUsername("^[a-zA-Z0-9_.]{3,32}$");
    if (!validUsername.match(username).hasMatch()) {
        QMessageBox::warning(this, "Invalid Username",
            "Username must be 3-32 characters.\n"
            "Only letters, numbers, underscores, and dots allowed.");
        m_usernameEdit->setFocus();
        return;
    }

    emit loginRequested(username);
}
