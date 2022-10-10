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
#include "audio-channel.h"

#define do_log(level, format, ...) blog(level, "[wasapi-capture: '%s'] " format, obs_source_get_name(gc->source), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

#define SETTING_CAPTURE_WINDOW "window"

#define DEFAULT_RETRY_INTERVAL 2.0f
#define ERROR_RETRY_INTERVAL 4.0f

struct ts_info {
	uint64_t start;
	uint64_t end;
};

#define DEBUG_AUDIO 0
#define MAX_BUFFERING_TICKS 45

struct wasapi_capture_config {
	char *title;
	char *class;
	char *executable;
};

struct audio_channel_info {
	uint64_t ptr;
	struct audio_channel *channel;
};

struct wasapi_capture {
	obs_source_t *source;
	struct wasapi_capture_config config;

	HANDLE injector_process;

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
	bool initial_config;
	bool is_app;

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
	HANDLE audio_data_event;
	wchar_t *app_sid;
	int retrying;

	union {
		struct {
			struct shmem_data *shmem_data;
			uint8_t *audio_data_buffer;
		};

		void *data;
	};

	HANDLE capture_thread;
	HANDLE mix_thread;
	struct resample_info out_sample_info;
	DARRAY(struct audio_channel_info) audio_channels;
	size_t block_size;
	size_t channels;
	size_t planes;
	float buffer[MAX_AUDIO_CHANNELS][AUDIO_OUTPUT_FRAMES];

	DARRAY(struct audio_channel *) mix_channels;

	pthread_mutex_t channel_mutex;
	uint64_t buffered_ts;
	struct circlebuf buffered_timestamps;
	uint64_t buffering_wait_ticks;
	int total_buffering_ticks;
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

	gc->capturing = false;
	SetEvent(gc->audio_data_event);
	if (gc->capture_thread != INVALID_HANDLE_VALUE) {
		WaitForSingleObject(gc->capture_thread, INFINITE);
		gc->capture_thread = INVALID_HANDLE_VALUE;
	}
	if (gc->mix_thread != INVALID_HANDLE_VALUE) {
		WaitForSingleObject(gc->mix_thread, INFINITE);
		gc->mix_thread = INVALID_HANDLE_VALUE;
	}

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
	close_handle(&gc->audio_data_event);

	if (gc->active)
		info("capture stopped");

	gc->wait_for_target_startup = false;
	gc->active = false;

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

	for (size_t i = 0; i < gc->audio_channels.num; i++) {
		audio_channel_destroy(gc->audio_channels.array[i].channel);
	}
	da_free(gc->audio_channels);

	pthread_mutex_destroy(&gc->channel_mutex);
	circlebuf_free(&gc->buffered_timestamps);
	da_free(gc->mix_channels);

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
	gc->capture_thread = INVALID_HANDLE_VALUE;
	pthread_mutex_init_value(&gc->channel_mutex);
	pthread_mutex_init(&gc->channel_mutex, NULL);
	da_init(gc->audio_channels);

	struct obs_audio_info audio_info;
	obs_get_audio_info(&audio_info);
	gc->out_sample_info.format = AUDIO_FORMAT_FLOAT_PLANAR;
	gc->out_sample_info.samples_per_sec = audio_info.samples_per_sec;
	gc->out_sample_info.speakers = audio_info.speakers;

	bool planar = is_audio_planar(gc->out_sample_info.format);
	gc->channels = get_audio_channels(gc->out_sample_info.speakers);
	gc->planes = planar ? gc->channels : 1;
	gc->block_size = (planar ? 1 : gc->channels) * get_audio_bytes_per_channel(gc->out_sample_info.format);

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

	gc->audio_data_event = open_event_gc(gc, AUDIO_DATA_EVENT);

