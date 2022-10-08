#include <inttypes.h>
#include <obs-module.h>
#include <obs-hotkey.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/dstr.h>
#include <util/util_uint64.h>
#include <windows.h>
#include <dxgi.h>
#include <emmintrin.h>
#include <ipc-util/pipe.h>
#include "nt-stuff.h"
#include "wasapi-hook-info.h"
#include "window-helpers.h"
#include "app-helpers.h"
#include "obfuscate.h"

#define do_log(level, format, ...) blog(level, "[wasapi-capture: '%s'] " format, obs_source_get_name(gc->source), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

#define SETTING_CAPTURE_WINDOW "window"

#define DEFAULT_RETRY_INTERVAL 2.0f
#define ERROR_RETRY_INTERVAL 4.0f

struct wasapi_capture_config {
	char *title;
	char *class;
	char *executable;
};

struct wasapi_capture {
	obs_source_t *source;
	struct wasapi_capture_config config;

	HANDLE injector_process;

	uint32_t channels;
	uint32_t samplerate;
	uint32_t byte_persample;

	DWORD process_id;
	DWORD thread_id;
	HWND next_window;
	HWND window;
	float retry_time;
	float retry_interval;

	struct dstr title;
	struct dstr class;
	struct dstr executable;

	enum window_priority priority;
	bool wait_for_target_startup;
	bool active;
	bool capturing;
	bool activate_hook;
	bool process_is_64bit;
	bool error_acquiring;
	bool dwm_capture;
	bool initial_config;
	bool convert_16bit;
	bool is_app;
	bool cursor_hidden;

	ipc_pipe_server_t pipe;
	struct hook_info *global_hook_info;
	HANDLE keepalive_mutex;
	HANDLE hook_init;
	HANDLE hook_restart;
	HANDLE hook_stop;
	HANDLE hook_ready;
	HANDLE hook_exit;
	HANDLE hook_data_map;
	HANDLE global_hook_info_map;
	HANDLE target_process;
	HANDLE audio_data_mutex;
	wchar_t *app_sid;
	int retrying;

	union {
		struct {
			struct shmem_data *shmem_data;
			uint8_t *audio_data_buffer;
		};

		void *data;
	};
};

struct wasapi_offset offsets32 = {0};
struct wasapi_offset offsets64 = {0};

static inline HANDLE open_mutex_plus_id(struct wasapi_capture *gc, const wchar_t *name, DWORD id)
{
	wchar_t new_name[64];
	_snwprintf(new_name, 64, L"%s%lu", name, id);
	return gc->is_app ? open_app_mutex(gc->app_sid, new_name) : open_mutex(new_name);
}

static inline HANDLE open_mutex_gc(struct wasapi_capture *gc, const wchar_t *name)
{
	return open_mutex_plus_id(gc, name, gc->process_id);
}

static inline HANDLE open_event_plus_id(struct wasapi_capture *gc, const wchar_t *name, DWORD id)
{
	wchar_t new_name[64];
	_snwprintf(new_name, 64, L"%s%lu", name, id);
	return gc->is_app ? open_app_event(gc->app_sid, new_name) : open_event(new_name);
}

static inline HANDLE open_event_gc(struct wasapi_capture *gc, const wchar_t *name)
{
	return open_event_plus_id(gc, name, gc->process_id);
}

static inline HANDLE open_map_plus_id(struct wasapi_capture *gc, const wchar_t *name, DWORD id)
{
	wchar_t new_name[64];
	_snwprintf(new_name, 64, L"%s%lu", name, id);

	debug("map id: %S", new_name);

	return gc->is_app ? open_app_map(gc->app_sid, new_name) : OpenFileMappingW(GC_MAPPING_FLAGS, false, new_name);
}

static inline HANDLE open_hook_info(struct wasapi_capture *gc)
{
	return open_map_plus_id(gc, SHMEM_HOOK_INFO, gc->process_id);
}

static void close_handle(HANDLE *p_handle)
{
	HANDLE handle = *p_handle;
	if (handle) {
		if (handle != INVALID_HANDLE_VALUE)
			CloseHandle(handle);
		*p_handle = NULL;
	}
}

