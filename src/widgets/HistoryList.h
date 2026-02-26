#pragma once

#include <QWidget>
#include <QListWidget>

class HistoryList : public QWidget {
    Q_OBJECT

public:
    explicit HistoryList(QWidget* parent = nullptr);

private:
    void setupUi();
    void populateMockHistory();

    QListWidget* m_listWidget;
};
