#include <QApplication>
#include <QFile>
#include <QIcon>

#include "app/SkypeApp.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Skype");
    app.setApplicationVersion("1.0.0.106");
    app.setOrganizationName("Skype Technologies S.A.");
    app.setWindowIcon(QIcon(":/icons/skype_icon.svg"));

    // Load stylesheet
    QFile styleFile(":/styles/skype1.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    SkypeApp skypeApp;
    skypeApp.start();

    return app.exec();
}