static inline HMODULE kernel32(void)
{
	static HMODULE kernel32_handle = NULL;
	if (!kernel32_handle)
		kernel32_handle = GetModuleHandleW(L"kernel32");
	return kernel32_handle;
}

static inline HANDLE open_process(DWORD desired_access, bool inherit_handle, DWORD process_id)
{
	static HANDLE(WINAPI * open_process_proc)(DWORD, BOOL, DWORD) = NULL;
	if (!open_process_proc)
		open_process_proc = get_obfuscated_func(kernel32(), "NuagUykjcxr", 0x1B694B59451ULL);

	return open_process_proc(desired_access, inherit_handle, process_id);
}

static void stop_capture(struct wasapi_capture *gc)
{
	info("stop capture called");
	ipc_pipe_server_free(&gc->pipe);

	if (gc->hook_stop) {
		info("set hook stop event");
		SetEvent(gc->hook_stop);
	}
	if (gc->global_hook_info) {
		UnmapViewOfFile(gc->global_hook_info);
		gc->global_hook_info = NULL;
	}
	if (gc->data) {
		UnmapViewOfFile(gc->data);
		gc->data = NULL;
	}

	if (gc->app_sid) {
		LocalFree(gc->app_sid);
		gc->app_sid = NULL;
	}

	close_handle(&gc->hook_restart);
	close_handle(&gc->hook_stop);
	close_handle(&gc->hook_ready);
	close_handle(&gc->hook_exit);
	close_handle(&gc->hook_init);
	close_handle(&gc->hook_data_map);
	close_handle(&gc->keepalive_mutex);
	close_handle(&gc->global_hook_info_map);
	close_handle(&gc->target_process);
	close_handle(&gc->audio_data_mutex);

	if (gc->active)
		info("capture stopped");

	gc->wait_for_target_startup = false;
	gc->active = false;
	gc->capturing = false;

	if (gc->retrying)
		gc->retrying--;
}

static inline void free_config(struct wasapi_capture_config *config)
{
	bfree(config->title);
	bfree(config->class);
	bfree(config->executable);
	memset(config, 0, sizeof(*config));
}

static void wasapi_capture_destroy(void *data)
{
	struct wasapi_capture *gc = data;
	stop_capture(gc);

	dstr_free(&gc->title);
	dstr_free(&gc->class);
	dstr_free(&gc->executable);
	free_config(&gc->config);
	bfree(gc);
}

static inline void get_config(struct wasapi_capture_config *cfg, obs_data_t *settings, const char *window)
{
	build_window_strings(window, &cfg->class, &cfg->title, &cfg->executable);
}

static inline int s_cmp(const char *str1, const char *str2)
{
	if (!str1 || !str2)
		return -1;

	return strcmp(str1, str2);
}

static inline bool capture_needs_reset(struct wasapi_capture_config *cfg1, struct wasapi_capture_config *cfg2)
{

	if (s_cmp(cfg1->class, cfg2->class) != 0 || s_cmp(cfg1->title, cfg2->title) != 0 || s_cmp(cfg1->executable, cfg2->executable) != 0)
		return true;

	return false;
}

static void wasapi_capture_update(void *data, obs_data_t *settings)
{
	struct wasapi_capture *gc = data;
	struct wasapi_capture_config cfg;
	bool reset_capture = false;
	const char *window = obs_data_get_string(settings, SETTING_CAPTURE_WINDOW);

	get_config(&cfg, settings, window);
	reset_capture = capture_needs_reset(&cfg, &gc->config);

	gc->error_acquiring = false;
	gc->activate_hook = !!window && !!*window;

	free_config(&gc->config);
	gc->config = cfg;
	gc->retry_interval = DEFAULT_RETRY_INTERVAL;
	gc->wait_for_target_startup = false;

	dstr_free(&gc->title);
	dstr_free(&gc->class);
	dstr_free(&gc->executable);

	dstr_copy(&gc->title, gc->config.title);
	dstr_copy(&gc->class, gc->config.class);
	dstr_copy(&gc->executable, gc->config.executable);

	if (!gc->initial_config) {
		if (reset_capture) {
			stop_capture(gc);
		}
	} else {
		gc->initial_config = false;
	}
}