	if (!gc->audio_data_event) {
		DWORD error = GetLastError();
		if (error == 2) {
			if (!gc->retrying) {
				gc->retrying = 2;
				info("hook not loaded yet, retrying..");
			}
		} else {
			warn("failed to open audio data events: %lu", GetLastError());
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

static inline bool init_shmem_capture(struct wasapi_capture *gc)
{
	gc->audio_data_buffer = (uint8_t *)gc->data + gc->shmem_data->audio_offset;
	return true;
}

static inline void find_min_ts(struct wasapi_capture *gc, uint64_t *min_ts)
{
	for (size_t i = 0; i < gc->audio_channels.num; i++) {
		struct audio_channel *source = gc->audio_channels.array[i].channel;
		if (!source->audio_pending && source->audio_ts && source->audio_ts < *min_ts) {
			*min_ts = source->audio_ts;
		}
	}
}

static inline bool mark_invalid_sources(struct wasapi_capture *gc, size_t sample_rate, uint64_t min_ts)
{
	bool recalculate = false;

	for (size_t i = 0; i < gc->audio_channels.num; i++) {
		struct audio_channel *source = gc->audio_channels.array[i].channel;
		recalculate |= audio_channel_audio_buffer_insuffient(source, sample_rate, min_ts);
	}

	return recalculate;
}

static inline void calc_min_ts(struct wasapi_capture *gc, size_t sample_rate, uint64_t *min_ts)
{
	find_min_ts(gc, min_ts);
	if (mark_invalid_sources(gc, sample_rate, *min_ts))
		find_min_ts(gc, min_ts);
}

static void wasapi_capture_add_audio_buffering(struct wasapi_capture *gc, size_t sample_rate, struct ts_info *ts, uint64_t min_ts)
{
	struct ts_info new_ts;
	uint64_t offset;
	uint64_t frames;
	size_t total_ms;
	size_t ms;
	int ticks;

	if (gc->total_buffering_ticks == MAX_BUFFERING_TICKS)
		return;

	if (!gc->buffering_wait_ticks)
		gc->buffered_ts = ts->start;

	offset = ts->start - min_ts;
	frames = ns_to_audio_frames(sample_rate, offset);
	ticks = (int)((frames + AUDIO_OUTPUT_FRAMES - 1) / AUDIO_OUTPUT_FRAMES);

	gc->total_buffering_ticks += ticks;

	if (gc->total_buffering_ticks >= MAX_BUFFERING_TICKS) {
		ticks -= gc->total_buffering_ticks - MAX_BUFFERING_TICKS;
		gc->total_buffering_ticks = MAX_BUFFERING_TICKS;
		blog(LOG_WARNING, "Max audio buffering reached!");
	}

	ms = ticks * AUDIO_OUTPUT_FRAMES * 1000 / sample_rate;
	total_ms = gc->total_buffering_ticks * AUDIO_OUTPUT_FRAMES * 1000 / sample_rate;

	blog(LOG_INFO,
	     "wasapi-capture===>: adding %d milliseconds of audio buffering, total "
	     "audio buffering is now %d milliseconds",
	     (int)ms, (int)total_ms);
#if DEBUG_AUDIO == 1
	blog(LOG_DEBUG,
	     "min_ts (%" PRIu64 ") < start timestamp "
	     "(%" PRIu64 ")",
	     min_ts, ts->start);
	blog(LOG_DEBUG, "old buffered ts: %" PRIu64 "-%" PRIu64, ts->start, ts->end);
#endif

	new_ts.start = gc->buffered_ts - audio_frames_to_ns(sample_rate, gc->buffering_wait_ticks * AUDIO_OUTPUT_FRAMES);

	while (ticks--) {
		uint64_t cur_ticks = ++gc->buffering_wait_ticks;

		new_ts.end = new_ts.start;
		new_ts.start = gc->buffered_ts - audio_frames_to_ns(sample_rate, cur_ticks * AUDIO_OUTPUT_FRAMES);

#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG, "add buffered ts: %" PRIu64 "-%" PRIu64, new_ts.start, new_ts.end);
#endif

		circlebuf_push_front(&gc->buffered_timestamps, &new_ts, sizeof(new_ts));
	}

	*ts = new_ts;
}

static inline void wasapi_capture_mix_audio(struct audio_output_data *mixes, struct audio_channel *source, size_t channels, size_t sample_rate,
					    struct ts_info *ts)
{
	size_t total_floats = AUDIO_OUTPUT_FRAMES;
	size_t start_point = 0;

	if (source->audio_ts < ts->start || ts->end <= source->audio_ts)
		return;

	if (source->audio_ts != ts->start) {
		start_point = convert_time_to_frames(sample_rate, source->audio_ts - ts->start);
		if (start_point == AUDIO_OUTPUT_FRAMES)
			return;

		total_floats -= start_point;
	}

	for (size_t ch = 0; ch < channels; ch++) {
		register float *mix = mixes->data[ch];
		register float *aud = source->audio_output_buf[ch];
		register float *end;

		mix += start_point;
		end = aud + total_floats;

		while (aud < end)
			*(mix++) += *(aud++);
	}
}

static void ignore_audio(struct audio_channel *source, size_t channels, size_t sample_rate)
{
	size_t num_floats = source->audio_input_buf[0].size / sizeof(float);

	if (num_floats) {
		for (size_t ch = 0; ch < channels; ch++)
			circlebuf_pop_front(&source->audio_input_buf[ch], NULL, source->audio_input_buf[ch].size);

		source->last_audio_input_buf_size = 0;
		source->audio_ts += (uint64_t)num_floats * 1000000000ULL / (uint64_t)sample_rate;
	}
}

static bool discard_if_stopped(struct audio_channel *source, size_t channels)
{
	size_t last_size;
	size_t size;

	last_size = source->last_audio_input_buf_size;
	size = source->audio_input_buf[0].size;

	if (!size)
		return false;

	/* if perpetually pending data, it means the audio has stopped,
	 * so clear the audio data */
	if (last_size == size) {
		if (!source->pending_stop) {
			source->pending_stop = true;
#if DEBUG_AUDIO == 1
			blog(LOG_DEBUG, "doing pending stop trick: '0x%p'", source);
#endif
			return true;
		}

		for (size_t ch = 0; ch < channels; ch++)
			circlebuf_pop_front(&source->audio_input_buf[ch], NULL, source->audio_input_buf[ch].size);

		source->pending_stop = false;
		source->audio_ts = 0;
		source->last_audio_input_buf_size = 0;
#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG, "source audio data appears to have "
				"stopped, clearing");
#endif
		return true;
	} else {
		source->last_audio_input_buf_size = size;
		return false;
	}
}

#define MAX_AUDIO_SIZE (AUDIO_OUTPUT_FRAMES * sizeof(float))

static inline void wasapi_capture_discard_audio(struct wasapi_capture *gc, struct audio_channel *source, size_t channels, size_t sample_rate,
						struct ts_info *ts)
{
	size_t total_floats = AUDIO_OUTPUT_FRAMES;
	size_t size;

	if (ts->end <= source->audio_ts) {
#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG,
		     "can't discard, source "
		     "timestamp (%" PRIu64 ") >= "
		     "end timestamp (%" PRIu64 ")",
		     source->audio_ts, ts->end);
#endif
		return;
	}

	if (source->audio_ts < (ts->start - 1)) {
		if (source->audio_pending && source->audio_input_buf[0].size < MAX_AUDIO_SIZE && discard_if_stopped(source, channels))
			return;

#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG,
		     "can't discard, source "
		     "timestamp (%" PRIu64 ") < "
		     "start timestamp (%" PRIu64 ")",
		     source->audio_ts, ts->start);
#endif
		if (gc->total_buffering_ticks == MAX_BUFFERING_TICKS)
			ignore_audio(source, channels, sample_rate);
		return;
	}

