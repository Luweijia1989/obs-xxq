#include "AOADeviceManager.h"
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include "common-define.h"
#include <winusb.h>
#include <Setupapi.h>
#include <Devpkey.h>

#pragma comment(lib, "winusb.lib")

const char *vendor = "YPP";
const char *model = "XxqProjectionApp"; //根据这个model来打开手机的app
const char *description = u8"鱼耳直播助手";
const char *version = "1.0";
const char *uri = "https://www.yuerzhibo.com";
const char *serial = "0000000012345678";

int isAOADevice(int vid, int pid)
{
	if (vid == VID_GOOGLE) {
		switch (pid) {
		case PID_AOA_ACC:
		case PID_AOA_ACC_ADB:
		case PID_AOA_ACC_AU:
		case PID_AOA_ACC_AU_ADB:
			return 1;
		case PID_AOA_AU:
		case PID_AOA_AU_ADB:
			qDebug("device is audio-only\n");
			break;
		default:
			break;
		}
	}

	return 0;
}

GUID ADB_DEVICE_GUID = {0xf72fe0d4,
			0xcbcb,
			0x407d,
			{0x88, 0x14, 0x9e, 0xd6, 0x73, 0xd0, 0xdd, 0x6b}};

GUID USB_DEVICE_GUID = {0xA5DCBF10,
			0x6530,
			0x11D2,
			{0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
			     
static QString GUID_HID      = "745A17A0-74D3-11D0-B6FE-00A0C90F57DA";
static QString GUID_KEYBOARD = "4d36e96b-e325-11ce-bfc1-08002be10318";
static QString GUID_MOUSE    = "4d36e96f-e325-11ce-bfc1-08002be10318";
AOADeviceManager::AOADeviceManager()
{
	auto path = QStandardPaths::writableLocation(
			    QStandardPaths::AppLocalDataLocation) +
		    "/vids.txt";
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

	ipc_client_create(&m_client);
	m_driverHelper = new DriverHelper(this);
	connect(m_driverHelper, &DriverHelper::installProgress, this,
		&AOADeviceManager::installProgress);
	connect(m_driverHelper, &DriverHelper::installError, this,
		&AOADeviceManager::installError);

	circlebuf_init(&m_mediaDataBuffer);

	//h264.setFileName("E:\\android.h264");
	//h264.open(QFile::ReadWrite);
}

AOADeviceManager::~AOADeviceManager()
{
	disconnectDevice();

	circlebuf_free(&m_mediaDataBuffer);
	if (m_cacheBuffer)
		free(m_cacheBuffer);

	ipc_client_destroy(&m_client);
}

int AOADeviceManager::initUSB()
{
	int r;
	r = libusb_init(&m_ctx);
	if (r < 0) {
		qDebug() << "error init usb context";
		return r;
	}
	libusb_set_debug(m_ctx, LIBUSB_LOG_LEVEL_NONE);
	return 0;
}

void AOADeviceManager::clearUSB()
{
	if (m_devs) {
		libusb_free_device_list(m_devs, m_devs_count);
		m_devs = NULL;
	}

	if (m_ctx != NULL) {
		libusb_exit(m_ctx); //close the session
		m_ctx = NULL;
	}
}

int AOADeviceManager::setupDroid(libusb_device *usbDevice,
				 accessory_droid *device)
{

	memset(device, 0, sizeof(accessory_droid));

	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(usbDevice, &desc);
	if (r < 0) {
		qDebug("failed to get device descriptor\n");
		return r;
	}

	struct libusb_config_descriptor *config;
	r = libusb_get_config_descriptor(usbDevice, 0, &config);
	if (r < 0) {
		qDebug("failed t oget config descriptor\n");
		return r;
	}

	const struct libusb_interface *inter;
	const struct libusb_interface_descriptor *interdesc;
	const struct libusb_endpoint_descriptor *epdesc;
	int i, j, k;

	for (i = 0; i < (int)config->bNumInterfaces; i++) {
		qDebug("checking interface #%d\n", i);
		inter = &config->interface[i];
		if (inter == NULL) {
			qDebug("interface is null\n");
			continue;
		}
		qDebug("interface has %d alternate settings\n",
		       inter->num_altsetting);
		for (j = 0; j < inter->num_altsetting; j++) {
			interdesc = &inter->altsetting[j];
			if (interdesc->bNumEndpoints == 2 &&
			    interdesc->bInterfaceClass == 0xff &&
			    interdesc->bInterfaceSubClass == 0xff &&
			    (device->inendp <= 0 || device->outendp <= 0)) {
				qDebug("interface %d is accessory candidate\n",
				       i);
				for (k = 0; k < (int)interdesc->bNumEndpoints;
				     k++) {
					epdesc = &interdesc->endpoint[k];
					if (epdesc->bmAttributes != 0x02) {
						break;
					}
					if ((epdesc->bEndpointAddress &
					     LIBUSB_ENDPOINT_IN) &&
					    device->inendp <= 0) {
						device->inendp =
							epdesc->bEndpointAddress;
						device->inpacketsize =
							epdesc->wMaxPacketSize;
						qDebug("using EP 0x%02x as bulk-in EP\n",
						       (int)device->inendp);
					} else if ((!(epdesc->bEndpointAddress &
						      LIBUSB_ENDPOINT_IN)) &&
						   device->outendp <= 0) {
						device->outendp =
							epdesc->bEndpointAddress;
						device->outpacketsize =
							epdesc->wMaxPacketSize;
						qDebug("using EP 0x%02x as bulk-out EP\n",
						       (int)device->outendp);
					} else {
						break;
					}
				}
				if (device->inendp && device->outendp) {
					device->bulkInterface =
						interdesc->bInterfaceNumber;
				}
			}
		}
	}

	if (!(device->inendp && device->outendp)) {
		qDebug("device has no accessory endpoints\n");
		return -2;
	}

	r = libusb_open(usbDevice, &device->usbHandle);
	if (r < 0) {
		qDebug("failed to open usb handle\n");
		return r;
	}

	r = libusb_claim_interface(device->usbHandle, device->bulkInterface);
	if (r < 0) {
		qDebug("failed to claim bulk interface\n");
		libusb_close(device->usbHandle);
		return r;
	}

	device->connected = true;
	device->c_vid = desc.idVendor;
	device->c_pid = desc.idProduct;

	return 0;
}

int AOADeviceManager::shutdownUSBDroid(accessory_droid *device)
{

	if (device->audioendp)
		libusb_release_interface(device->usbHandle,
					 device->audioInterface);

	if ((device->inendp && device->outendp))
		libusb_release_interface(device->usbHandle,
					 device->bulkInterface);

	if (device->usbHandle != NULL)
		libusb_close(device->usbHandle);

	device->connected = false;
	device->c_vid = 0;
	device->c_pid = 0;

	return 0;
}

void AOADeviceManager::switchDroidToAcc(int vid, int pid, int force)
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
		if (deviceProtocol < AOA_PROTOCOL_MIN ||
		    deviceProtocol > AOA_PROTOCOL_MAX) {
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
			if (0 > (r = libusb_control_transfer(
					 handle, 0x40, 52, 0, (uint16_t)i,
					 (unsigned char *)setupStrings[i],
					 strlen(setupStrings[i]), 2000))) {
				qDebug("send string %d call failed\n", i);
				break;
			}
		}

		if (deviceProtocol >= 2) {
			if (0 > (r = libusb_control_transfer(
					 handle, 0x40, 58,
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

		if (0 > (r = libusb_control_transfer(handle, 0x40, 53, 0, 0, NULL, 0,
						     2000))) {
			qDebug("start accessory call failed\n");
			break;
		}
	} while (0);

	if(handle)
		libusb_close(handle);

	libusb_exit(ctx);
}

bool AOADeviceManager::handleMediaData()
{
	size_t headerSize = 4 + 8 + 1;

	if (m_mediaDataBuffer.size < headerSize)
		return false;

	size_t pktSize = 0;
	circlebuf_peek_front(&m_mediaDataBuffer, &pktSize, 4);

	size_t totalSize = pktSize + headerSize;
	if (m_mediaDataBuffer.size < totalSize)
		return false;

	if (!m_cacheBuffer) {
		m_cacheBufferSize = headerSize + pktSize;
		m_cacheBuffer = (uint8_t *)malloc(m_cacheBufferSize);
	} else if (m_cacheBufferSize < headerSize + pktSize) {
		m_cacheBufferSize = headerSize + pktSize;
		m_cacheBuffer =
			(uint8_t *)realloc(m_cacheBuffer, m_cacheBufferSize);
	}

	circlebuf_pop_front(&m_mediaDataBuffer, m_cacheBuffer, headerSize);
	int type = m_cacheBuffer[4];
	int64_t pts = 0;
	memcpy(&pts, m_cacheBuffer + 5, 8);
	circlebuf_pop_front(&m_mediaDataBuffer, m_cacheBuffer, pktSize);

	if (type == 1) { //video
		bool is_pps = pts == ((int64_t)UINT64_C(0x8000000000000000));
		if (is_pps) {
			struct av_packet_info pack_info = {0};
			pack_info.size = sizeof(struct media_video_info);
			pack_info.type = FFM_MEDIA_VIDEO_INFO;
			pack_info.pts = 0;

			struct media_video_info info;
			info.video_extra_len = pktSize;
			memcpy(info.video_extra, m_cacheBuffer, pktSize);
			ipc_client_write_2(m_client, &pack_info,
					   sizeof(struct av_packet_info), &info,
					   sizeof(struct media_video_info), INFINITE);

			struct media_audio_info audio_info;
			audio_info.format = AUDIO_FORMAT_16BIT;
			audio_info.samples_per_sec = 48000;
			audio_info.speakers = SPEAKERS_STEREO;

			struct av_packet_info audio_pack_info = {0};
			audio_pack_info.size = sizeof(struct media_audio_info);
			audio_pack_info.type = FFM_MEDIA_AUDIO_INFO;
			ipc_client_write_2(m_client, &audio_pack_info,
					   sizeof(struct av_packet_info), &audio_info,
					   sizeof(struct media_audio_info),
					   INFINITE);
		} else {
			struct av_packet_info pack_info = {0};
			pack_info.size = pktSize;
			pack_info.type = FFM_PACKET_VIDEO;
			pack_info.pts = pts * 1000000;
			ipc_client_write_2(m_client, &pack_info,
					   sizeof(struct av_packet_info),
					   m_cacheBuffer, pack_info.size,
					   INFINITE);
		}
	} else {
		struct av_packet_info pack_info = {0};
		pack_info.size = pktSize;
		pack_info.type = FFM_PACKET_AUDIO;
		pack_info.pts = pts * 1000000;
		ipc_client_write_2(m_client, &pack_info,
				   sizeof(struct av_packet_info), m_cacheBuffer,
				   pack_info.size, INFINITE);
	}
	return true;
}
#include <QElapsedTimer>
void *AOADeviceManager::a2s_usbRxThread(void *d)
{
	AOADeviceManager *device = (AOADeviceManager *)d;

	if (!device->buffer)
		device->buffer =
			(unsigned char *)malloc(device->m_droid.inpacketsize);

	while (device->m_continuousRead) {
		int len = 0;
		int r = libusb_bulk_transfer(
			device->m_droid.usbHandle, device->m_droid.inendp,
			device->buffer, device->m_droid.inpacketsize, &len, 200);
		if (r == 0) {
			circlebuf_push_back(&device->m_mediaDataBuffer,
					    device->buffer, len);

			while (true) {
				bool b = device->handleMediaData();

				if (b)
					continue;
				else
					break;
			}
		} else if (r == LIBUSB_ERROR_TIMEOUT) {

			continue;
		} else
			break;
	}

	circlebuf_free(&device->m_mediaDataBuffer);

	QMetaObject::invokeMethod(device, "disconnectDevice",
				  Qt::QueuedConnection);

	qDebug("a2s_usbRxThread finished\n");
	return NULL;
}

int AOADeviceManager::startUSBPipe()
{
	m_continuousRead = true;
	m_usbReadThread = std::thread(AOADeviceManager::a2s_usbRxThread, this);
	m_heartbeatThread =
		std::thread(AOADeviceManager::heartbeatThread, this);

	return 0;
}

void AOADeviceManager::stopUSBPipe()
{
	m_continuousRead = false;
	qDebug("waiting for usb rx thread...\n");
	if (m_usbReadThread.joinable())
		m_usbReadThread.join();

	if (m_heartbeatThread.joinable()) {
		m_timeoutCondition.wakeOne();
		m_heartbeatThread.join();
	}

	qDebug("usb threads stopped\n");

	// h264.close();
}

void *AOADeviceManager::startThread(void *d)
{
	AOADeviceManager *manager = (AOADeviceManager *)d;

	int r = -1;
	while (!manager->m_exitStart) {
		int len = 0;
		unsigned char data = 1;
		manager->m_usbMutex.lock();
		r = libusb_bulk_transfer(manager->m_droid.usbHandle,
					 manager->m_droid.outendp, &data, 1,
					 &len, 100);
		manager->m_usbMutex.unlock();

		if (r == LIBUSB_ERROR_TIMEOUT)
			continue;
		else
			break;
	}

	if (r == LIBUSB_SUCCESS) {
		manager->startUSBPipe();
		qDebug("new Android connected");
		send_status(manager->m_client, MIRROR_START);
	} else
		QMetaObject::invokeMethod(manager, "disconnectDevice",
					  Qt::QueuedConnection);

	qDebug() << "start thread exit.\n";
	return NULL;
}

void *AOADeviceManager::heartbeatThread(void *d)
{
	AOADeviceManager *device = (AOADeviceManager *)d;

	while (device->m_continuousRead) {
		int len = 0;
		uint8_t data[1] = {2};
		device->m_usbMutex.lock();
		int r = libusb_bulk_transfer(device->m_droid.usbHandle,
					     device->m_droid.outendp, data, 1,
					     &len, 10);
		device->m_usbMutex.unlock();

		if (r == 0 || r == LIBUSB_ERROR_TIMEOUT) {
			device->m_timeoutMutex.lock();
			device->m_waitCondition.wait(&device->m_timeoutMutex,
						     100);
			device->m_timeoutMutex.unlock();
			continue;
		} else
			break;
	}

	qDebug() << "heartbeat thread exit.\n";
	return NULL;
}

int AOADeviceManager::connectDevice(libusb_device *device)
{
	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(device, &desc);
	if (r < 0) {
		return -1;
	}

	qDebug() << "usb device vid: " << QString::number(desc.idVendor, 16);
	switch (desc.bDeviceClass) {
	case 0x09:
		return -1;
	}

	r = setupDroid(device, &m_droid);
	if (r < 0) {
		qDebug("failed to setup droid: %d\n", r);
		return -3;
	}

	m_exitStart = false;
	m_startThread = std::thread(AOADeviceManager::startThread, this);

	return 0;
}

void AOADeviceManager::disconnectDevice()
{
	m_exitStart = true;
	if (m_startThread.joinable())
		m_startThread.join();

	stopUSBPipe();
	shutdownUSBDroid(&m_droid);
	clearUSB();

	qDebug("Android disconnected");

	send_status(m_client, MIRROR_STOP);
}

bool AOADeviceManager::startTask()
{
	clearUSB();

	bool ret = false;
	if (initUSB() < 0)
		return ret;

	m_devs_count = libusb_get_device_list(m_ctx, &m_devs);
	for (int i = 0; i < m_devs_count; i++) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(m_devs[i], &desc);
		if (r < 0) {
			continue;
		}

		if (isAOADevice(desc.idVendor, desc.idProduct)) {
			ret = connectDevice(m_devs[i]) == 0;
			break;
		}
	}

	return ret;
}

void AOADeviceManager::deferUpdateUsbInventory(bool isAdd)
{
	if (m_deviceChangeMutex.tryLock()) {
		QMetaObject::invokeMethod(this, "updateUsbInventory",
					  Qt::QueuedConnection,
					  Q_ARG(bool, isAdd));
		m_deviceChangeMutex.unlock();
	}
}

int AOADeviceManager::checkAndInstallDriver() // 1->检测到adb 2->没找到android设备 3->切到aoa模式失败 4->还没切换呢就已经在aoa模式了，需要提醒用户插拔
{
	int ret = -1;
	int switchAOACount = 0;
	bool sendSwitchCommand = false;

Retry:
	qDebug() << "start usb stask";
	auto path = m_androidDeviceInfo.devicePath;
	if (path.length() < 4)
		return ret;

	path = path.mid(4);
	path = path.left(path.lastIndexOf('#'));
	path.replace("#", "\\");

	if (m_driverHelper->checkInstall(m_androidDeviceInfo.vid, m_androidDeviceInfo.pid, path)) {
		qDebug() << "checkInstall wait device reconnect";
		m_waitMutex.lock();
		m_waitCondition.wait(&m_waitMutex, 2500);
		m_waitMutex.unlock();
		qDebug() << "checkInstall wait device reconnect finish";
		goto Retry;
	}
	if (isAOADevice(m_androidDeviceInfo.vid, m_androidDeviceInfo.pid)) {
		ret = sendSwitchCommand ? 0 : 4;
	} else {
		qDebug() << "switch command loop count: " << switchAOACount;
		switchAOACount++;
		if (switchAOACount > 3) {
			ret = 3;
		} else {
			sendSwitchCommand = true;
			switchDroidToAcc(m_androidDeviceInfo.vid, m_androidDeviceInfo.pid, 1);
			m_waitMutex.lock();
			m_waitCondition.wait(&m_waitMutex, 2500);
			m_waitMutex.unlock();
			if (enumDeviceAndCheck())
				goto Retry;
		}
	}

	return ret;
}

bool AOADeviceManager::enumDeviceAndCheck()
{
	bool ret = false;
	HDEVINFO devInfo = SetupDiGetClassDevs(&USB_DEVICE_GUID, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (!devInfo)
		return ret;

        SP_DEVINFO_DATA devData;
        devData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (int i = 0; ; i++)
        {
		SetupDiEnumDeviceInfo(devInfo, i, &devData);
		if (GetLastError() == ERROR_NO_MORE_ITEMS) break;

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
		if (deviceGUID.contains(GUID_MOUSE, Qt::CaseInsensitive) ||
		    deviceGUID.contains(GUID_KEYBOARD, Qt::CaseInsensitive) ||
		    deviceGUID.contains(GUID_HID, Qt::CaseInsensitive))
			continue;

		QString deviceHardwareId = getDeviceProperty(devInfo, &devData, SPDRP_HARDWAREID);
		if (deviceHardwareId.contains("root", Qt::CaseInsensitive) || deviceHardwareId.contains("hub", Qt::CaseInsensitive))
			continue;

		qDebug() << deviceHardwareId << getDeviceProperty(devInfo, &devData, SPDRP_COMPATIBLEIDS);

		int f1 = deviceHardwareId.indexOf('_');
		if (f1 < 0)
			continue;
		int f2 = deviceHardwareId.indexOf('_', f1 + 1);
		if (f2 < 0)
			continue;

		bool ok = false;
		int vid = deviceHardwareId.mid(f1 + 1, 4).toInt(&ok, 16);
		if (!ok)
			continue;

		int pid = deviceHardwareId.mid(f2 + 1, 4).toInt(&ok, 16);
		if (!ok)
			continue;

		if (!m_vids.contains(vid))
			continue;

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

		m_androidDeviceInfo.devicePath = QString::fromWCharArray(detailData->DevicePath);
		m_androidDeviceInfo.vid = vid;
		m_androidDeviceInfo.pid = pid;
		ret = true;
		free(detailData);
		break;
	    
        }
        SetupDiDestroyDeviceInfoList(devInfo);
	return ret;
}

void AOADeviceManager::updateUsbInventory(bool isDeviceAdd)
{
	QMutexLocker locker(&m_deviceChangeMutex);
	qDebug() << "enter updateUsbInventory";
	bool doLost = true;
	if (isDeviceAdd && !m_droid.connected) {
		bool exist = enumDeviceAndCheck();
		if (exist) {
			int ret = checkAndInstallDriver();
			if (ret == 0)
				startTask();
			else if (ret == 3)
				emit infoPrompt(u8"启动投屏服务失败，后台退出鱼耳直播APP，插拔手机后再试。");
			else if (ret == 4)
				emit infoPrompt(u8"启动投屏服务失败，请插拔手机后再试。");
			else
				qDebug() << "checkAndInstallDriver return : " << ret;
		}

		if (exist)
			doLost = false;
	}

	if (doLost)
		emit deviceLost();

	qDebug() << "leave updateUsbInventory";
}
