#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include "models/Contact.h"

class StatusSelector : public QWidget {
    Q_OBJECT

public:
    explicit StatusSelector(QWidget* parent = nullptr);

    ContactStatus currentStatus() const;

signals:
    void statusChanged(ContactStatus status);

private:
    void setupUi();
    void populateStatuses();
    QIcon statusIcon(ContactStatus status) const;

    QComboBox* m_combo;
};