extern void wait_for_hook_initialization(void);
static void *wasapi_capture_create(obs_data_t *settings, obs_source_t *source)
{
	wait_for_hook_initialization();

	struct wasapi_capture *gc = bzalloc(sizeof(*gc));
	gc->source = source;
	gc->initial_config = true;
	gc->retry_interval = DEFAULT_RETRY_INTERVAL;

	wasapi_capture_update(gc, settings);
	return gc;
}

#define STOP_BEING_BAD                                                      \
	"  This is most likely due to security software. Please make sure " \
	"that the OBS installation folder is excluded/ignored in the "      \
	"settings of the security software you are using."

static bool check_file_integrity(struct wasapi_capture *gc, const char *file, const char *name)
{
	DWORD error;
	HANDLE handle;
	wchar_t *w_file = NULL;

	if (!file || !*file) {
		warn("Game capture %s not found." STOP_BEING_BAD, name);
		return false;
	}

	if (!os_utf8_to_wcs_ptr(file, 0, &w_file)) {
		warn("Could not convert file name to wide string");
		return false;
	}

	handle = CreateFileW(w_file, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	bfree(w_file);

	if (handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
		return true;
	}

	error = GetLastError();
	if (error == ERROR_FILE_NOT_FOUND) {
		warn("Game capture file '%s' not found." STOP_BEING_BAD, file);
	} else if (error == ERROR_ACCESS_DENIED) {
		warn("Game capture file '%s' could not be loaded." STOP_BEING_BAD, file);
	} else {
		warn("Game capture file '%s' could not be loaded: %lu." STOP_BEING_BAD, file, error);
	}

	return false;
}

static inline bool is_64bit_windows(void)
{
#ifdef _WIN64
	return true;
#else
	BOOL x86 = false;
	bool success = !!IsWow64Process(GetCurrentProcess(), &x86);
	return success && !!x86;
#endif
}

static inline bool is_64bit_process(HANDLE process)
{
	BOOL x86 = true;
	if (is_64bit_windows()) {
		bool success = !!IsWow64Process(process, &x86);
		if (!success) {
			return false;
		}
	}

	return !x86;
}

static inline bool open_target_process(struct wasapi_capture *gc)
{
	gc->target_process = open_process(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, false, gc->process_id);
	if (!gc->target_process) {
		warn("could not open process: %s", gc->config.executable);
		return false;
	}

	gc->process_is_64bit = is_64bit_process(gc->target_process);
	gc->is_app = is_app(gc->target_process);
	if (gc->is_app) {
		gc->app_sid = get_app_sid(gc->target_process);
	}
	return true;
}

static inline bool init_keepalive(struct wasapi_capture *gc)
{
	wchar_t new_name[64];
	_snwprintf(new_name, 64, L"%s%lu", WINDOW_HOOK_KEEPALIVE, gc->process_id);

	gc->keepalive_mutex = CreateMutexW(NULL, false, new_name);
	if (!gc->keepalive_mutex) {
		warn("Failed to create keepalive mutex: %lu", GetLastError());
		return false;
	}

	return true;
}

static inline bool init_audio_data_mutexes(struct wasapi_capture *gc)
{
	gc->audio_data_mutex = open_mutex_gc(gc, AUDIO_DATA_MUTEX);

	if (!gc->audio_data_mutex) {
		DWORD error = GetLastError();
		if (error == 2) {
			if (!gc->retrying) {
				gc->retrying = 2;
				info("hook not loaded yet, retrying..");
			}
		} else {
			warn("failed to open audio data mutexes: %lu", GetLastError());
		}
		return false;
	}

	return true;
}

/* if there's already a hook in the process, then signal and start */
static inline bool attempt_existing_hook(struct wasapi_capture *gc)
{
	gc->hook_restart = open_event_gc(gc, EVENT_CAPTURE_RESTART);
	if (gc->hook_restart) {
		debug("existing hook found, signaling process: %s", gc->config.executable);
		SetEvent(gc->hook_restart);
		return true;
	}

	return false;
}

static inline bool init_hook_info(struct wasapi_capture *gc)
{
	gc->global_hook_info_map = open_hook_info(gc);
	if (!gc->global_hook_info_map) {
		warn("init_hook_info: get_hook_info failed: %lu", GetLastError());
		return false;
	}

	gc->global_hook_info = MapViewOfFile(gc->global_hook_info_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*gc->global_hook_info));
	gc->global_hook_info->offset = gc->process_is_64bit ? offsets64 : offsets32;
	if (!gc->global_hook_info) {
		warn("init_hook_info: failed to map data view: %lu", GetLastError());
		return false;
	}

	return true;
}

