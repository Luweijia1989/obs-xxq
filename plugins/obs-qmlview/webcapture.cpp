#include "webcapture.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QDateTime>
#include <QImage>
#include <graphics/matrix4.h>

#define EVENT_CAPTURE_RESTART "CaptureHook_Restart"
#define EVENT_CAPTURE_STOP "CaptureHook_Stop"

#define EVENT_HOOK_READY "CaptureHook_HookReady"
#define EVENT_HOOK_EXIT "CaptureHook_Exit"

#define WINDOW_HOOK_KEEPALIVE "CaptureHook_KeepAlive"
#define CAPTURE_HOOK_CLIENT_KEEPALIVE "CaptureHook_Client_KeepAlive"

#define MUTEX_TEXTURE1 "CaptureHook_TextureMutex1"
#define MUTEX_TEXTURE2 "CaptureHook_TextureMutex2"

#define SHMEM_HOOK_INFO "CaptureHook_HookInfo"
#define SHMEM_TEXTURE "CaptureHook_Texture"

static void close_handle(HANDLE *p_handle)
{
	HANDLE handle = *p_handle;
	if (handle) {
		if (handle != INVALID_HANDLE_VALUE)
			CloseHandle(handle);
		*p_handle = NULL;
	}
}

#define GC_EVENT_FLAGS (EVENT_MODIFY_STATE | SYNCHRONIZE)
#define GC_MUTEX_FLAGS (SYNCHRONIZE)
#define GC_MAPPING_FLAGS (FILE_MAP_READ | FILE_MAP_WRITE)

static inline HANDLE create_event(const wchar_t *name)
{
	return CreateEventW(NULL, false, false, name);
}

static inline HANDLE open_event(const wchar_t *name)
{
	return OpenEventW(GC_EVENT_FLAGS, false, name);
}

static inline HANDLE create_mutex(const wchar_t *name)
{
	return CreateMutexW(NULL, false, name);
}

static inline HANDLE open_mutex(const wchar_t *name)
{
	return OpenMutexW(GC_MUTEX_FLAGS, false, name);
}

static inline HANDLE create_event_plus_md5(QString name, QString md5)
{
	auto new_name = name + md5;
	return create_event(new_name.toStdWString().c_str());
}

static inline HANDLE create_mutex_plus_md5(QString name, QString md5)
{
	auto new_name = name + md5;
	return create_mutex(new_name.toStdWString().c_str());
}

static inline bool object_signalled(HANDLE event)
{
	if (!event)
		return false;

	return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
}

static inline HANDLE open_mutex_plus_md5(QString name, QString md5)
{
	auto new_name = name + md5;
	return open_mutex(new_name.toStdWString().c_str());
}

static inline HANDLE open_map_plus_md5(QString name, QString md5)
{
	auto new_name = name + md5;
	return OpenFileMappingW(GC_MAPPING_FLAGS, false, new_name.toStdWString().c_str());
}

struct shmem_data {
	volatile int last_tex;
	uint32_t tex1_offset;
	uint32_t tex2_offset;
};

struct shtex_data {
	uintptr_t tex_handle;
	bool tex_changed;
	unsigned long process_id;
};

enum capture_type {
	CAPTURE_TYPE_MEMORY,
	CAPTURE_TYPE_TEXTURE,
};

struct capture_info {
	/* capture info */
	enum capture_type type;
	uint32_t cx;
	uint32_t cy;
	uint32_t map_id;
	uint32_t map_size;
};

WebCapture::WebCapture() : m_source(nullptr)
{
	data = nullptr;
	shtex_data = nullptr;
}

WebCapture::~WebCapture()
{
	stopCapture();
}

static const char *webcapture_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("webcapture-source");
}

static void webcapture_source_update(void *data, obs_data_t *settings)
{
	WebCapture *capture = (WebCapture *)data;
	QString url = obs_data_get_string(settings, "url");
	capture->updateUrl(url);
	capture->stage = (capture_stage)obs_data_get_int(settings, "stage");
}

