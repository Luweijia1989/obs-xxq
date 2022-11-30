#include "phone-camera.h"
#include <qcoreapplication.h>
#include <Setupapi.h>
#include <Devpkey.h>
#include <qstandardpaths.h>
#include <qfile.h>
#include <libusb-1.0/libusb.h>
#include <usbmuxd.h>

#include "ios-camera.h"

#define IOS_DEVICE_LIST "iOS_device_list"

extern "C" {
int in_install_driver;
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

QSet<QString> installingDevices;
QSet<QString> runningDevices;

PhoneCamera::PhoneCamera(obs_data_t *settings, obs_source_t *source) : m_source(source)
{
	int type = obs_data_get_int(settings, "phoneType");
	switchPhoneType(iOS);

	driverInstallationPrepare();

	auto list = enumUSBDevice();
	for (auto iter = list.begin(); iter != list.end(); iter++) {
		const QString &path = *iter;
		if (installingDevices.contains(path) || runningDevices.contains(path))
			continue;

		if (checkTargetDevice(path)) {
			if (isAOADevice(m_validDevice.vid, m_validDevice.pid))
				qDebug() << "start with aoa device connected, should reconnect";
			else
				doDriverProcess();
			break;
		}
	}
}

PhoneCamera::~PhoneCamera()
{
	qApp->removeNativeEventFilter(m_eventFilter);
	delete m_eventFilter;

	if (m_iOSCamera)
		delete m_iOSCamera;
}

void PhoneCamera::driverInstallationPrepare()
{
	initAndroidVids();

	m_eventFilter = new NativeEventFilter;
	connect(m_eventFilter, &NativeEventFilter::deviceChange, this, [=](QString devicePath, bool isAdd) {
		qDebug() << "device change, isAdd: " << isAdd << "  , path" << devicePath << "   , installing: " << installingDevices;
		if (isAdd)
			m_eventLoop.quit();

		if (installingDevices.contains(devicePath)) {
			if (isAdd) {
				int cc = 0;
				cc++;
				qDebug() << "FFFFFFFFFFFsssssssss";
			}
		} else {
			if (isAdd) {
				if (checkTargetDevice(devicePath)) {
					doDriverProcess();
				}
			}
		}
	});
	qApp->installNativeEventFilter(m_eventFilter);

	connect(&m_driverInstallProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
		[=](int exitCode, QProcess::ExitStatus exitStatus) {
			Q_UNUSED(exitStatus)

			qDebug() << (m_phoneType == iOS ? "iOS device " : "Android device ") << "driver install result code: " << exitCode << ". install path"
				 << m_validDevice.devicePath;
			installingDevices.remove(m_validDevice.devicePath);

			if (m_driverInstallTimer.isActive())
				m_driverInstallTimer.stop();
			//exit code 1=>成功 2=>失败 3=>已经安装过了 4->找不到要安装的设备

			bool success = exitCode == 1 || exitCode == 3;
			if (m_phoneType == Android) {
				if (success) {
					if (!m_validDevice.aoa) {
						// 第一阶段, 需要发送switch指令。
						QTimer t;
						t.setSingleShot(true);
						t.setInterval(1000);
						connect(&t, &QTimer::timeout, &m_eventLoop, &QEventLoop::quit);
						t.start();
						switchDroidToAcc(m_validDevice.vid, m_validDevice.pid, 1);
						m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

						if (!t.isActive())
							qDebug() << "switch aoa, no device add event, indicate a error";
					} else {
						startFinalTask();
					}
				} else
					qDebug() << "Android driver error, try restart";
			} else {
				if (success)
					startFinalTask();
				else
					qDebug() << "iOS driver error, try restart";
			}

			in_install_driver = 0;
			unlock_install();
		});

	connect(&m_driverInstallTimer, &QTimer::timeout, this, [=]() { m_driverInstallProcess.kill(); });
	connect(&m_driverInstallProcess, &QProcess::errorOccurred, this,
		[=](QProcess::ProcessError error) { qDebug() << "fail to start driver install process: " << error; });

	m_driverInstallTimer.setSingleShot(true);
	m_driverInstallTimer.setInterval(60 * 10 * 1000);
}

void PhoneCamera::initAndroidVids()
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

void PhoneCamera::switchPhoneType(int type)
{
	if (type == m_phoneType)
		return;

	m_phoneType = (PhoneType)type;

	if (m_iOSCamera) {
		m_iOSCamera->deleteLater();
		m_iOSCamera = nullptr;
	}
	if (type == iOS) {
		m_iOSCamera = new iOSCamera(this);
		connect(m_iOSCamera, &iOSCamera::updateDeviceList, this, [=](QMap<QString, QString> devices){
			m_iOSDevices = devices;
		});
		m_iOSCamera->start();
	} else if (type == Android) {
	}
}

const QMap<QString, QString> &PhoneCamera::deviceList() const
{
	return m_iOSDevices;
}

bool PhoneCamera::checkTargetDevice(QString devicePath)
{
	//todo should check this device is not used
	//device path ===> \\?\usb#vid_046d&pid_082d#e03b2c4f#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
	bool ret = false;
	bool ok = false;
	auto vid = devicePath.mid(devicePath.indexOf("vid_") + 4, 4).toInt(&ok, 16);
	auto pid = devicePath.mid(devicePath.indexOf("pid_") + 4, 4).toInt(&ok, 16);
	if (m_phoneType == iOS)
		ret = isAppleDevice(vid, pid);
	else
		ret = m_vids.contains(vid);

	if (ret) {
		m_validDevice.devicePath.clear();
		m_validDevice.devicePath = devicePath;
		m_validDevice.vid = vid;
		m_validDevice.pid = pid;
		m_validDevice.aoa = isAOADevice(vid, pid);
	}

	return ret;
}

QList<QString> PhoneCamera::enumUSBDevice()
{
	QList<QString> result;
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

		result.append(QString::fromWCharArray(detailData->DevicePath).toLower());
		free(detailData);
	}
	SetupDiDestroyDeviceInfoList(devInfo);
	return result;
}

