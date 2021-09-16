#include "AOADeviceManager.h"
#include "InformationWidget.h"
#include <QApplication>
#include "DriverHelper.h"

int main(int argc, char *argv[]){
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("yuerlive");

    QThread thread;
    QObject::connect(&app, &QApplication::aboutToQuit, &thread, &QThread::quit);
    thread.start();

    AOADeviceManager manager;
    manager.moveToThread(&thread);

    InformationWidget widget;
    QObject::connect(&manager, &AOADeviceManager::installProgress, &widget, &InformationWidget::onInstallStatus);
    QObject::connect(&manager, &AOADeviceManager::infoPrompt, &widget, &InformationWidget::onInfoPrompt);
    QObject::connect(&manager, &AOADeviceManager::deviceLost, &widget, &InformationWidget::onDeviceLost);

    QMetaObject::invokeMethod(&manager, "updateUsbInventory", Qt::QueuedConnection, Q_ARG(bool, true));


    HelerWidget w(&manager);
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    w.setFixedSize(1, 1);
    w.show();
    w.hide();


    return app.exec();
}