static void *webcapture_source_create(obs_data_t *settings, obs_source_t *source)
{
	WebCapture *capture = new WebCapture();
	capture->m_source = source;
	if (settings) {
		capture->updateUrl(obs_data_get_string(settings, "url"));
		capture->stage = (capture_stage)obs_data_get_int(settings, "stage");
	}
	return capture;
}

static void webcapture_source_destroy(void *data)
{
	if (!data)
		return;
	WebCapture *capture = (WebCapture *)data;
	delete capture;
}

static void webcapture_source_defaults(obs_data_t *settings) {}

static obs_properties_t *webcapture_source_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	obs_properties_add_text(ppts, "url", "web url", OBS_TEXT_DEFAULT);
	obs_properties_add_int(ppts, "stage", "capture stage", 0, 2, 1);
	return ppts;
}

static uint32_t webcapture_source_getwidth(void *data)
{
	if (!data)
		return 0;
	WebCapture *capture = (WebCapture *)data;
	return capture->width;
}

static uint32_t webcapture_source_getheight(void *data)
{
	if (!data)
		return 0;
	WebCapture *capture = (WebCapture *)data;
	return capture->height;
}

static void webcapture_source_show(void *data) {}

static void webcapture_source_hide(void *data) {}

static void webcapture_source_render(void *data, gs_effect_t *effect)
{
	if (!data)
		return;
	WebCapture *capture = (WebCapture *)data;

	auto compute_scale = [](uint32_t in_w, uint32_t in_h, float *out_w_scale, float *out_h_scale, float *x_offset, float *y_offset) {
		struct obs_video_info ovi;
		obs_get_video_info(&ovi);

		auto gameRatio = (float)in_w / (float)in_h;
		auto viewRatio = (float)ovi.base_width / (float)ovi.base_height;
		int tw = 0, th = 0;
		if (gameRatio > viewRatio) {
			tw = ovi.base_width;
			th = tw / gameRatio;
		} else {
			th = ovi.base_height;
			tw = th * gameRatio;
		}

		*out_w_scale = (float)tw / (float)in_w;
		*out_h_scale = (float)th / (float)in_h;

		*x_offset = (float)(ovi.base_width - tw) / 2.f;
		*y_offset = (float)(ovi.base_height - th) / 2.f;
	};

	gs_texture_t *render_texture = nullptr;
	if (capture->stage == capture_stage::START) {
		if (!capture->texture) {
			return;
		}
		render_texture = capture->texture;
	} else if (capture->stage == capture_stage::PAUSE) {
		if (!capture->pause_texture) {
			QImage ss(":/mark/image/main/xbox/cloudgame_bk.png");
			uint8_t *bd = (uint8_t *)ss.constBits();

			capture->pause_texture = gs_texture_create(ss.width(), ss.height(), GS_BGRA, 1, (const uint8_t **)&bd, 0);
		}

		if (capture->pause_texture)
			render_texture = capture->pause_texture;
	} else if (capture->stage == capture_stage::STOP) {
		// do nothing
	}

	if (!render_texture)
		return;

	auto w = gs_texture_get_width(render_texture);
	auto h = gs_texture_get_height(render_texture);

	if (capture->last_width != w || capture->last_height != h) {
		compute_scale(w, h, &capture->w_s, &capture->h_s, &capture->x_offset, &capture->y_offset);
		capture->last_width = w;
		capture->last_height = h;
	}

	gs_matrix_push();
	matrix4 matrix;
	memset(&matrix, 0, sizeof(matrix));
	vec4_set(&matrix.x, capture->w_s, 0.f, 0.f, 0.f);
	vec4_set(&matrix.y, 0.f, capture->h_s, 0.f, 0.f);
	vec4_set(&matrix.z, 0.f, 0.f, 1.f, 0.f);
	vec4_set(&matrix.t, 0.f, 1.f, 0.f, 1.f);
	gs_matrix_translate3f(capture->x_offset, capture->y_offset, 0.0f);
	gs_matrix_mul(&matrix);

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), render_texture);
	obs_source_draw(render_texture, 0, 0, 0, 0, true);

	gs_matrix_pop();
}

