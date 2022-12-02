#include <obs-module.h>
#include <thread>
#include <qmetatype.h>
#include <qmap.h>
#include <qstring>
#include <qset.h>
#include "driver-helper.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("phone-camera", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "phone camera sources";
}

void RegisterPhoneCamera();

extern "C" {
extern int usbmuxd_process();
extern int should_exit;
}

std::thread usbmuxd_th;

QSet<QString> installingDevices;
QSet<QString> runningDevices;
DriverHelper *driverHelper = nullptr;

bool obs_module_load(void)
{
	qRegisterMetaType<QMap<QString, QString>>("QMap<QString, QPair<QString, uint32_t>>");

	driverHelper = new DriverHelper();

	usbmuxd_th = std::thread([] { usbmuxd_process(); });

	RegisterPhoneCamera();
	return true;
}

void obs_module_unload()
{
	should_exit = 1;
	if (usbmuxd_th.joinable())
		usbmuxd_th.join();

	delete driverHelper;

	blog(LOG_DEBUG, "phone camera module unloaded.");
}
