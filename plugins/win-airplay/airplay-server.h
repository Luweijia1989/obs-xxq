#pragma once

#include "tool.h"
#include <obs-module.h>
#include <obs.hpp>
#include <list>
#include <vector>
#include <map>
#include <string>
#include <limits>
#include "Airplay2Def.h"
#include <util/circlebuf.h>
#include "common-define.h"
#include "ipc.h"
#include <util/threading.h>
#include <util/platform.h>
#include <graphics/image-file.h>

#include "dxva2_decoder.h"
#include "dxva2_renderer.h"

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
	uint32_t video_info_index;
	uint8_t *data;
	size_t data_len;
	int64_t pts;
};

struct VideoInfo {
	uint8_t *data;
	size_t data_len;
};

class ScreenMirrorServer {
public:
	enum MirrorBackEnd {
		None = -1,
		IOS_USB_CABLE,
		IOS_AIRPLAY,
		ANDROID_USB_CABLE,
		ANDROID_AOA,
		ANDROID_WIRELESS,
	};

	ScreenMirrorServer(obs_source_t *source);
	~ScreenMirrorServer();
	void outputVideo(uint8_t *data, size_t data_len, int64_t pts);
	void outputAudio(size_t data_len, uint64_t pts, int serial);
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
	bool initPipe();
	void parseNalus(uint8_t *data, size_t size, uint8_t **out,
			size_t *out_len);
	void handleMirrorStatus(int status);
	bool handleMediaData();
	const char *killProc();
	void updateStatusImage();
	void loadImage(std::string path);
	void saveStatusSettings();

	void initDecoder(uint8_t *data, size_t len);
	void dropFrame(int64_t now_ms);
	void dropAudioFrame(int64_t now_ms);
	void initSoftOutputFrame();
	void updateSoftOutputFrame(AVFrame *frame);

private:
	HANDLE m_handler;

	std::thread m_audioLoopThread;
	uint32_t m_audioSampleRate = 0;
	bool m_stop = false;

	std::list<VideoFrame> m_videoFrames;
	circlebuf m_audioFrames;
	uint8_t *m_audioCacheBuffer = nullptr;
	uint8_t *m_audioTempBuffer = nullptr;
	pthread_mutex_t m_videoDataMutex;
	pthread_mutex_t m_audioDataMutex;
	pthread_mutex_t m_statusMutex;
	int64_t m_offset = LLONG_MAX;
	int64_t m_audioOffset = LLONG_MAX;
	int64_t m_extraDelay = 0;
	float m_startTimeElapsed = 0.;

	std::map<uint32_t, VideoInfo> m_videoInfos;
	uint32_t m_videoInfoIndex = 0;
	uint32_t m_lastVideoInfoIndex = 0;

	struct IPCServer *m_ipcServer = nullptr;
	os_process_pipe_t *process = nullptr;
	circlebuf m_avBuffer;
	MirrorBackEnd m_backend = None;
	MirrorBackEnd m_lastStopType = None;
	MirrorBackEnd m_audioFrameType = None;

	std::vector<std::string> m_resourceImgs;

	DXVA2Renderer *m_renderer = nullptr;
	AVDecoder *m_decoder = nullptr;
	AVFrame* m_decodedFrame = av_frame_alloc();
	AVPacket m_encodedPacket = { 0 };
	obs_source_frame2 m_softOutputFrame;
};