void WebCapture::stopCapture()
{
	if (capture_stop) {
		SetEvent(capture_stop);
	}
	if (global_hook_info) {
		UnmapViewOfFile(global_hook_info);
		global_hook_info = NULL;
	}
	if (data) {
		UnmapViewOfFile(data);
		data = NULL;
	}

	close_handle(&capture_restart);
	close_handle(&capture_stop);
	close_handle(&capture_ready);
	close_handle(&capture_exit);
	close_handle(&capture_data_map);
	close_handle(&keepalive_mutex);
	close_handle(&global_hook_info_map);
	close_handle(&texture_mutexes[0]);
	close_handle(&texture_mutexes[1]);
	close_handle(&dumped_tex_handle);
	close_handle(&capture_process_id);

	if (texture) {
		obs_enter_graphics();
		gs_texture_destroy(texture);
		obs_leave_graphics();
		texture = NULL;
	}

	if (pause_texture) {
		obs_enter_graphics();
		gs_texture_destroy(pause_texture);
		obs_leave_graphics();
		pause_texture = NULL;
	}

	if (active)
		blog(LOG_DEBUG, "capture stopped");

	active = false;
	capturing = false;
}

void WebCapture::updateUrl(QString url)
{
	if (url.isEmpty())
		return;

	if (current_url == url)
		return;

	stopCapture();
	url_md5 = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex();
	current_url = url;
}

HANDLE WebCapture::open_event_gc(QString name)
{
	QString new_name = name + url_md5;
	return open_event(new_name.toStdWString().c_str());
}

bool WebCapture::attempExistCapture()
{
	capture_restart = open_event_gc(EVENT_CAPTURE_RESTART);
	if (capture_restart) {
		blog(LOG_DEBUG, "existing capture found.");
		SetEvent(capture_restart);
		return true;
	}
	return false;
}

bool WebCapture::init_texture_mutexes()
{
	texture_mutexes[0] = open_mutex_plus_md5(MUTEX_TEXTURE1, url_md5);
	texture_mutexes[1] = open_mutex_plus_md5(MUTEX_TEXTURE2, url_md5);

	if (!texture_mutexes[0] || !texture_mutexes[1]) {
		DWORD error = GetLastError();
		blog(LOG_DEBUG, "failed to open texture mutexes: %lu", GetLastError());
		return false;
	}

	return true;
}

bool WebCapture::init_capture_info()
{
	global_hook_info_map = open_map_plus_md5(SHMEM_HOOK_INFO, url_md5);
	if (!global_hook_info_map) {
		blog(LOG_DEBUG, "init_hook_info: get_hook_info failed: %lu", GetLastError());
		return false;
	}

	global_hook_info = (capture_info *)MapViewOfFile(global_hook_info_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*global_hook_info));
	if (!global_hook_info) {
		blog(LOG_DEBUG, "init_hook_info: failed to map data view: %lu", GetLastError());
		return false;
	}

	return true;
}

bool WebCapture::init_events()
{
	if (!capture_restart) {
		capture_restart = open_event_gc(EVENT_CAPTURE_RESTART);
		if (!capture_restart) {
			blog(LOG_DEBUG,
			     "init_events: failed to get hook_restart "
			     "event: %lu",
			     GetLastError());
			return false;
		}
	}

	if (!capture_stop) {
		capture_stop = open_event_gc(EVENT_CAPTURE_STOP);
		if (!capture_stop) {
			blog(LOG_DEBUG, "init_events: failed to get hook_stop event: %lu", GetLastError());
			return false;
		}
	}

	if (!capture_ready) {
		capture_ready = open_event_gc(EVENT_HOOK_READY);
		if (!capture_ready) {
			blog(LOG_DEBUG, "init_events: failed to get hook_ready event: %lu", GetLastError());
			return false;
		}
	}

	if (!capture_exit) {
		capture_exit = open_event_gc(EVENT_HOOK_EXIT);
		if (!capture_exit) {
			blog(LOG_DEBUG, "init_events: failed to get hook_exit event: %lu", GetLastError());
			return false;
		}
	}

	return true;
}