	if (source->audio_ts != ts->start && source->audio_ts != (ts->start - 1)) {
		size_t start_point = convert_time_to_frames(sample_rate, source->audio_ts - ts->start);
		if (start_point == AUDIO_OUTPUT_FRAMES) {
#if DEBUG_AUDIO == 1
			blog(LOG_DEBUG, "can't discard, start point is "
					"at audio frame count");
#endif
			return;
		}

		total_floats -= start_point;
	}

	size = total_floats * sizeof(float);

	if (source->audio_input_buf[0].size < size) {
		if (discard_if_stopped(source, channels))
			return;

#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG, "can't discard, data still pending");
#endif
		source->audio_ts = ts->end;
		return;
	}

	for (size_t ch = 0; ch < channels; ch++)
		circlebuf_pop_front(&source->audio_input_buf[ch], NULL, size);

	source->last_audio_input_buf_size = 0;

#if DEBUG_AUDIO == 1
	blog(LOG_DEBUG, "audio discarded, new ts: %" PRIu64, ts->end);
#endif

	source->pending_stop = false;
	source->audio_ts = ts->end;
}

bool wasapi_capture_fetch_audio(struct wasapi_capture *gc, uint64_t start_ts_in, uint64_t end_ts_in, uint64_t *out_ts, struct audio_output_data *mixes)
{
	da_resize(gc->mix_channels, 0);

	struct ts_info ts = {start_ts_in, end_ts_in};
	circlebuf_push_back(&gc->buffered_timestamps, &ts, sizeof(ts));
	circlebuf_peek_front(&gc->buffered_timestamps, &ts, sizeof(ts));

	uint64_t min_ts = ts.start;

	size_t audio_size = AUDIO_OUTPUT_FRAMES * sizeof(float);

#if DEBUG_AUDIO == 1
	blog(LOG_DEBUG, "ts %llu-%llu", ts.start, ts.end);
#endif

	pthread_mutex_lock(&gc->channel_mutex);
	for (size_t i = 0; i < gc->audio_channels.num; i++) {
		da_push_back(gc->mix_channels, &(gc->audio_channels.array[i].channel));
	}
	pthread_mutex_unlock(&gc->channel_mutex);
	/* ------------------------------------------------ */
	/* render audio data */
	for (size_t i = 0; i < gc->mix_channels.num; i++) {
		audio_channel_pick_audio_data(gc->mix_channels.array[i], audio_size, gc->channels);
	}

	/* ------------------------------------------------ */
	/* get minimum audio timestamp */
	pthread_mutex_lock(&gc->channel_mutex);
	calc_min_ts(gc, gc->out_sample_info.samples_per_sec, &min_ts);
	pthread_mutex_unlock(&gc->channel_mutex);

	/* ------------------------------------------------ */
	/* if a source has gone backward in time, buffer */
	if (min_ts < ts.start)
		wasapi_capture_add_audio_buffering(gc, gc->out_sample_info.samples_per_sec, &ts, min_ts);

	/* ------------------------------------------------ */
	/* mix audio */
	if (!gc->buffering_wait_ticks) {
		for (size_t i = 0; i < gc->mix_channels.num; i++) {
			struct audio_channel *source = gc->mix_channels.array[i];

			if (source->audio_pending)
				continue;

			pthread_mutex_lock(&source->audio_buf_mutex);

			if (source->audio_output_buf[0][0] && source->audio_ts)
				wasapi_capture_mix_audio(mixes, source, gc->channels, gc->out_sample_info.samples_per_sec, &ts);

			pthread_mutex_unlock(&source->audio_buf_mutex);
		}
	}

	/* ------------------------------------------------ */
	/* discard audio */
	pthread_mutex_lock(&gc->channel_mutex);
	for (size_t i = 0; i < gc->audio_channels.num; i++) {
		struct audio_channel *a_c = gc->audio_channels.array[i].channel;
		pthread_mutex_lock(&a_c->audio_buf_mutex);
		wasapi_capture_discard_audio(gc, a_c, gc->channels, gc->out_sample_info.samples_per_sec, &ts);
		pthread_mutex_unlock(&a_c->audio_buf_mutex);
	}
	pthread_mutex_unlock(&gc->channel_mutex);

	circlebuf_pop_front(&gc->buffered_timestamps, NULL, sizeof(ts));

	*out_ts = ts.start;

	if (gc->buffering_wait_ticks) {
		gc->buffering_wait_ticks--;
		return false;
	}

	return true;
}

