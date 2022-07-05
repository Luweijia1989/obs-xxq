#include <windows.h>
#include <obs-module.h>
#include <util/windows/win-version.h>
#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("wasapi-capture", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows wasapi capture";
}

extern struct obs_source_info wasapi_capture_info;

static HANDLE init_hooks_thread = NULL;

/* temporary, will eventually be erased once we figure out how to create both
 * 32bit and 64bit versions of the helpers/hook */
#ifdef _WIN64
#define IS32BIT false
#else
#define IS32BIT true
#endif

bool obs_module_load(void)
{
	obs_register_source(&wasapi_capture_info);
	return true;
}

void obs_module_unload(void)
{
	
}
