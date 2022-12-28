#include "driver-helper.h"
#include <qstandardpaths.h>
#include <qfile.h>
#include <qdebug.h>
#include <qapplication.h>
#include <pthread.h>
#include <libusb-1.0/libusb.h>
#include <Setupapi.h>
#include <Devpkey.h>
#include "usb-device-reset-helper.h"
#include "application.h"

extern "C" {
pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

void lock_install()
{
	pthread_mutex_lock(&lock);
}

void unlock_install()
{
	pthread_mutex_unlock(&lock);
}
}

extern QSet<QString> installingDevices;
extern QSet<QString> runningDevices;
extern QSet<QString> iOSDevices;
extern QSet<QString> AndroidDevices;

DriverHelper::DriverHelper(QObject *parent) : QObject(parent)
{
	initAndroidVids();

	m_eventFilter = new NativeEventFilter;
	connect(m_eventFilter, &NativeEventFilter::deviceChange, this, [=](QString devicePath, bool isAdd) {
		qDebug() << "device change, isAdd: " << isAdd << "  , path" << devicePath << "   , installing: " << installingDevices;
		add_usb_device_change_event();
		if (isAdd) {
			m_eventLoop.quit();
			doDriverProcess(devicePath);
		} else {
			iOSDevices.remove(devicePath);
			AndroidDevices.remove(devicePath);
		}
	});
	qApp->installNativeEventFilter(m_eventFilter);
}

DriverHelper::~DriverHelper()
{
	qApp->removeNativeEventFilter(m_eventFilter);
	delete m_eventFilter;

	for (auto iter = m_installs.begin(); iter != m_installs.end(); iter++) {
		QProcess *p = *iter;
		p->kill();
		p->waitForFinished();
	}
	m_installs.clear();
}

void DriverHelper::checkDevices()
{
	auto list = enumUSBDevice();
	for (auto iter = list.begin(); iter != list.end(); iter++) {
		const QString &path = iter.key();
		if (doDriverProcess(path, true))
			break;
	}
}

QMap<QString, QPair<QString, uint32_t>> DriverHelper::androidDevices()
{
	auto all = enumUSBDevice();
	QMap<QString, QPair<QString, uint32_t>> ret;
	for (auto iter = all.begin(); iter != all.end(); iter++) {
		auto path = iter.key();
		bool ok = false;
		auto vid = path.mid(path.indexOf("vid_") + 4, 4).toInt(&ok, 16);
		if (m_vids.contains(vid))
			ret.insert(iter.key(), {iter.value(), 0});
	}

	return ret;
}

void DriverHelper::initAndroidVids()
{
	auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/vids.txt";
	QString vids;
	QFile idFile(path);
	if (idFile.exists()) {
		idFile.open(QFile::ReadOnly);
		vids = idFile.readAll();
		idFile.close();
	} else
		vids = "2d95,12d1,22d9,2717,2a70,04e8,2a45,0b05,17ef,29a9,109b,054c,18d1,19d2";

	bool ok = false;
	if (!vids.isEmpty()) {
		auto list = vids.split(',');
		for (auto it : list) {
			auto id = it.toInt(&ok, 16);
			if (ok)
				m_vids.insert(id);
		}
	}
}

