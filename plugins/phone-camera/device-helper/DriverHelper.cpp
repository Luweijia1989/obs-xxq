#include "DriverHelper.h"
#include "msapi_utf8.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QRunnable>
#include <QThreadPool>
#include <libusb-1.0/libusb.h>

#define VID_GOOGLE 0x18D1
#define PID_AOA_ACC 0x2D00
#define PID_AOA_ACC_ADB 0x2D01
#define PID_AOA_AU 0x2D02
#define PID_AOA_AU_ADB 0x2D03
#define PID_AOA_ACC_AU 0x2D04
#define PID_AOA_ACC_AU_ADB 0x2D05
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

#define safe_free(p)                     \
	do {                             \
		if ((void *)p != NULL) { \
			free((void *)p); \
			p = NULL;        \
		}                        \
	} while (0)

char *to_valid_filename(char *name, const char *ext)
{
	size_t i, j, k;
	BOOL found;
	char *ret;
	wchar_t unauthorized[] = L"\x0001\x0002\x0003\x0004\x0005\x0006\x0007\x0008\x000a"
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

DriverHelper::DriverHelper(QObject *parent) : QObject(parent)
{
	temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/yuerlive";
	temp_dir = QDir::toNativeSeparators(temp_dir);
	QDir d(temp_dir);
	if (!d.exists())
		d.mkdir(temp_dir);
}

void DriverHelper::checkInstall(bool isAppleDevice, int vid, int pid, QString targetDevicePath)
{
	if (targetDevicePath.length() < 4) {
		emit complete(false);
		return;
	}

	targetDevicePath = targetDevicePath.mid(4);
	targetDevicePath = targetDevicePath.left(targetDevicePath.lastIndexOf('#'));
	targetDevicePath.replace("#", "\\");

	if (isAppleDevice || !isAOADevice(vid, pid))
		emit changeTip(u8"正在安装驱动");
	else
		emit changeTip(u8"正在安装驱动(第二阶段)");

	int status  = 4;

	wdi_is_driver_supported(isAppleDevice ? WDI_LIBUSB0 : WDI_LIBUSBK, &file_info);

	target_driver_version = file_info.dwFileVersionMS;
	target_driver_version <<= 32;
	target_driver_version += file_info.dwFileVersionLS;
	pd_options.driver_type = isAppleDevice ? WDI_LIBUSB0 : WDI_LIBUSBK;

	wdi_device_info *list = nullptr;
	wdi_options_create_list option = {true, true, false};
	wdi_create_list(&list, &option);

	QList<wdi_device_info *> targetDevs;
	for (wdi_device_info *dev = list; dev != NULL; dev = dev->next) {
		if (dev->vid == vid && dev->pid == pid)
			targetDevs.append(dev);
	}

	wdi_device_info *targetDev = nullptr;
	if (targetDevs.size() == 1)
		targetDev = targetDevs.front();
	else if (targetDevs.size() > 1) {
		auto iter = targetDevs.begin();
		for (; iter != targetDevs.end(); iter++) {
			auto dev = *iter;
			if (!isAppleDevice && isAOADevice(dev->vid, dev->pid) && dev->is_composite)
				continue;
			QString path(dev->device_id);
			if (!path.isEmpty() && targetDevicePath.contains(path, Qt::CaseInsensitive)) {
				targetDev = dev;
				break;
			}
		}

		if (iter == targetDevs.end())
			targetDev = targetDevs.front();
	}

	if (targetDev) {
		QString driverName(targetDev->driver);
		if (!driverName.startsWith(isAppleDevice ? "libusb0" : "libusbK")) {
			emit installProgress(0);
			status = install(targetDev) ? 1 : 2;
		} else {
			status = 3;
		}
	}

	wdi_destroy_list(list);

	emit complete(status);
}

bool deleteDir(const QString &path)
{
	if (path.isEmpty()) {
		return false;
	}

	QDir dir(path);
	if (!dir.exists()) {
		return true;
	}

	dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
	QFileInfoList fileList = dir.entryInfoList();
	foreach(QFileInfo file, fileList)
	{
		if (file.isFile()) {
			file.dir().remove(file.fileName());
		} else {
			deleteDir(file.absoluteFilePath());
		}
	}

	return dir.rmpath(dir.absolutePath());
}

bool DriverHelper::install(wdi_device_info *dev)
{
Retry:
	QString dir = QString("%1\\%2").arg(temp_dir).arg(dev->pid);
	deleteDir(dir);
	bool success = false;
	char *inf_name = to_valid_filename(dev->desc, ".inf");
	if (inf_name != NULL) {
		qDebug() << "use info name: " << inf_name;
		emit installProgress(1);
		int res = wdi_prepare_driver(dev, dir.toStdString().c_str(), inf_name, &pd_options);
		emit installProgress(2);
		if (res == WDI_SUCCESS) {
			qDebug() << "Successfully extracted driver files.";
			res = wdi_install_driver(dev, dir.toStdString().c_str(), inf_name, &id_options);
			if (res == WDI_SUCCESS) {
				emit installProgress(3);
				qDebug() << "Successfully install the driver";
				success = true;
			} else {
				qDebug() << "wdi_install_driver error: " << res;

				if (res == WDI_ERROR_PENDING_INSTALLATION)
					goto Retry;
			}
		} else {
			qDebug() << "wdi_prepare_driver error: " << res;
		}
	}

	safe_free(inf_name);
	if (!success)
		emit installError(u8"驱动安装失败，正在重试，请耐心等待。。。");

	return success;
}
