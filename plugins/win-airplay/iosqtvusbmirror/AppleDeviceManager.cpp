#include "AppleDeviceManager.h"
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include "../ipc.h"
#include "../common-define.h"
#include <winusb.h>
#include <Setupapi.h>
#include <Devpkey.h>
#include "MirrorManager.h"

#define APPLE_VID 0x05AC
#define PID_RANGE_LOW 0x1290
#define PID_RANGE_MAX 0x12af

GUID USB_DEVICE_GUID = {0xA5DCBF10,
			0x6530,
			0x11D2,
			{0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
			     
AppleDeviceManager::AppleDeviceManager()
{
	m_mirrorManager = new MirrorManager;
	m_driverHelper = new DriverHelper(this);
	connect(m_driverHelper, &DriverHelper::installProgress, this,
		&AppleDeviceManager::installProgress);
	connect(m_driverHelper, &DriverHelper::installError, this,
		&AppleDeviceManager::installError);
}

AppleDeviceManager::~AppleDeviceManager()
{
	delete m_mirrorManager;
}

void AppleDeviceManager::deferUpdateUsbInventory(bool isAdd)
{
	if (m_deviceChangeMutex.tryLock()) {
		QMetaObject::invokeMethod(this, "updateUsbInventory",
					  Qt::QueuedConnection,
					  Q_ARG(bool, isAdd));
		m_deviceChangeMutex.unlock();
	}
}

int AppleDeviceManager::checkAndInstallDriver() 
{
	int switchAOACount = 0;
	bool sendSwitchCommand = false;

Retry:
	auto path = m_appleDeviceInfo.devicePath;
	if (path.length() < 4)
		return -2;

	path = path.mid(4);
	path = path.left(path.lastIndexOf('#'));
	path.replace("#", "\\");

	if (m_driverHelper->checkInstall(m_appleDeviceInfo.vid, m_appleDeviceInfo.pid, path)) {
		qDebug() << "checkInstall wait device reconnect";
		m_waitMutex.lock();
		m_waitCondition.wait(&m_waitMutex, 2500);
		m_waitMutex.unlock();
		qDebug() << "checkInstall wait device reconnect finish";
		goto Retry;
	}

	int ret = m_mirrorManager->checkAndChangeMode(m_appleDeviceInfo.vid, m_appleDeviceInfo.pid);
	if (ret == 0) {
		qDebug() << "checkAndChangeMode wait device reconnect";
		m_waitMutex.lock();
		m_waitCondition.wait(&m_waitMutex, 2500);
		m_waitMutex.unlock();
		qDebug() << "checkAndChangeMode wait device reconnect finish";
		goto Retry;
	}

	return ret;
}

bool isAppleDevice(int vid, int pid)
{
	return vid == APPLE_VID && pid > PID_RANGE_LOW && pid < PID_RANGE_MAX;
}

bool AppleDeviceManager::enumDeviceAndCheck()
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

		QString deviceHardwareId = getDeviceProperty(devInfo, &devData, SPDRP_HARDWAREID);
		if (deviceHardwareId.contains("root", Qt::CaseInsensitive) || deviceHardwareId.contains("hub", Qt::CaseInsensitive))
			continue;

		qDebug() << deviceHardwareId;

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

		if (!isAppleDevice(vid, pid)) {
			continue;
		}

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

		m_appleDeviceInfo.devicePath = QString::fromWCharArray(detailData->DevicePath);
		m_appleDeviceInfo.vid = vid;
		m_appleDeviceInfo.pid = pid;
		ret = true;
		free(detailData);
		break;
	    
        }
        SetupDiDestroyDeviceInfoList(devInfo);
	return ret;
}

void AppleDeviceManager::updateUsbInventory(bool isDeviceAdd)
{
	QMutexLocker locker(&m_deviceChangeMutex);
	qDebug() << "enter updateUsbInventory";
	bool doLost = true;
	if (isDeviceAdd && !m_inScreenMirror) {
		bool exist = enumDeviceAndCheck();
		if (exist) {
			int ret = checkAndInstallDriver();
			if (ret == 1)
				startTask();
			else if (ret == -1)
				emit infoPrompt(u8"打开苹果设备失败。请重启电脑后再试");
			else if (ret == -2)
				emit infoPrompt(u8"无效的设备路径，请插拔设备后再试");
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


void AppleDeviceManager::startTask()
{
	qDebug() << "start usb stask";
}