QMap<QString, QString> DriverHelper::enumUSBDevice()
{
	QMap<QString, QString> result;
	HDEVINFO devInfo = SetupDiGetClassDevs(&USB_DEVICE_GUID, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (!devInfo)
		return result;

	SP_DEVINFO_DATA devData;
	devData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (int i = 0;; i++) {
		SetupDiEnumDeviceInfo(devInfo, i, &devData);
		if (GetLastError() == ERROR_NO_MORE_ITEMS)
			break;

		auto getDeviceProperty = [](HDEVINFO di, SP_DEVINFO_DATA *did, DWORD property) -> QString {
			QString result;
			DWORD DataT = 0;
			LPTSTR buffer = NULL;
			DWORD buffersize = 0;
			if (!SetupDiGetDeviceRegistryProperty(di, did, property, &DataT, (PBYTE)buffer, buffersize, &buffersize))
				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
					return result;

			buffer = (TCHAR *)LocalAlloc(LPTR, buffersize);
			if (!SetupDiGetDeviceRegistryProperty(di, did, property, &DataT, (PBYTE)buffer, buffersize, &buffersize)) {
				LocalFree(buffer);
				return result;
			}
			result = QString::fromWCharArray(buffer);
			LocalFree(buffer);
			return result;
		};
		QString deviceGUID = getDeviceProperty(devInfo, &devData, SPDRP_CLASSGUID);
		if (deviceGUID.contains(GUID_MOUSE, Qt::CaseInsensitive) || deviceGUID.contains(GUID_KEYBOARD, Qt::CaseInsensitive) ||
		    deviceGUID.contains(GUID_HID, Qt::CaseInsensitive))
			continue;

		QString deviceHardwareId = getDeviceProperty(devInfo, &devData, SPDRP_HARDWAREID);
		if (deviceHardwareId.contains("root", Qt::CaseInsensitive) || deviceHardwareId.contains("hub", Qt::CaseInsensitive))
			continue;

		qDebug() << deviceHardwareId << getDeviceProperty(devInfo, &devData, SPDRP_COMPATIBLEIDS);

		SP_DEVICE_INTERFACE_DATA interfaceData;
		interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		if (!SetupDiEnumDeviceInterfaces(devInfo, NULL, &USB_DEVICE_GUID, i, &interfaceData)) {
			continue;
		}

		// Determine required size for interface detail data
		ULONG requiredLength = 0;
		if (!SetupDiGetDeviceInterfaceDetail(devInfo, &interfaceData, NULL, 0, &requiredLength, NULL)) {
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				continue;
			}
		}

		// Allocate storage for interface detail data
		PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredLength);
		detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (!SetupDiGetDeviceInterfaceDetail(devInfo, &interfaceData, detailData, requiredLength, &requiredLength, NULL)) {
			free(detailData);
			continue;
		}

		auto desc = getDeviceProperty(devInfo, &devData, SPDRP_DEVICEDESC);
		result.insert(QString::fromWCharArray(detailData->DevicePath).toLower(), desc);
		free(detailData);
	}
	SetupDiDestroyDeviceInfoList(devInfo);
	return result;
}

bool DriverHelper::doDriverProcess(QString devicePath, bool checkAoA)
{
	if (devicePath.isEmpty())
		return false;

	if (installingDevices.contains(devicePath) || runningDevices.contains(devicePath)) {
		qDebug() << devicePath << " in use.";
		return false;
	}

	bool ok = false;
	auto vid = devicePath.mid(devicePath.indexOf("vid_") + 4, 4).toInt(&ok, 16);
	auto pid = devicePath.mid(devicePath.indexOf("pid_") + 4, 4).toInt(&ok, 16);

	auto androidValid = [this, checkAoA, vid, pid, devicePath]() {
		if (checkAoA && isAOADevice(vid, pid)) {
			qDebug() << "start with aoa device connected, should reconnect, device path==>" << devicePath;
			return false;
		}

		return m_vids.contains(vid);
	};

	bool isAndroid = androidValid();
	bool isApple = isAppleDevice(vid, pid);

	if (isAndroid) {
		if (!App()->mediaAvailable(PhoneType::Android))
			return false;
	} else if (isApple) {
		if (!App()->mediaAvailable(PhoneType::iOS))
			return false;
	} else
		return false;

	qDebug() << "using device: " << devicePath;

	lock_install();

	installingDevices.insert(devicePath);

	QStringList p;
	p << QString::number(isAppleDevice(vid, pid) ? 1 : 0);
	p << QString::number(vid);
	p << QString::number(pid);
	p << devicePath;

	QProcess *process = new QProcess;
	QTimer *timer = new QTimer;

	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
		Q_UNUSED(exitStatus)

		qDebug() << (isApple ? "iOS device " : "Android device ") << "driver install result code: " << exitCode << ". install path" << devicePath;
		installingDevices.remove(devicePath);

		if (timer->isActive())
			timer->stop();
		//exit code 1=>成功 2=>失败 3=>已经安装过了 4->找不到要安装的设备

		bool success = exitCode == 1 || exitCode == 3;
		if (!isApple) {
			if (success) {
				if (!isAOADevice(vid, pid)) {
					// 第一阶段, 需要发送switch指令。
					QTimer t;
					t.setSingleShot(true);
					t.setInterval(1000);
					connect(&t, &QTimer::timeout, &m_eventLoop, &QEventLoop::quit);
					t.start();
					switchDroidToAcc(vid, pid, 1);
					m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

					if (!t.isActive())
						qDebug() << "switch aoa, no device add event, indicate a error";
				} else
					AndroidDevices.insert(devicePath);
			} else
				qDebug() << "Android driver error, try restart";
		} else {
			if (success)
				iOSDevices.insert(devicePath);
			else
				qDebug() << "iOS driver error, try restart";
		}

		unlock_install();

		process->deleteLater();
		timer->deleteLater();
		m_installs.remove(process);

		add_usb_device_change_event();
	});

	connect(timer, &QTimer::timeout, this, [=]() { process->kill(); });
	connect(process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) { qDebug() << "fail to start driver install process: " << error; });

	timer->setSingleShot(true);
	timer->setInterval(60 * 10 * 1000);

	process->start("device-helper.exe", p);
	timer->start();

	return true;
}

