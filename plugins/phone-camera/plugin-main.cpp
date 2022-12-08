#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("phone-camera", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "phone camera sources";
}

extern void RegisterPhoneCamera();
bool obs_module_load(void)
{
	RegisterPhoneCamera();
	return true;
}

void obs_module_unload()
{
	blog(LOG_DEBUG, "phone camera module unloaded.");
}