bool WebCapture::tryCapture()
{
	auto new_name = WINDOW_HOOK_KEEPALIVE + url_md5;
	keepalive_mutex = CreateMutexW(NULL, false, new_name.toStdWString().c_str());
	if (!keepalive_mutex) {
		blog(LOG_DEBUG, "Failed to create keepalive mutex: %lu", GetLastError());
		return false;
	}

	if (!attempExistCapture())
		return false;

	if (!init_texture_mutexes())
		return false;

	if (!init_capture_info())
		return false;

	if (!init_events())
		return false;

	active = true;
	return true;
}

capture_result WebCapture::init_capture_data()
{
	width = global_hook_info->cx;
	height = global_hook_info->cy;

	if (data) {
		UnmapViewOfFile(data);
		data = NULL;
	}

	CloseHandle(capture_data_map);

	auto new_name = QString("%1%2").arg(SHMEM_TEXTURE).arg(global_hook_info->map_id);
	capture_data_map = OpenFileMappingW(GC_MAPPING_FLAGS, false, new_name.toStdWString().c_str());
	if (!capture_data_map) {
		DWORD error = GetLastError();
		if (error == 2) {
			return CAPTURE_RETRY;
		} else {
			blog(LOG_DEBUG,
			     "init_capture_data: failed to open file "
			     "mapping: %lu",
			     error);
		}
		return CAPTURE_FAIL;
	}

	data = MapViewOfFile(capture_data_map, FILE_MAP_ALL_ACCESS, 0, 0, global_hook_info->map_size);
	if (!data) {
		blog(LOG_DEBUG, "init_capture_data: failed to map data view: %lu", GetLastError());
		return CAPTURE_FAIL;
	}

	return CAPTURE_SUCCESS;
}

bool WebCapture::init_shmem_capture()
{
	enum gs_color_format format;

	texture_buffers[0] = (uint8_t *)data + shmem_data->tex1_offset;
	texture_buffers[1] = (uint8_t *)data + shmem_data->tex2_offset;

	obs_enter_graphics();
	gs_texture_destroy(texture);
	texture = gs_texture_create(width, height, GS_BGRA, 1, NULL, GS_DYNAMIC);
	obs_leave_graphics();

	if (!texture) {
		blog(LOG_DEBUG, "init_shmem_capture: failed to create texture");
		return false;
	}

	return true;
}

bool WebCapture::init_shtex_capture()
{
	capture_process_id = OpenProcess(PROCESS_ALL_ACCESS, FALSE, shtex_data->process_id);
	if (capture_process_id == NULL)
		return false;

	return true;
}

bool WebCapture::capture_client_active()
{
	auto client_alive = [this]() {
		auto new_name = CAPTURE_HOOK_CLIENT_KEEPALIVE + url_md5;
		HANDLE handle = OpenMutexW(SYNCHRONIZE, false, new_name.toStdWString().c_str());
		if (handle) {
			CloseHandle(handle);
			return true;
		}

		return GetLastError() != ERROR_FILE_NOT_FOUND;
	};
	uint64_t cur_time = QDateTime::currentMSecsSinceEpoch();
	bool alive = true;

	if (cur_time - lastKeepaliveCheck_ > 500) {
		alive = client_alive();
		lastKeepaliveCheck_ = cur_time;
	}

	return alive;
}

bool WebCapture::startCapture()
{
	blog(LOG_DEBUG, "Starting capture");

	if (global_hook_info->type == CAPTURE_TYPE_MEMORY) {
		if (!init_shmem_capture()) {
			return false;
		}

		blog(LOG_DEBUG, "memory capture successful");
	} else {
		if (!init_shtex_capture()) {
			return false;
		}

		blog(LOG_DEBUG, "shared texture capture successful");
	}

	return true;
}

