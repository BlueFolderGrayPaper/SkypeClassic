#pragma once

#include <QString>
#include <QStringList>

struct GroupChat {
    QString groupId;
    QString groupName;
    QString creator;
    QStringList members;
};
