#pragma once

#include <obs-module.h>
#include <obs.hpp>
#include <list>
#include <vector>
#include <string>
#include <limits>
#include "Airplay2Def.h"
#include <util/circlebuf.h>
#include "VideoDecoder.h"
#include "common-define.h"
#include "ipc.h"
#include <util/threading.h>
#include <util/platform.h>
#include <graphics/image-file.h>
#include "media-io/audio-resampler.h"
#include <portaudio.h>

#include "d3d11va_decoder.h"
#include "d3d11va_renderer.h"

extern "C" {
#include <util/pipe.h>
}

struct AudioFrame {
	uint8_t *data;
	size_t data_len;
	int64_t pts;
	int serial;
};

struct VideoFrame {
	bool is_header;
	uint8_t *data;
	size_t data_len;
	int64_t pts;
};

class ScreenMirrorServer {
public:
	enum MirrorBackEnd {
		None = -1,
		IOS_USB_CABLE,
		IOS_AIRPLAY,
		ANDROID_USB_CABLE,
	};

	ScreenMirrorServer(obs_source_t *source);
	~ScreenMirrorServer();
	void outputVideo(bool is_header, uint8_t *data, size_t data_len, int64_t pts);
	void outputAudio(size_t data_len, uint64_t pts, int serial);
	void outputAudioFrame(uint8_t *data, size_t size);
	void doRenderer(gs_effect_t *effect);

	void mirrorServerSetup();
	void mirrorServerDestroy();

	void setBackendType(int type);
	int backendType();

	static void pipeCallback(void *param, uint8_t *data, size_t size);
	static void WinAirplayVideoTick(void *data, float seconds);
	static void *CreateWinAirplay(obs_data_t *settings, obs_source_t *source);
	static void *audio_tick_thread(void *data);
	int m_width = 0;
	int m_height = 0;
	obs_source_t *m_source = nullptr;
	obs_source_mirror_status mirror_status = OBS_SOURCE_MIRROR_STOP;
	gs_image_file2_t *if2 = nullptr;
	uint64_t last_time = 0;
	gs_texture_t *m_renderTexture = nullptr;

private:
	void dumpResourceImgs();
	void resetState();
	bool initAudioRenderer();
	void destroyAudioRenderer();
	void resetResampler(enum speaker_layout speakers,
			    enum audio_format format, uint32_t samples_per_sec);
	bool initPipe();
	void parseNalus(uint8_t *data, size_t size, uint8_t **out,
			size_t *out_len);
	void handleMirrorStatus(int status);
	bool handleMediaData();
	const char *killProc();
	void updateStatusImage();
	void loadImage(const char *path);
	void saveStatusSettings();

	void initDecoder(uint8_t *data, size_t len);

private:
	HANDLE m_handler;

	PaStreamParameters open_param_;
	PaStream *pa_stream_ = nullptr;
	audio_resampler_t *resampler = nullptr;
	uint32_t sample_rate;
	uint32_t channels;
	struct resample_info to;
	struct resample_info from;

	std::list<VideoFrame > m_videoFrames;
	std::list<AudioFrame *> m_audioFrames;
	pthread_mutex_t m_videoDataMutex;
	pthread_mutex_t m_audioDataMutex;
	pthread_mutex_t m_statusMutex;
	pthread_t m_audioTh;
	bool m_running = true;
	int64_t m_offset = LLONG_MAX;
	int64_t m_audioOffset = LLONG_MAX;
	int64_t m_lastAudioPts = LLONG_MAX;
	int m_audioPacketSerial = -1;
	int64_t m_extraDelay = 0;
	float m_startTimeElapsed = 0.;

	struct IPCServer *m_ipcServer = nullptr;
	os_process_pipe_t *process = nullptr;
	circlebuf m_avBuffer;
	MirrorBackEnd m_backend = None;
	MirrorBackEnd m_lastStopType = None;

	std::vector<std::string> m_resourceImgs;

	D3D11VARenderer *m_renderer = nullptr;
	AVDecoder *m_decoder = nullptr;
	AVFrame* m_decodedFrame = av_frame_alloc();
	AVPacket m_encodedPacket = { 0 };
};
