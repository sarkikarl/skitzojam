// main.cpp
#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("EightDAudio");
    QCoreApplication::setApplicationName("EightDAudioEffect");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    // Closing the last visible window shouldn't quit the app — we hide
    // to the tray instead and only actually exit via the tray menu.
    app.setQuitOnLastWindowClosed(false);

    bool startMinimized = app.arguments().contains("--minimized");

    MainWindow window(startMinimized);
    return app.exec();
}
