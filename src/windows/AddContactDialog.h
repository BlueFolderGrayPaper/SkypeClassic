#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class AddContactDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddContactDialog(QWidget* parent = nullptr);

    QString skypeName() const;

private:
    void setupUi();

    QLineEdit* m_skypeNameEdit;
    QLineEdit* m_messageEdit;
};
