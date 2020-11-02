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
#include <QStandardPaths>
#include <QDir>
extern "C" {
#include <util/pipe.h>
}
#include "msapi_utf8.h"
#include <util/dstr.h>
#include <util/platform.h>
#include "../common-define.h"

static bool in_installing = false;
#define APPLE_VID 0x05AC
#define PID_RANGE_LOW 0x1290
#define PID_RANGE_MAX 0x12af
#define safe_free(p)                     \
	do {                             \
		if ((void *)p != NULL) { \
			free((void *)p); \
			p = NULL;        \
		}                        \
	} while (0)
#define IOS_USB_EXE "ios-usb-mirror.exe"
#define DRIVER_EXE "driver-tool.exe"

char *to_valid_filename(char *name, char *ext)
{
	size_t i, j, k;
	BOOL found;
	char *ret;
	wchar_t unauthorized[] =
		L"\x0001\x0002\x0003\x0004\x0005\x0006\x0007\x0008\x000a"
		L"\x000b\x000c\x000d\x000e\x000f\x0010\x0011\x0012\x0013\x0014\x0015\x0016\x0017"
		L"\x0018\x0019\x001a\x001b\x001c\x001d\x001e\x001f\x007f\"*/:<>?\\|,";
	wchar_t to_underscore[] = L" \t";
	wchar_t *wname, *wext, *wret;

	if ((name == NULL) || (ext == NULL)) {
		return NULL;
	}

	if (strlen(name) > WDI_MAX_STRLEN)
		return NULL;

	// Convert to UTF-16
	wname = utf8_to_wchar(name);
	wext = utf8_to_wchar(ext);
	if ((wname == NULL) || (wext == NULL)) {
		safe_free(wname);
		safe_free(wext);
		return NULL;
	}

	// The returned UTF-8 string will never be larger than the sum of its parts
	wret = (wchar_t *)calloc(2 * (wcslen(wname) + wcslen(wext) + 2), 1);
	if (wret == NULL) {
		safe_free(wname);
		safe_free(wext);
		return NULL;
	}
	wcscpy(wret, wname);
	safe_free(wname);
	wcscat(wret, wext);
	safe_free(wext);

	for (i = 0, k = 0; i < wcslen(wret); i++) {
		found = FALSE;
		for (j = 0; j < wcslen(unauthorized); j++) {
			if (wret[i] == unauthorized[j]) {
				found = TRUE;
				break;
			}
		}
		if (found)
			continue;
		found = FALSE;
		for (j = 0; j < wcslen(to_underscore); j++) {
			if (wret[i] == to_underscore[j]) {
				wret[k++] = '_';
				found = TRUE;
				break;
			}
		}
		if (found)
			continue;
		wret[k++] = wret[i];
	}
	wret[k] = 0;
	ret = wchar_to_utf8(wret);
	safe_free(wret);
	return ret;
}

class InstallHelper : public QObject {
	Q_OBJECT
public:
	InstallHelper()
	{
		m_terminateTimer = new QTimer(this);
		m_terminateTimer->setSingleShot(true);
		m_terminateTimer->setInterval(400);
		connect(m_terminateTimer, &QTimer::timeout, this,
			&InstallHelper::stopMirror);

		temp_dir = QStandardPaths::writableLocation(
				   QStandardPaths::TempLocation) +
			   "/yuerlive";
		temp_dir = QDir::toNativeSeparators(temp_dir);
		QDir d(temp_dir);
		if (!d.exists())
			d.mkdir(temp_dir);

		wdi_is_driver_supported(WDI_LIBUSB0, &file_info);

		target_driver_version = file_info.dwFileVersionMS;
		target_driver_version <<= 32;
		target_driver_version += file_info.dwFileVersionLS;
		pd_options.driver_type = 1;
	}
	~InstallHelper()
	{
		if (m_list)
			wdi_destroy_list(m_list);
	}
signals:
	void installDriver();
	void updateDevices();
	void installStatusChanged(bool b);
	void startMirror();
	void stopMirror();

public slots:
	void onInstall()
	{
		in_installing = true;
		installStatusChanged(in_installing);

		char *inf_name = to_valid_filename(m_device->desc, ".inf");
		if (inf_name == NULL) {
			qDebug() << "inf name invalid";
			goto out;
		}

		qDebug() << "use info name: " << inf_name;
		int res = wdi_prepare_driver(m_device,
					     temp_dir.toStdString().c_str(),
					     inf_name, &pd_options);
		if (res == WDI_SUCCESS) {
			qDebug() << "Successfully extracted driver files.";

			res = wdi_install_driver(m_device,
						 temp_dir.toStdString().c_str(),
						 inf_name, &id_options);
			if (res == WDI_SUCCESS) {
				qDebug() << "Successfully install the driver";
				requestStart();
			}
		}

	out:
		safe_free(inf_name);
		m_device = nullptr;
		in_installing = false;
		installStatusChanged(in_installing);
	}

