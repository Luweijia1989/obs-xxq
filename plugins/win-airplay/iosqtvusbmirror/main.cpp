#include "AppleDeviceManager.h"
#include "InformationWidget.h"
#include <QApplication>
#include "DriverHelper.h"

void stdin_read_thread()
{
	uint8_t buf[1024] = {0};
	while (true) {
		int read_len =
			fread(buf, 1, 1024,
			      stdin); // read 0 means parent has been stopped
		if (read_len) {
			if (buf[0] == 1) {
				qApp->quit();
				break;
			}
		} else {
			qApp->quit();
			break;
		}
	}
}

int main(int argc, char *argv[]){
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("yuerlive");

    QThread thread;
    QObject::connect(&app, &QApplication::aboutToQuit, &thread, &QThread::quit);
    thread.start();

    AppleDeviceManager manager;
    manager.moveToThread(&thread);

    InformationWidget widget;
    QObject::connect(&manager, &AppleDeviceManager::installProgress, &widget, &InformationWidget::onInstallStatus);
    QObject::connect(&manager, &AppleDeviceManager::installError, &widget, &InformationWidget::onInstallError);
    QObject::connect(&manager, &AppleDeviceManager::infoPrompt, &widget, &InformationWidget::onInfoPrompt);
    QObject::connect(&manager, &AppleDeviceManager::deviceLost, &widget, &InformationWidget::onDeviceLost);

    QMetaObject::invokeMethod(&manager, "updateUsbInventory", Qt::QueuedConnection, Q_ARG(bool, true));


    HelerWidget w(&manager);
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    w.setFixedSize(1, 1);
    w.show();
    w.hide();

    std::thread th(stdin_read_thread);
    th.detach();

    return app.exec();
}
