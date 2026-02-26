#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include "models/Contact.h"

class SearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SearchDialog(const QList<Contact>& contacts, QWidget* parent = nullptr);

    QString addedContact() const { return m_addedContact; }

signals:
    void contactAdded(const QString& skypeName);

private slots:
    void onSearchClicked();

private:
    void setupUi();

    QList<Contact> m_contacts;
    QLineEdit* m_searchEdit;
    QListWidget* m_resultsList;
    QPushButton* m_searchBtn;
    QPushButton* m_addBtn;
    QString m_addedContact;
};
