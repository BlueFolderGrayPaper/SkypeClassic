#include <QCoreApplication>
#include <QCommandLineParser>
#include "server/ChatServer.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("SkypeClassic Server");
    app.setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption portOption("port", "Server port (default: 33033)", "port", "33033");
    parser.addOption(portOption);
    parser.process(app);

    quint16 port = parser.value(portOption).toUShort();

    ChatServer server(port);
    if (!server.start()) {
        qCritical() << "Failed to start server";
        return 1;
    }

    qDebug() << "SkypeClassic Server running on port" << port;
    qDebug() << "Default accounts: alice/alice, bob/bob, charlie/charlie";
    qDebug() << "Or sign in with any name to auto-register.";

    return app.exec();
}
