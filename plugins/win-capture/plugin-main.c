#include <windows.h>
#include <obs-module.h>
#include <util/windows/win-version.h>
#include <util/platform.h>
#define COBJMACROS
#include <dxgi.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-capture", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows game/screen/window capture";
}

extern struct obs_source_info duplicator_capture_info;
extern struct obs_source_info monitor_capture_info;
extern struct obs_source_info window_capture_info;
extern struct obs_source_info game_capture_info;
extern struct obs_source_info lyric_capture_info;

static HANDLE init_hooks_thread = NULL;

extern bool cached_versions_match(void);
extern bool load_cached_graphics_offsets(bool is32bit, const char *config_path);
extern bool load_graphics_offsets(bool is32bit, const char *config_path);

/* temporary, will eventually be erased once we figure out how to create both
 * 32bit and 64bit versions of the helpers/hook */
#ifdef _WIN64
#define IS32BIT false
#else
#define IS32BIT true
#endif

/* note, need to enable cache writing in load-graphics-offsets.c if you turn
 * this back on*/
#define USE_HOOK_ADDRESS_CACHE false

static DWORD WINAPI init_hooks(LPVOID param)
{
	char *config_path = param;

	if (USE_HOOK_ADDRESS_CACHE && cached_versions_match() &&
	    load_cached_graphics_offsets(IS32BIT, config_path)) {

		load_cached_graphics_offsets(!IS32BIT, config_path);
		obs_register_source(&game_capture_info);
		obs_register_source(&lyric_capture_info);

	} else if (load_graphics_offsets(IS32BIT, config_path)) {
		load_graphics_offsets(!IS32BIT, config_path);
	}

	bfree(config_path);
	return 0;
}

#define MICROSOFT_VENDOR_ID 5140
bool has_multi_video_adapter()
{
	IDXGIFactory1 *pIDXGIFactory = NULL;
	if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1,
				      (void **)(&pIDXGIFactory))))
		return false;
	int i = 0;
	int adapterCount = 0;
	IDXGIAdapter1 *pAdapter;
	while (IDXGIFactory1_EnumAdapters1(pIDXGIFactory, i, &pAdapter) !=
	       DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC1 desc;
		if (SUCCEEDED(IDXGIAdapter1_GetDesc1(pAdapter, &desc)) &&
		    desc.VendorId != MICROSOFT_VENDOR_ID) {
			adapterCount++;
		}
		++i;
	}

	if (pIDXGIFactory)
		IDXGIFactory1_Release(pIDXGIFactory);

	return adapterCount > 1;
}

void wait_for_hook_initialization(void)
{
	static bool initialized = false;

	if (!initialized) {
		if (init_hooks_thread) {
			WaitForSingleObject(init_hooks_thread, INFINITE);
			CloseHandle(init_hooks_thread);
			init_hooks_thread = NULL;
		}
		initialized = true;
	}
}

void init_hook_files(void);

bool graphics_uses_d3d11 = false;
bool wgc_supported = false;
bool win8_or_above = false;

bool obs_module_load(void)
{
	struct win_version_info ver;
	char *config_dir;

	struct win_version_info win1903 = {
		.major = 10, .minor = 0, .build = 18362, .revis = 0};

	config_dir = obs_module_config_path(NULL);
	if (config_dir) {
		os_mkdirs(config_dir);
		bfree(config_dir);
	}

	get_win_ver(&ver);

	win8_or_above = ver.major > 6 || (ver.major == 6 && ver.minor >= 2);

	obs_enter_graphics();
	graphics_uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
	obs_leave_graphics();

	if (graphics_uses_d3d11)
		wgc_supported = win_version_compare(&ver, &win1903) >= 0;

	if (win8_or_above && graphics_uses_d3d11)
		obs_register_source(&duplicator_capture_info);

	obs_register_source(&monitor_capture_info);
	obs_register_source(&window_capture_info);

	char *config_path = obs_module_config_path(NULL);

	init_hooks_thread =
		CreateThread(NULL, 0, init_hooks, config_path, 0, NULL);
	obs_register_source(&game_capture_info);
	obs_register_source(&lyric_capture_info);

	return true;
}

void obs_module_unload(void)
{
	wait_for_hook_initialization();
}