	void onUpdateDevices()
	{
		if (in_installing)
			return;

		if (m_list) {
			wdi_destroy_list(m_list);
			m_list = nullptr;
		}
		wdi_options_create_list option = {true, true, false};
		wdi_create_list(&m_list, &option);

		bool foundApple = false;
		for (wdi_device_info *dev = m_list; dev != NULL;
		     dev = dev->next) {
			if (dev->vid == APPLE_VID && (dev->pid > PID_RANGE_LOW && dev->pid < PID_RANGE_MAX)) {
				if (!dev->is_composite) {
					QString driverName(dev->driver);
					bool needInstall = true;
					if (driverName.startsWith("libusb0")) {
						if (dev->driver_version ==
						    target_driver_version)
							needInstall = false;
					}

					if (needInstall) {
						m_device = dev;
						emit installDriver();
					} else
						requestStart();

					foundApple = true;
					break;
				}
			}
		}

		if (!foundApple)
			m_terminateTimer->start();
	}

private:
	void requestStart()
	{
		if (m_terminateTimer->isActive())
			m_terminateTimer->stop();

		emit startMirror();
	}

private:
	QString temp_dir;
	VS_FIXEDFILEINFO file_info;
	UINT64 target_driver_version = 0;
	wdi_options_prepare_driver pd_options = {0};
	wdi_options_install_driver id_options = {0};
	wdi_device_info *m_device = nullptr;
	wdi_device_info *m_list = nullptr;
	QTimer *m_terminateTimer;
};

class NativeEventFilter : public QAbstractNativeEventFilter {
public:
	NativeEventFilter(InstallHelper *helper) : m_helper(helper) {}
	bool nativeEventFilter(const QByteArray &eventType, void *message,
			       long *) override
	{
		Q_UNUSED(eventType)
		MSG *msg = static_cast<MSG *>(message);
		int msgType = msg->message;
		if (msgType == WM_DEVICECHANGE) {
			emit m_helper->updateDevices();
		}
		return false;
	}

private:
	InstallHelper *m_helper;
};

class ProgressBar : public QProgressBar {
	Q_OBJECT
public:
	ProgressBar(InstallHelper *helper, QWidget *parent = nullptr)
		: QProgressBar(parent), m_helper(helper)
	{
		setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint |
			       Qt::WindowTitleHint | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
		setWindowTitle(u8"正在安装驱动");
		setMinimum(0);
		setMaximum(0);
		setTextVisible(false);
		setOrientation(Qt::Horizontal);

		emit m_helper->updateDevices();

		connect(helper, &InstallHelper::installStatusChanged, this,
			[=](bool b) { setVisible(b); });
		connect(helper, &InstallHelper::startMirror, this,
			&ProgressBar::onStartMirror);
		connect(helper, &InstallHelper::stopMirror, this,
			&ProgressBar::onStopMirror);
	}

public slots:
	void onStopMirror()
	{
		if (!m_mirrorProcess)
			return;

		uint8_t data[1] = {1};
		os_process_pipe_write(m_mirrorProcess, data, 1);
		os_process_pipe_destroy_timeout(m_mirrorProcess, 1500);
		m_mirrorProcess = NULL;
	}

	void onStop()
	{
		onStopMirror();
		if (in_installing)
			os_kill_process(DRIVER_EXE);
		else
			qApp->quit();
	}
	 
	void onStartMirror()
	{
		if (m_mirrorProcess)
			return;

		struct dstr cmd;
		dstr_init_move_array(&cmd,
				     os_get_executable_path_ptr(IOS_USB_EXE));
		dstr_insert_ch(&cmd, 0, '\"');
		dstr_cat(&cmd, "\" \"");
		m_mirrorProcess = os_process_pipe_create(cmd.array, "w");
		dstr_free(&cmd);
	}

private:
	InstallHelper *m_helper;
	os_process_pipe_t *m_mirrorProcess = nullptr;
};

class StdInThread : public QThread {
	Q_OBJECT
public:
	StdInThread(QObject *parent = nullptr) : QThread(parent) {}
	virtual void run() override
	{
		uint8_t buf[1024] = {0};
		while (true) {
			int read_len = fread(
				buf, 1, 1024,
				stdin); // read 0 means parent has been stopped
			if (read_len) {
				if (buf[0] == 1) {
					emit stop();
					break;
				}
			} else {
				emit stop();
				break;
			}
		}
	}

signals:
	void stop();
};

int main(int argc, char *argv[])
{
	os_kill_process(IOS_USB_EXE);
	SetErrorMode(SEM_FAILCRITICALERRORS);
	_setmode(_fileno(stdin), O_BINARY);

	struct IPCClient *client = NULL;
	ipc_client_create(&client);
	send_status(client, MIRROR_STOP);
	ipc_client_destroy(&client);

	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

	QApplication a(argc, argv);

	InstallHelper helper;
	QThread thread;
	thread.start();
	NativeEventFilter filter(&helper);
	a.installNativeEventFilter(&filter);

	QObject::connect(&a, &QApplication::aboutToQuit, &thread,
			 &QThread::quit);
	QObject::connect(&helper, &InstallHelper::installDriver, &helper,
			 &InstallHelper::onInstall);
	QObject::connect(&helper, &InstallHelper::updateDevices, &helper,
			 &InstallHelper::onUpdateDevices);
	helper.moveToThread(&thread);

	StdInThread stdinThread;
	ProgressBar bar(&helper);
	QObject::connect(&stdinThread, &StdInThread::stop, &bar,
			 &ProgressBar::onStop);

	bar.setWindowFlag(Qt::FramelessWindowHint, true);
	bar.setFixedSize(1,1);
	bar.show();
	bar.hide();
	bar.setWindowFlag(Qt::FramelessWindowHint, false);
	bar.setFixedSize(280, 32);
	stdinThread.start();

	return a.exec();
}

#include "main.moc"
