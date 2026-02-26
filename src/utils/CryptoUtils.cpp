#include "utils/CryptoUtils.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace CryptoUtils {

QString generateSalt() {
    QByteArray salt;
    salt.resize(16);
    for (int i = 0; i < 16; ++i) {
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return salt.toHex();
}

QString hashPassword(const QString& password, const QString& salt) {
    QByteArray data = (salt + password).toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

QString hashAndStore(const QString& password) {
    QString salt = generateSalt();
    QString hash = hashPassword(password, salt);
    return salt + ":" + hash;
}

bool verifyPassword(const QString& password, const QString& storedHash) {
    int sep = storedHash.indexOf(':');
    if (sep < 0) return false;
    QString salt = storedHash.left(sep);
    QString hash = storedHash.mid(sep + 1);
    return hashPassword(password, salt) == hash;
}

}
