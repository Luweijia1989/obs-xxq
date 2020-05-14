#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-dshow", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows DirectShow source/encoder";
}

extern bool g_st_checkpass;
extern void RegisterDShowSource();
extern void RegisterDShowEncoders();
extern int InitGlfw();
extern void UninitGlfw();
extern int check_license();

bool obs_module_load(void)
{
	RegisterDShowSource();
	RegisterDShowEncoders();

	g_st_checkpass = check_license() == 0;
	if (g_st_checkpass) {
		InitGlfw();
	}

	return true;
}

void obs_module_unload(void)
{
	UninitGlfw();
}