void WebCapture::copyMemTexture()
{
	int cur_texture;
	HANDLE mutex = NULL;
	uint32_t pitch;
	int next_texture;
	uint8_t *data;

	if (!shmem_data)
		return;

	cur_texture = shmem_data->last_tex;

	if (cur_texture < 0 || cur_texture > 1)
		return;

	next_texture = cur_texture == 1 ? 0 : 1;

	if (object_signalled(texture_mutexes[cur_texture])) {
		mutex = texture_mutexes[cur_texture];

	} else if (object_signalled(texture_mutexes[next_texture])) {
		mutex = texture_mutexes[next_texture];
		cur_texture = next_texture;

	} else {
		return;
	}

	if (gs_texture_map(texture, &data, &pitch)) {
		auto p = width * 4;
		if (pitch == p) {
			memcpy(data, texture_buffers[cur_texture], pitch * height);
		} else {
			uint8_t *input = texture_buffers[cur_texture];
			uint32_t best_pitch = pitch < p ? pitch : p;

			for (uint32_t y = 0; y < height; y++) {
				uint8_t *line_in = input + p * y;
				uint8_t *line_out = data + pitch * y;
				memcpy(line_out, line_in, best_pitch);
			}
		}
		gs_texture_unmap(texture);
	}

	ReleaseMutex(mutex);
}

void WebCapture::copyShareTexture()
{
	HANDLE mutex = NULL;
	if (!shtex_data)
		return;

	if (object_signalled(texture_mutexes[0])) {
		mutex = texture_mutexes[0];
	} else {
		return;
	}

	if (shtex_data->tex_changed) {
		if (dumped_tex_handle)
			close_handle(&dumped_tex_handle);

		if (texture) {
			gs_texture_destroy(texture);
			texture = nullptr;
		}

		if (DuplicateHandle(capture_process_id, (HANDLE)shtex_data->tex_handle, GetCurrentProcess(), &dumped_tex_handle, 0, FALSE,
				    DUPLICATE_SAME_ACCESS)) {
			texture = gs_texture_open_nt_shared((uint32_t)dumped_tex_handle);
		}

		shtex_data->tex_changed = false;
	}

	ReleaseMutex(mutex);
}

static void webcapture_tick(void *data, float seconds)
{
	WebCapture *capture = (WebCapture *)data;
	if (capture->url_md5.isEmpty())
		return;

	if (capture->capture_stop && object_signalled(capture->capture_stop)) {
		blog(LOG_DEBUG, "hook stop signal received");
		capture->stopCapture();
	}

	if (capture->active && !capture->capture_ready) {
		capture->capture_ready = capture->open_event_gc(EVENT_HOOK_READY);
	}

	if (capture->capture_ready && object_signalled(capture->capture_ready)) {
		blog(LOG_DEBUG, "capture initializing!");
		enum capture_result result = capture->init_capture_data();

		if (result == CAPTURE_SUCCESS)
			capture->capturing = capture->startCapture();
		else
			blog(LOG_DEBUG, "init_capture_data failed");

		if (result != CAPTURE_RETRY && !capture->capturing) {
			capture->stopCapture();
		}
	}

	capture->retry_time += seconds;

	if (!capture->active) {
		if (capture->retry_time > capture->retry_interval) {
			if (!capture->tryCapture())
				capture->stopCapture();

			capture->retry_time = 0.0f;
		}
	} else {
		if (!capture->capture_client_active()) {
			blog(LOG_DEBUG, "capture window no longer exists, "
					"terminating capture");
			capture->stopCapture();
		} else {
			if (capture->global_hook_info->type == CAPTURE_TYPE_MEMORY) {
				obs_enter_graphics();
				capture->copyMemTexture();
				obs_leave_graphics();
			} else {
				obs_enter_graphics();
				capture->copyShareTexture();
				obs_leave_graphics();
			}
		}
	}
}

struct obs_source_info webcapture_source_info = {"webcapture_source",
						 OBS_SOURCE_TYPE_INPUT,
						 OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,
						 webcapture_source_get_name,
						 webcapture_source_create,
						 webcapture_source_destroy,
						 webcapture_source_getwidth,
						 webcapture_source_getheight,
						 webcapture_source_defaults,
						 webcapture_source_properties,
						 webcapture_source_update,
						 nullptr,
						 nullptr,
						 webcapture_source_show,
						 webcapture_source_hide,
						 webcapture_tick,
						 webcapture_source_render,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr,
						 nullptr};
