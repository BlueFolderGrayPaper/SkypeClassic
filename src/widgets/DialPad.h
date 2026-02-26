#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>

class DialPad : public QWidget {
    Q_OBJECT

public:
    explicit DialPad(QWidget* parent = nullptr);

signals:
    void callSkypeNumber(const QString& skypeNumber);

private slots:
    void onDigitPressed(const QString& digit);
    void onCallClicked();
    void onClearClicked();

private:
    void setupUi();
    QPushButton* createDigitButton(const QString& digit, const QString& letters = "");

    QLineEdit* m_numberDisplay;
};