static inline void clamp_audio_output(struct wasapi_capture *gc, size_t bytes)
{
	size_t float_size = bytes / sizeof(float);

	for (size_t plane = 0; plane < gc->planes; plane++) {
		float *mix_data = gc->buffer[plane];
		float *mix_end = &mix_data[float_size];

		while (mix_data < mix_end) {
			float val = *mix_data;
			val = (val > 1.0f) ? 1.0f : val;
			val = (val < -1.0f) ? -1.0f : val;
			*(mix_data++) = val;
		}
	}
}

static void wasapi_capture_input_and_output(struct wasapi_capture *gc, uint64_t audio_time, uint64_t prev_time)
{
	size_t bytes = AUDIO_OUTPUT_FRAMES * gc->block_size;
	struct audio_output_data data;
	uint64_t new_ts = 0;
	bool success;

	memset(&data, 0, sizeof(struct audio_output_data));

#if DEBUG_AUDIO == 1
	blog(LOG_DEBUG, "audio_time: %llu, prev_time: %llu, bytes: %lu", audio_time, prev_time, bytes);
#endif

	/* clear mix buffers */
	memset(gc->buffer[0], 0, AUDIO_OUTPUT_FRAMES * MAX_AUDIO_CHANNELS * sizeof(float));

	for (size_t i = 0; i < gc->planes; i++)
		data.data[i] = gc->buffer[i];

	/* get new audio data */
	success = wasapi_capture_fetch_audio(gc, prev_time, audio_time, &new_ts, &data);
	if (!success)
		return;

	///* clamps audio data to -1.0..1.0 */
	clamp_audio_output(gc, bytes);

	struct obs_source_audio audio;
	audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
	audio.frames = AUDIO_OUTPUT_FRAMES;
	audio.samples_per_sec = gc->out_sample_info.samples_per_sec;
	audio.speakers = gc->out_sample_info.speakers;
	audio.timestamp = new_ts;
	for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		audio.data[i] = (const uint8_t *)data.data[i];
	}
	obs_source_output_audio(gc->source, &audio);
}

