﻿#include "AOADeviceManager.h"
#include "InformationWidget.h"
#include <QApplication>
#include "DriverHelper.h"

int main(int argc, char *argv[]){
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);


    QThread thread;
    QObject::connect(&app, &QApplication::aboutToQuit, &thread, &QThread::quit);
    thread.start();

    AOADeviceManager manager;
    manager.moveToThread(&thread);

    InformationWidget widget;
    QObject::connect(&manager, &AOADeviceManager::installProgress, &widget, &InformationWidget::onInstallStatus);

    QMetaObject::invokeMethod(&manager, "updateUsbInventory", Qt::QueuedConnection);

    NativeEventFilter filter(&manager);
    app.installNativeEventFilter(&filter);

    QWidget w;
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    w.setFixedSize(1, 1);
    w.show();
    w.hide();


    return app.exec();
}