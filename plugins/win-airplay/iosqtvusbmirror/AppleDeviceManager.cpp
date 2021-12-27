#include "AppleDeviceManager.h"
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include "../ipc.h"
#include "../common-define.h"
#include <winusb.h>
#include <iostream>
#include <Setupapi.h>
#include <Devpkey.h>
#include <QElapsedTimer>
#include "MirrorManager.h"

#define APPLE_VID 0x05AC
#define PID_RANGE_LOW 0x1290
#define PID_RANGE_MAX 0x12af

GUID USB_DEVICE_GUID = {0xA5DCBF10,
			0x6530,
			0x11D2,
			{0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};

ReadStdinThread::ReadStdinThread(QObject *parent)
	: QThread(parent)
{

}

void ReadStdinThread::run()
{
	uint8_t buf[1024] = {0};
	while (true) {
		int read_len = fread(buf, 1, 1024, stdin); // read 0 means parent has been stopped
		if (!read_len || buf[0] == 1) {
			break;
		}
	}
	emit quit();
	qDebug() << "ReadStdinThread stopped.";
}
			     
AppleDeviceManager::AppleDeviceManager()
{
	m_mirrorManager = new MirrorManager;
	m_mirrorManager->setParent(this);
	m_driverHelper = new DriverHelper(this);
	connect(m_driverHelper, &DriverHelper::installProgress, this,
		&AppleDeviceManager::installProgress);
	connect(m_driverHelper, &DriverHelper::installError, this,
		&AppleDeviceManager::installError);

	m_readStdinThread = new ReadStdinThread(this);
	connect(m_readStdinThread, &ReadStdinThread::quit, this, [=](){
		deleteLater();
	}, Qt::QueuedConnection);
	m_readStdinThread->start();

	connect(this, &QObject::destroyed, qApp, &QApplication::quit);
}

AppleDeviceManager::~AppleDeviceManager()
{
	
}

void AppleDeviceManager::deferUpdateUsbInventory(bool isAdd)
{
	if (isAdd)
		signalWait();

	if (m_deviceChangeMutex.tryLock()) {
		m_deviceChangeMutex.unlock();
		QMetaObject::invokeMethod(this, "updateUsbInventory", Qt::QueuedConnection, Q_ARG(bool, isAdd));
	}
}

int AppleDeviceManager::checkAndInstallDriver() 
{
Retry:
	auto path = m_appleDeviceInfo.devicePath;
	if (path.length() < 4)
		return -1;

	path = path.mid(4);
	path = path.left(path.lastIndexOf('#'));
	path.replace("#", "\\");

	//0->一切ok 1->需要安装 -1->设备不见了
	int ret = m_driverHelper->checkInstall(m_appleDeviceInfo.vid, m_appleDeviceInfo.pid, path);
	if (ret == 1) {
		qDebug() << "checkInstall wait device reconnect";
		m_waitMutex.lock();
		m_waitCondition.wait(&m_waitMutex, 2500);
		m_waitMutex.unlock();
		qDebug() << "checkInstall wait device reconnect finish";
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
			if (ret != 0)
				emit infoPrompt(u8"设备异常，请插拔设备后再试");
			else
				startTask();
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

	m_mirrorManager->startMirrorTask(m_appleDeviceInfo.vid, m_appleDeviceInfo.pid);
}
