#pragma once

#include <obs-module.h>
#include <obs.hpp>
#include "Airplay2Def.h"
#include <util/circlebuf.h>
#include "VideoDecoder.h"
#include "common-define.h"
#include "ipc.h"
#include "util/threading.h"
#include <graphics/image-file.h>
#include "audio-monitoring/win32/wasapi-output.h"
#include "media-io/audio-resampler.h"
extern "C" {
#include <util/pipe.h>
}

//#define DUMPFILE

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
	void outputVideo(SFgVideoFrame *data);
	void outputAudio(SFgAudioFrame *data);

	void ipcSetup();
	void ipcDestroy();
	void mirrorServerSetup();
	void mirrorServerDestroy();

	void setBackendType(int type);
	int backendType();
	void loadImage(const char *path);
	void renderImage(gs_effect_t *effect);

	static void pipeCallback(void *param, uint8_t *data, size_t size);
	int m_width = 0;
	int m_height = 0;
	obs_source_t *m_source = nullptr;
	pthread_mutex_t m_typeChangeMutex;
	obs_source_mirror_status mirror_status = OBS_SOURCE_MIRROR_STOP;
	gs_image_file2_t *if2 = nullptr;

private:
	bool initAudioRenderer();
	void destroyAudioRenderer();
	void onAudioData(uint8_t *data, size_t size);
	void resetResampler(enum speaker_layout speakers, enum audio_format format, uint32_t samples_per_sec);
	bool initPipe();
	void parseNalus(uint8_t *data, size_t size, uint8_t **out,
			size_t *out_len);
	void doWithNalu(uint8_t *data, size_t size);
	void handleMirrorStatus();
	bool handleAirplayData();
	bool handleUSBData();
	const char *killProc();

private:
	IMMDevice *device;
	IAudioClient *client;
	IAudioRenderClient *render;
	audio_resampler_t *resampler = nullptr;
	uint32_t sample_rate;
	uint32_t channels;
	struct resample_info to;
	struct resample_info from;
	bool play_back_started = false;

	obs_source_t *m_cropFilter = nullptr;
	obs_source_audio m_audioFrame;
	obs_source_frame2 m_videoFrame;
	long long lastPts = 0;

	struct IPCServer *m_ipcServer = nullptr;
	os_process_pipe_t *process = nullptr;
	circlebuf m_avBuffer;
	VideoDecoder m_decoder;
	struct media_info m_mediaInfo;
	bool m_infoReceived = false;
	MirrorBackEnd m_backend = None;

#ifdef DUMPFILE
	FILE *m_auioFile;
	FILE *m_videoFile;
#endif
};
