#pragma once

#include <QString>
#include <QDateTime>
#include <QList>

struct Message {
    QString sender;
    QString text;
    QDateTime timestamp;
    bool isOutgoing;
};
