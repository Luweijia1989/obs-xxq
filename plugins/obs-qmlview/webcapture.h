#pragma once

#include "obs.h"
#include "obs-module.h"
#include <QString>
#include <Windows.h>

enum capture_result { CAPTURE_FAIL, CAPTURE_RETRY, CAPTURE_SUCCESS };
enum capture_stage { STOP, START, PAUSE };
class WebCapture {
public:
	bool attempExistCapture();
	bool init_texture_mutexes();
	bool init_capture_info();
	bool init_events();
	capture_result init_capture_data();
	bool init_shmem_capture();
	bool init_shtex_capture();
	bool capture_client_active();

	HANDLE open_event_gc(QString name);

	WebCapture();
	~WebCapture();

	bool startCapture();
	void stopCapture();
	bool tryCapture();
	void copyMemTexture();
	void copyShareTexture();
	void updateUrl(QString url);

	obs_source_t *m_source{};
	int width{};
	int height{};

	union {
		struct {
			struct shmem_data *shmem_data;
			uint8_t *texture_buffers[2];
		};

		struct shtex_data *shtex_data;
		void *data;
	};

	QString current_url{};
	QString url_md5{};
	capture_stage stage{};
	gs_texture_t *texture{};
	gs_texture_t *pause_texture{};
	bool active{};
	bool capturing{};
	struct capture_info *global_hook_info{};
	HANDLE keepalive_mutex{};
	HANDLE capture_init{};
	HANDLE capture_restart{};
	HANDLE capture_stop{};
	HANDLE capture_ready{};
	HANDLE capture_exit{};
	HANDLE capture_data_map{};
	HANDLE global_hook_info_map{};
	HANDLE texture_mutexes[2]{};

	HANDLE capture_process_id = NULL;
	HANDLE dumped_tex_handle = NULL;

	float retry_time{};
	float retry_interval = 0.2;
	uint64_t lastKeepaliveCheck_{};
	float w_s = 0.f, h_s = 0.f, x_offset = 0.f, y_offset = 0.f;
	uint32_t last_width = 0, last_height = 0;
};
