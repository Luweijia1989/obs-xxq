#include <QApplication>
#include <Windows.h>
#include <QProgressBar>
#include <fcntl.h>
#include <io.h>
#include <libwdi.h>
#include <QAbstractNativeEventFilter>
#include <QDebug>
#include <QThread>
#include <QTimer>

static bool in_installing = false;

class NativeEventFilter : public QAbstractNativeEventFilter {
public:
	NativeEventFilter() {}
	bool nativeEventFilter(const QByteArray &eventType, void *message,
			       long *) override
	{
		Q_UNUSED(eventType)
		MSG *msg = static_cast<MSG *>(message);
		int msgType = msg->message;
		if (msgType == WM_DEVICECHANGE) {
			
		}
		return false;
	}
};

class ProgressBar : public QProgressBar {
	Q_OBJECT
public:
	ProgressBar(QWidget *parent = nullptr) : QProgressBar(parent)
	{
		setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint |
			       Qt::WindowTitleHint| Qt::WindowStaysOnTopHint);
		setWindowTitle(u8"正在安装驱动");
		setMinimum(0);
		setMaximum(0);
		setTextVisible(false);
		setOrientation(Qt::Horizontal);
		setFixedSize(280, 32);
	}

signals:
	void test();
};

class InstallHelper : public QObject {
	Q_OBJECT
public:
	InstallHelper()
	{

	}
signals:
	void installDriver();

public slots:
	void onInstall()
	{
		
	}
};

int main(int argc, char *argv[])
{
	SetErrorMode(SEM_FAILCRITICALERRORS);
	_setmode(_fileno(stdin), O_BINARY);

	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

	QApplication a(argc, argv);

	NativeEventFilter filter;
	a.installNativeEventFilter(&filter);

	InstallHelper helper;
	QThread thread;
	QObject::connect(&a, &QApplication::aboutToQuit, &thread, &QThread::quit);
	QObject::connect(&helper, &InstallHelper::installDriver, &helper, &InstallHelper::onInstall);
	helper.moveToThread(&thread);
	thread.start();

	//wdi_device_info *list = nullptr;
	//wdi_options_create_list option = {true, true, false};
	//wdi_create_list(&list, &option);

	ProgressBar bar;
	bar.show();

	return a.exec();
}

#include "main.moc"