void DriverHelper::switchDroidToAcc(int vid, int pid, int force)
{
	struct libusb_device_handle *handle = nullptr;
	unsigned char ioBuffer[2];
	int r;
	int deviceProtocol;

	libusb_context *ctx = nullptr;
	libusb_init(&ctx);
	do {
		auto handle = libusb_open_device_with_vid_pid(ctx, vid, pid);
		if (!handle) {
			qDebug("Failed to connect to device\n");
			break;
		}

		if (libusb_kernel_driver_active(handle, 0) > 0) {
			if (!force) {
				qDebug("kernel driver active, ignoring device");
				break;
			}
			if (libusb_detach_kernel_driver(handle, 0) != 0) {
				qDebug("failed to detach kernel driver, ignoring device");
				break;
			}
		}
		if (0 > (r = libusb_control_transfer(handle,
						     0xC0, //bmRequestType
						     51,   //Get Protocol
						     0, 0, ioBuffer, 2, 2000))) {
			qDebug("get protocol call failed\n");
			break;
		}

		deviceProtocol = ioBuffer[1] << 8 | ioBuffer[0];
		if (deviceProtocol < AOA_PROTOCOL_MIN || deviceProtocol > AOA_PROTOCOL_MAX) {
			qDebug("Unsupported AOA protocol %d\n", deviceProtocol);
			break;
		}

		const char *setupStrings[6];
		setupStrings[0] = vendor;
		setupStrings[1] = model;
		setupStrings[2] = description;
		setupStrings[3] = version;
		setupStrings[4] = uri;
		setupStrings[5] = serial;

		int i;
		for (i = 0; i < 6; i++) {
			if (0 >
			    (r = libusb_control_transfer(handle, 0x40, 52, 0, (uint16_t)i, (unsigned char *)setupStrings[i], strlen(setupStrings[i]), 2000))) {
				qDebug("send string %d call failed\n", i);
				break;
			}
		}

		if (deviceProtocol >= 2) {
			if (0 > (r = libusb_control_transfer(handle, 0x40, 58,
#ifdef USE_AUDIO
							     1, // 0=no audio, 1=2ch,16bit PCM, 44.1kHz
#else
							     0,
#endif
							     0, NULL, 0, 2000))) {
				qDebug("set audio call failed\n");
				break;
			}
		}

		if (0 > (r = libusb_control_transfer(handle, 0x40, 53, 0, 0, NULL, 0, 2000))) {
			qDebug("start accessory call failed\n");
			break;
		}
	} while (0);

	if (handle)
		libusb_close(handle);

	libusb_exit(ctx);
}
