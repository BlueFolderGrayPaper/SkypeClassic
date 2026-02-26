#pragma once

#include <QString>

namespace CryptoUtils {
    QString generateSalt();
    QString hashPassword(const QString& password, const QString& salt);
    QString hashAndStore(const QString& password);
    bool verifyPassword(const QString& password, const QString& storedHash);
}
