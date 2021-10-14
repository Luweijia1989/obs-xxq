#include "DriverHelper.h"
#include "msapi_utf8.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QRunnable>
#include <QThreadPool>
#include "libusb.h"

const int GOOGLE_VID = 0x18D1;
const int GOOGLE_PID = 0x2D04;

bool isAOADevice(int vid, int pid)
{
	return vid == GOOGLE_VID && pid == GOOGLE_PID;
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

DriverHelper::DriverHelper(QObject *parent) : QObject(parent)
{
	temp_dir =
		QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
		"/yuerlive";
	temp_dir = QDir::toNativeSeparators(temp_dir);
	QDir d(temp_dir);
	if (!d.exists())
		d.mkdir(temp_dir);

	wdi_is_driver_supported(WDI_LIBUSBK, &file_info);

	target_driver_version = file_info.dwFileVersionMS;
	target_driver_version <<= 32;
	target_driver_version += file_info.dwFileVersionLS;
	pd_options.driver_type = WDI_LIBUSBK;
}

bool DriverHelper::checkInstall(int vid, int pid, QString targetDevicePath, void (*func)(libusb_context **, libusb_device ***, int), libusb_context **c, libusb_device ***d, int ct)
{
	int ret = false;

	if (inInstall)
		return ret;

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
			if (isAOADevice(dev->vid, dev->pid) && dev->is_composite)
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

	QString driverName(targetDev->driver);
	if (!driverName.startsWith("libusbK"))
		ret = true;
	else {
		if(isAOADevice(targetDev->vid, targetDev->pid))
			emit installProgress(1, 3);
	}

	if (ret) {
		inInstall = true;
		emit installProgress(!isAOADevice(targetDev->vid, targetDev->pid) ? 0 : 1, 0);

		func(c, d, ct);
		install(targetDev);
	}

	wdi_destroy_list(list);

	return ret;
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

void DriverHelper::install(wdi_device_info *dev)
{
	int res = -1;
Retry:
	QString dir = QString("%1\\%2").arg(temp_dir).arg(dev->pid);
	deleteDir(dir);
	int step = !isAOADevice(dev->vid, dev->pid) ? 0 : 1;
	bool success = false;
	char *inf_name = to_valid_filename(dev->desc, ".inf");
	if (inf_name != NULL) {
		qDebug() << "use info name: " << inf_name;
		emit installProgress(step, 1);
		int res = wdi_prepare_driver(dev, dir.toStdString().c_str(),
					     inf_name, &pd_options);
		emit installProgress(step, 2);
		if (res == WDI_SUCCESS) {
			qDebug() << "Successfully extracted driver files.";
			res = wdi_install_driver(dev, dir.toStdString().c_str(),
						 inf_name, &id_options);
			if (res == WDI_SUCCESS) {
				emit installProgress(step, 3);
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

	inInstall = false;
	safe_free(inf_name);
	if(!success)
		emit installError(u8"驱动安装失败，正在重试，请耐心等待。。。");
}
