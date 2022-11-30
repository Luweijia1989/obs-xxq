#include <obs-module.h>
#include <thread>
#include <qmetatype.h>
#include <qmap.h>
#include <qstring>

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
bool obs_module_load(void)
{
	qRegisterMetaType<QMap<QString, QString>>("QMap<QString, QString>");

	usbmuxd_th = std::thread([] { usbmuxd_process(); });

	RegisterPhoneCamera();
	return true;
}

void obs_module_unload()
{
	should_exit = 1;
	if (usbmuxd_th.joinable())
		usbmuxd_th.join();

	blog(LOG_DEBUG, "phone camera module unloaded.");
}