static void pipe_log(void *param, uint8_t *data, size_t size)
{
	struct wasapi_capture *gc = param;
	if (data && size)
		info("%s", data);
}

static inline bool init_pipe(struct wasapi_capture *gc)
{
	char name[64];
	sprintf(name, "%s%lu", PIPE_NAME, gc->process_id);

	if (!ipc_pipe_server_start(&gc->pipe, name, pipe_log, gc)) {
		warn("init_pipe: failed to start pipe, %s", name);
		return false;
	}

	return true;
}

static inline bool create_inject_process(struct wasapi_capture *gc, const char *inject_path, const char *hook_dll)
{
	wchar_t *command_line_w = malloc(4096 * sizeof(wchar_t));
	wchar_t *inject_path_w;
	wchar_t *hook_dll_w;
	PROCESS_INFORMATION pi = {0};
	STARTUPINFO si = {0};
	bool success = false;

	os_utf8_to_wcs_ptr(inject_path, 0, &inject_path_w);
	os_utf8_to_wcs_ptr(hook_dll, 0, &hook_dll_w);

	si.cb = sizeof(si);

	swprintf(command_line_w, 4096, L"\"%s\" \"%s\" %lu %lu", inject_path_w, hook_dll_w, 1, gc->thread_id);

	success = !!CreateProcessW(inject_path_w, command_line_w, NULL, NULL, false, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	if (success) {
		CloseHandle(pi.hThread);
		gc->injector_process = pi.hProcess;
	} else {
		warn("Failed to create inject helper process: %lu", GetLastError());
	}

	free(command_line_w);
	bfree(inject_path_w);
	bfree(hook_dll_w);
	return success;
}

static inline bool inject_hook(struct wasapi_capture *gc)
{
	bool success = false;
	const char *hook_dll;
	char *inject_path;
	char *hook_path;

	if (gc->process_is_64bit) {
		hook_dll = "wasapi-hook64.dll";
		inject_path = obs_module_file("inject-helper64.exe");
	} else {
		hook_dll = "wasapi-hook32.dll";
		inject_path = obs_module_file("inject-helper32.exe");
	}

	hook_path = obs_module_file(hook_dll);

	if (!check_file_integrity(gc, inject_path, "inject helper")) {
		goto cleanup;
	}
	if (!check_file_integrity(gc, hook_path, "graphics hook")) {
		goto cleanup;
	}

	info("using helper (%s hook)", "compatibility");
	success = create_inject_process(gc, inject_path, hook_dll);

cleanup:
	bfree(inject_path);
	bfree(hook_path);
	return success;
}

static const char *blacklisted_exes[] = {
	"explorer",
	"steam",
	"battle.net",
	"galaxyclient",
	"skype",
	"uplay",
	"origin",
	"devenv",
	"taskmgr",
	"chrome",
	"discord",
	"firefox",
	"systemsettings",
	"applicationframehost",
	"cmd",
	"shellexperiencehost",
	"winstore.app",
	"searchui",
	"lockapp",
	"windowsinternal.composableshell.experiences.textinput.inputapp",
	NULL,
};

static bool is_blacklisted_exe(const char *exe)
{
	char cur_exe[MAX_PATH];

	if (!exe)
		return false;

	for (const char **vals = blacklisted_exes; *vals; vals++) {
		strcpy(cur_exe, *vals);
		strcat(cur_exe, ".exe");

		if (strcmpi(cur_exe, exe) == 0)
			return true;
	}

	return false;
}

static bool target_suspended(struct wasapi_capture *gc)
{
	return thread_is_suspended(gc->process_id, gc->thread_id);
}

static bool init_events(struct wasapi_capture *gc);

static bool init_hook(struct wasapi_capture *gc)
{
	struct dstr exe = {0};
	bool blacklisted_process = false;

	if (get_window_exe(&exe, gc->next_window)) {
		info("attempting to hook process: %s", exe.array);
	}

	blacklisted_process = is_blacklisted_exe(exe.array);
	if (blacklisted_process)
		info("cannot capture %s due to being blacklisted", exe.array);
	dstr_free(&exe);

	if (blacklisted_process) {
		return false;
	}
	if (target_suspended(gc)) {
		return false;
	}
	if (!open_target_process(gc)) {
		return false;
	}
	if (!init_keepalive(gc)) {
		return false;
	}
	if (!init_pipe(gc)) {
		return false;
	}
	if (!attempt_existing_hook(gc)) {
		if (!inject_hook(gc)) {
			info("inject hook failed");
			return false;
		}
	}
	if (!init_audio_data_mutexes(gc)) {
		info("init audio datex mutex failed");
		return false;
	}
	if (!init_hook_info(gc)) {
		info("init hook info failed");
		return false;
	}
	if (!init_events(gc)) {
		info("init events failed");
		return false;
	}

	SetEvent(gc->hook_init);

	gc->window = gc->next_window;
	gc->next_window = NULL;
	gc->active = true;
	gc->retrying = 0;
	return true;
}

static void setup_window(struct wasapi_capture *gc, HWND window)
{
	HANDLE hook_restart;
	HANDLE process;

	GetWindowThreadProcessId(window, &gc->process_id);
	if (gc->process_id) {
		process = open_process(PROCESS_QUERY_INFORMATION, false, gc->process_id);
		if (process) {
			gc->is_app = is_app(process);
			if (gc->is_app) {
				gc->app_sid = get_app_sid(process);
			}
			CloseHandle(process);
		}
	}

	/* do not wait if we're re-hooking a process */
	hook_restart = open_event_gc(gc, EVENT_CAPTURE_RESTART);
	if (hook_restart) {
		gc->wait_for_target_startup = false;
		CloseHandle(hook_restart);
	}

	/* otherwise if it's an unhooked process, always wait a bit for the
	 * target process to start up before starting the hook process;
	 * sometimes they have important modules to load first or other hooks
	 * (such as steam) need a little bit of time to load.  ultimately this
	 * helps prevent crashes */
	if (gc->wait_for_target_startup) {
		gc->retry_interval = 3.0f;
		gc->wait_for_target_startup = false;
	} else {
		gc->next_window = window;
	}
}

static void get_selected_window(struct wasapi_capture *gc)
{
	HWND window;

	if (dstr_cmpi(&gc->class, "dwm") == 0) {
		wchar_t class_w[512];
		os_utf8_to_wcs(gc->class.array, 0, class_w, 512);
		window = FindWindowW(class_w, NULL);
	} else {
		window = find_window(INCLUDE_MINIMIZED, gc->priority, gc->class.array, gc->title.array, gc->executable.array);
	}

	if (window) {
		setup_window(gc, window);
	} else {
		gc->wait_for_target_startup = true;
	}
}

static void try_hook(struct wasapi_capture *gc)
{
	get_selected_window(gc);

	if (gc->next_window) {
		gc->thread_id = GetWindowThreadProcessId(gc->next_window, &gc->process_id);

		// Make sure we never try to hook ourselves (projector)
		if (gc->process_id == GetCurrentProcessId())
			return;

		if (!gc->thread_id && gc->process_id)
			return;
		if (!gc->process_id) {
			warn("error acquiring, failed to get window "
			     "thread/process ids: %lu",
			     GetLastError());
			gc->error_acquiring = true;
			return;
		}

		if (!init_hook(gc)) {
			stop_capture(gc);
		}
	} else {
		gc->active = false;
	}
}

static inline bool init_events(struct wasapi_capture *gc)
{
	if (!gc->hook_restart) {
		gc->hook_restart = open_event_gc(gc, EVENT_CAPTURE_RESTART);
		if (!gc->hook_restart) {
			warn("init_events: failed to get hook_restart "
			     "event: %lu",
			     GetLastError());
			return false;
		}
	}

	if (!gc->hook_stop) {
		gc->hook_stop = open_event_gc(gc, EVENT_CAPTURE_STOP);
		if (!gc->hook_stop) {
			warn("init_events: failed to get hook_stop event: %lu", GetLastError());
			return false;
		} else
			object_signalled(gc->hook_stop);
	}

	if (!gc->hook_init) {
		gc->hook_init = open_event_gc(gc, EVENT_HOOK_INIT);
		if (!gc->hook_init) {
			warn("init_events: failed to get hook_init event: %lu", GetLastError());
			return false;
		}
	}

	if (!gc->hook_ready) {
		gc->hook_ready = open_event_gc(gc, EVENT_HOOK_READY);
		if (!gc->hook_ready) {
			warn("init_events: failed to get hook_ready event: %lu", GetLastError());
			return false;
		}
	}

	if (!gc->hook_exit) {
		gc->hook_exit = open_event_gc(gc, EVENT_HOOK_EXIT);
		if (!gc->hook_exit) {
			warn("init_events: failed to get hook_exit event: %lu", GetLastError());
			return false;
		}
	}

	return true;
}

enum capture_result { CAPTURE_FAIL, CAPTURE_RETRY, CAPTURE_SUCCESS };

static inline enum capture_result init_capture_data(struct wasapi_capture *gc)
{
	if (gc->data) {
		UnmapViewOfFile(gc->data);
		gc->data = NULL;
	}

	CloseHandle(gc->hook_data_map);

	gc->hook_data_map = open_map_plus_id(gc, SHMEM_AUDIO, gc->global_hook_info->map_id);
	if (!gc->hook_data_map) {
		DWORD error = GetLastError();
		if (error == 2) {
			return CAPTURE_RETRY;
		} else {
			warn("init_capture_data: failed to open file "
			     "mapping: %lu",
			     error);
		}
		return CAPTURE_FAIL;
	}

	gc->data = MapViewOfFile(gc->hook_data_map, FILE_MAP_ALL_ACCESS, 0, 0, gc->global_hook_info->map_size);
	if (!gc->data) {
		warn("init_capture_data: failed to map data view: %lu", GetLastError());
		return CAPTURE_FAIL;
	}

	return CAPTURE_SUCCESS;
}

static void copy_shmem_tex(struct wasapi_capture *gc)
{
	if (!gc->shmem_data)
		return;

	if (object_signalled(gc->audio_data_mutex)) {
		uint32_t audio_size = gc->shmem_data->available_audio_size;
		if (audio_size > 0) {
			uint32_t offset = 0;
			while (true) {
				if (offset >= audio_size)
					break;

				uint8_t *buffer_data = gc->audio_data_buffer + offset + 4;

				uint32_t nf = 0;
				memcpy(&nf, gc->audio_data_buffer + offset, 4);
				offset += nf;

				//renderclient poniter
				//channel samplerate format byte_per_sample timestamp
				uint64_t ptr = 0;
				uint64_t timestamp = 0;
				uint32_t channel = 0, samplerate = 0, format = 0, byte_per_sample = 0;
				memcpy(&ptr, buffer_data, 8);
				memcpy(&channel, buffer_data + 8, 4);
				memcpy(&samplerate, buffer_data + 12, 4);
				memcpy(&format, buffer_data + 16, 4);
				memcpy(&byte_per_sample, buffer_data + 20, 4);
				memcpy(&timestamp, buffer_data + 24, 8);

				struct obs_source_audio data = {0};
				data.data[0] = (const uint8_t *)(buffer_data + 32);

				data.frames = (uint32_t)((nf - 36) / (channel * byte_per_sample));
				data.speakers = (enum speaker_layout)channel;
				data.samples_per_sec = samplerate;
				data.format = format;
				data.timestamp = timestamp;
				//data.timestamp -= util_mul_div64(data.frames, 1000000000ULL, data.samples_per_sec);
				obs_source_output_audio(gc->source, &data);

				static uint64_t ls = 0;
				debug("wascapture audio ts: %lld, len: %ld, timestamp: %lld", (data.timestamp - ls) / 1000000, data.frames, data.timestamp);
				ls = data.timestamp;

				gc->shmem_data->available_audio_size -= nf;
			}
		}
		ReleaseMutex(gc->audio_data_mutex);
	}
}

static inline bool init_shmem_capture(struct wasapi_capture *gc)
{
	gc->audio_data_buffer = (uint8_t *)gc->data + gc->shmem_data->audio_offset;
	return true;
}

static bool start_capture(struct wasapi_capture *gc)
{
	debug("Starting capture");

	if (!init_shmem_capture(gc)) {
		return false;
	}

	info("memory capture successful");

	return true;
}

static inline bool capture_valid(struct wasapi_capture *gc)
{
	return !object_signalled(gc->target_process);
}

static void wasapi_capture_tick(void *data, float seconds)
{
	struct wasapi_capture *gc = data;

	if (gc->hook_stop && object_signalled(gc->hook_stop)) {
		debug("hook stop signal received");
		stop_capture(gc);
	}

	if (gc->active && !gc->hook_ready && gc->process_id) {
		gc->hook_ready = open_event_gc(gc, EVENT_HOOK_READY);
	}

	if (gc->injector_process && object_signalled(gc->injector_process)) {
		DWORD exit_code = 0;

		GetExitCodeProcess(gc->injector_process, &exit_code);
		close_handle(&gc->injector_process);

		if (exit_code != 0) {
			warn("inject process failed: %ld", (long)exit_code);
			gc->error_acquiring = true;
		}
	}

	if (gc->hook_ready && object_signalled(gc->hook_ready)) {
		debug("capture initializing!");
		enum capture_result result = init_capture_data(gc);

		if (result == CAPTURE_SUCCESS)
			gc->capturing = start_capture(gc);
		else
			debug("init_capture_data failed");

		if (result != CAPTURE_RETRY && !gc->capturing) {
			gc->retry_interval = ERROR_RETRY_INTERVAL;
			stop_capture(gc);
		}
	}

	gc->retry_time += seconds;

	if (!gc->active) {
		if (!gc->error_acquiring && gc->retry_time > gc->retry_interval) {
			if (gc->activate_hook) {
				try_hook(gc);
				gc->retry_time = 0.0f;
			}
		}
	} else {
		if (!capture_valid(gc)) {
			info("capture window no longer exists, "
			     "terminating capture");
			stop_capture(gc);
		} else {
			copy_shmem_tex(gc);
		}
	}
}

static const char *wasapi_capture_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "windows wasapi capture";
}

