#include <qapplication.h>
#include <qthread.h>
#include "DriverHelper.h"
#include "InformationWidget.h"

int main(int argc, char *argv[])
{
	if (argc != 5)
		return -1;

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
}