void PhoneCamera::doDriverProcess()
{
	if (m_validDevice.devicePath.isEmpty())
		return;

	if (installingDevices.contains(m_validDevice.devicePath))
		return;

	lock_install();
	in_install_driver = 1;

	installingDevices.insert(m_validDevice.devicePath);

	QStringList p;
	p << QString::number(m_phoneType == iOS ? 1 : 0);
	p << QString::number(m_validDevice.vid);
	p << QString::number(m_validDevice.pid);
	p << m_validDevice.devicePath;
	m_driverInstallProcess.start("driver-installer.exe", p);

	m_driverInstallTimer.start();
}

void PhoneCamera::switchDroidToAcc(int vid, int pid, int force)
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

void PhoneCamera::installAndroidExtraDriver() {}

void PhoneCamera::startFinalTask()
{
	qDebug() << "startFinalTask.";
}

static const char *GetPhoneCameraInputName(void *)
{
	return "xxq phone camera plugin";
}

static void UpdatePhoneCameraInput(void *data, obs_data_t *settings);

static void *CreatePhoneCameraInput(obs_data_t *settings, obs_source_t *source)
{
	PhoneCamera *cameraInput = nullptr;

	try {
		cameraInput = new PhoneCamera(settings, source);
	} catch (const char *error) {
		blog(LOG_ERROR, "Could not create device '%s': %s", obs_source_get_name(source), error);
	}

	return cameraInput;
}

static void DestroyPhoneCameraInput(void *data)
{
	delete reinterpret_cast<PhoneCamera *>(data);
}

static void DeactivatePhoneCameraInput(void *data)
{
	auto cameraInput = reinterpret_cast<PhoneCamera *>(data);
	//cameraInput->deactivate();
}

static void ActivatePhoneCameraInput(void *data)
{
	auto cameraInput = reinterpret_cast<PhoneCamera *>(data);
	//cameraInput->activate();
}

static bool DeviceSelectionChanged(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{

	return false;
}

static bool UpdateClicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	PhoneCamera *camera = reinterpret_cast<PhoneCamera *>(data);
	auto devices = camera->deviceList();

	obs_property_t *dev_list = obs_properties_get(props, IOS_DEVICE_LIST);
	obs_property_list_clear(dev_list);

	for (auto iter = devices.begin(); iter != devices.end(); iter++) {
		obs_property_list_add_string(dev_list, iter.value().toUtf8().data(), iter.key().toUtf8().data());
	}

	return true;
}

static obs_properties_t *GetPhoneCameraProperties(void *data)
{
	UNUSED_PARAMETER(data);

	PhoneCamera *camera = reinterpret_cast<PhoneCamera *>(data);
	obs_properties_t *ppts = obs_properties_create();

	obs_property_t *p = obs_properties_add_list(ppts, IOS_DEVICE_LIST, "iOS device list", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	UpdateClicked(ppts, p, camera);

	obs_property_set_modified_callback(p, DeviceSelectionChanged);
	obs_properties_add_button(ppts, "update", "update device list", UpdateClicked);

	return ppts;
}

static void GetPhoneCameraDefaults(obs_data_t *settings) {}

static void SavePhoneCameraInput(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(settings);
}

static void UpdatePhoneCameraInput(void *data, obs_data_t *settings)
{
	PhoneCamera *input = reinterpret_cast<PhoneCamera *>(data);
	int type = obs_data_get_int(settings, "phoneType");
	QMetaObject::invokeMethod(input, "switchPhoneType", Q_ARG(int, 1));
}

void RegisterPhoneCamera()
{
	obs_source_info info = {};
	info.id = "xxq-phone-camera-source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO;
	info.get_name = GetPhoneCameraInputName;

	info.create = CreatePhoneCameraInput;
	info.destroy = DestroyPhoneCameraInput;

	info.deactivate = DeactivatePhoneCameraInput;
	info.activate = ActivatePhoneCameraInput;

	info.get_defaults = GetPhoneCameraDefaults;
	info.get_properties = GetPhoneCameraProperties;
	info.save = SavePhoneCameraInput;
	info.update = UpdatePhoneCameraInput;
	obs_register_source(&info);
}