static void wasapi_capture_defaults(obs_data_t *settings) {}

static bool window_not_blacklisted(const char *title, const char *class, const char *exe)
{
	UNUSED_PARAMETER(title);
	UNUSED_PARAMETER(class);

	return !is_blacklisted_exe(exe);
}

static void insert_preserved_val(obs_property_t *p, const char *val)
{
	char *class = NULL;
	char *title = NULL;
	char *executable = NULL;
	struct dstr desc = {0};

	build_window_strings(val, &class, &title, &executable);

	dstr_printf(&desc, "[%s]: %s", executable, title);
	obs_property_list_insert_string(p, 1, desc.array, val);
	obs_property_list_item_disable(p, 1, true);

	dstr_free(&desc);
	bfree(class);
	bfree(title);
	bfree(executable);
}

static bool window_changed_callback(obs_properties_t *ppts, obs_property_t *p, obs_data_t *settings)
{
	const char *cur_val;
	bool match = false;
	size_t i = 0;

	cur_val = obs_data_get_string(settings, SETTING_CAPTURE_WINDOW);
	if (!cur_val) {
		return false;
	}

	for (;;) {
		const char *val = obs_property_list_item_string(p, i++);
		if (!val)
			break;

		if (strcmp(val, cur_val) == 0) {
			match = true;
			break;
		}
	}

	if (cur_val && *cur_val && !match) {
		insert_preserved_val(p, cur_val);
		return true;
	}

	UNUSED_PARAMETER(ppts);
	return false;
}

static obs_properties_t *wasapi_capture_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_add_list(ppts, SETTING_CAPTURE_WINDOW, "Window", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, "", "");
	fill_window_list(p, INCLUDE_MINIMIZED, window_not_blacklisted);

	obs_property_set_modified_callback(p, window_changed_callback);

	UNUSED_PARAMETER(data);
	return ppts;
}

struct obs_source_info wasapi_capture_info = {
	.id = "wasapi_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = wasapi_capture_name,
	.create = wasapi_capture_create,
	.destroy = wasapi_capture_destroy,
	.get_defaults = wasapi_capture_defaults,
	.get_properties = wasapi_capture_properties,
	.update = wasapi_capture_update,
	.video_tick = wasapi_capture_tick,
};