static void mix_thread_proc(LPVOID param)
{
	os_set_thread_name("wasapi-capture: audio mix thread");

	struct wasapi_capture *gc = param;
	size_t rate = gc->out_sample_info.samples_per_sec;
	uint64_t samples = 0;
	uint64_t start_time = os_gettime_ns();
	uint64_t prev_time = start_time;
	uint64_t audio_time = prev_time;
	uint32_t audio_wait_time = (uint32_t)(audio_frames_to_ns(rate, AUDIO_OUTPUT_FRAMES) / 1000000);

	while (gc->capturing) {
		os_sleep_ms(audio_wait_time);

		uint64_t cur_time = os_gettime_ns();
		while (audio_time <= cur_time) {
			samples += AUDIO_OUTPUT_FRAMES;
			audio_time = start_time + audio_frames_to_ns(rate, samples);

			wasapi_capture_input_and_output(gc, audio_time, prev_time);

			prev_time = audio_time;
		}
	}
}

static void output_audio_data(struct wasapi_capture *gc, struct obs_source_audio *data, uint64_t ptr)
{
	struct audio_channel *channel = NULL;
	pthread_mutex_lock(&gc->channel_mutex);
	for (size_t i = 0; i < gc->audio_channels.num; i++) {
		if (gc->audio_channels.array[i].ptr == ptr) {
			channel = gc->audio_channels.array[i].channel;
			break;
		}
	}
	pthread_mutex_unlock(&gc->channel_mutex);

	if (!channel) {
		channel = audio_channel_create(&gc->out_sample_info);
		struct audio_channel_info info;
		info.channel = channel;
		info.ptr = ptr;
		pthread_mutex_lock(&gc->channel_mutex);
		da_push_back(gc->audio_channels, &info);
		pthread_mutex_unlock(&gc->channel_mutex);
	}

	audio_channel_output_audio(channel, data);
}

static void capture_thread_proc(LPVOID param)
{
	os_set_thread_name("wasapi-capture: audio capture thread");
	struct wasapi_capture *gc = param;
	while (gc->capturing) {
		if (WaitForSingleObject(gc->audio_data_event, 100) == WAIT_OBJECT_0) {
			if (!gc->capturing)
				break;

			if (WaitForSingleObject(gc->audio_data_mutex, 10) == WAIT_OBJECT_0) {
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

						//renderclient poniter channel samplerate format byte_per_sample timestamp
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
						output_audio_data(gc, &data, ptr);

						gc->shmem_data->available_audio_size -= nf;
					}
				}
				ReleaseMutex(gc->audio_data_mutex);
			}
		}
	}
}

static void start_capture(struct wasapi_capture *gc)
{
	debug("Starting capture");

	if (!init_shmem_capture(gc)) {
		return;
	}

	info("memory capture successful");

	gc->capturing = true;

	gc->capture_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)capture_thread_proc, gc, 0, NULL);
	gc->mix_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)mix_thread_proc, gc, 0, NULL);
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
			start_capture(gc);
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
