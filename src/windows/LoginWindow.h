#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>

class LoginWindow : public QWidget {
    Q_OBJECT

public:
    explicit LoginWindow(QWidget* parent = nullptr);

signals:
    void loginRequested(const QString& username);

private slots:
    void onSignInClicked();

private:
    void setupUi();

    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QCheckBox* m_rememberCheck;
    QPushButton* m_signInButton;
};
