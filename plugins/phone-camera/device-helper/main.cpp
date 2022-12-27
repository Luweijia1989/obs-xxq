#include <qapplication.h>
#include <qthread.h>
#include "DriverHelper.h"
#include "InformationWidget.h"
#include <libusb-1.0/libusb.h>

static void resetUsbDevice(char *t_serial)
{
	libusb_init(NULL);

	libusb_device **list;
	auto devsCount = libusb_get_device_list(NULL, &list);
	for (size_t i = 0; i < devsCount; i++) {
		libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(list[i], &desc) != LIBUSB_SUCCESS)
			continue;

		libusb_device_handle *handle = nullptr;
		if (libusb_open(list[i], &handle) == LIBUSB_SUCCESS) {
			char serial[40] = {0};
			if (libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (uint8_t *)serial, 40) <= 0) {
				libusb_close(handle);
				continue;
			}

			if (strcmp(serial, t_serial) == 0) {
				libusb_claim_interface(handle, 0);
				libusb_reset_device(handle);
				libusb_close(handle);
				break;
			}

			libusb_close(handle);
		}
	}
	libusb_free_device_list(list, 1);

	libusb_exit(NULL);
}

int main(int argc, char *argv[])
{
	if (argc == 5) {

		QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
		QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

		bool isAppleDevice = QString(argv[1]).toInt() == 1;
		int vid = QString(argv[2]).toInt();
		int pid = QString(argv[3]).toInt();
		QString path = QString(argv[4]);

		QApplication app(argc, argv);
		app.setQuitOnLastWindowClosed(false);
		app.setApplicationName("yuerlive");

		QThread thread;
		QObject::connect(&app, &QApplication::aboutToQuit, &thread, &QThread::quit);
		thread.start();

		DriverHelper helper;
		helper.moveToThread(&thread);

		InformationWidget widget;
		QObject::connect(&helper, &DriverHelper::installProgress, &widget, &InformationWidget::onInstallStatus);
		QObject::connect(&helper, &DriverHelper::installError, &widget, &InformationWidget::onInstallError);
		QObject::connect(&helper, &DriverHelper::changeTip, &widget, &InformationWidget::onChangeTip);
		QObject::connect(&helper, &DriverHelper::complete, [](int status) { qApp->exit(status); });

		QMetaObject::invokeMethod(&helper, "checkInstall", Q_ARG(bool, isAppleDevice), Q_ARG(int, vid), Q_ARG(int, pid), Q_ARG(QString, path));

		return app.exec();
	} else if (argc == 2) {
		resetUsbDevice(argv[1]);
	}

	return 0;
}
