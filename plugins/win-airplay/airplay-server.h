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
#include <QObject>

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

class QTimer;
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

	ScreenMirrorServer(obs_source_t *source, int type);
	~ScreenMirrorServer();
	void outputVideo(uint8_t *data, size_t data_len, int64_t pts);
	void outputAudio(uint8_t *data, size_t data_len, int64_t pts, int serial);
	void doRenderer(gs_effect_t *effect);

	void mirrorServerSetup();
	void mirrorServerDestroy();

	void setBackendType(int type);
	void changeBackendType(int type);

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
	void handleMirrorStatus(int status);
	void handleMirrorStatusInternal(int status);
	bool handleMediaData();
	void updateStatusImage();
	void tickImage();
	void saveStatusSettings();

	void initDecoder(uint8_t *data, size_t len, bool forceRecreate, bool forceSoftware);
	void dropFrame(int64_t now_ms);
	void dropAudioFrame(int64_t now_ms);
	void initSoftOutputFrame();
	void updateSoftOutputFrame(AVFrame *frame);
	bool canProcessMediaData();
	void releaseRenderTexture();

private:
	std::thread m_audioLoopThread;
	media_audio_info m_audioInfo;
	bool m_stop = false;

	std::list<VideoFrame> m_videoFrames;
	std::list<AudioFrame> m_audioFrames;
	pthread_mutex_t m_videoDataMutex;
	pthread_mutex_t m_audioDataMutex;
	int64_t m_offset = LLONG_MAX;
	int64_t m_audioOffset = LLONG_MAX;
	int64_t m_extraDelay = 0;
	int64_t m_firstAudioRecvTime = LLONG_MAX;
	int64_t m_firstVideoRecvTime = LLONG_MAX;
	int64_t m_audioExtraOffset = LLONG_MAX;
	int64_t m_videoExtraOffset = LLONG_MAX;
	pthread_mutex_t m_ptsMutex;

	std::map<uint32_t, VideoInfo> m_videoInfos;
	uint32_t m_videoInfoIndex = 0;
	uint32_t m_lastVideoInfoIndex = 0;

	struct IPCServer *m_ipcServer = nullptr;
	os_process_pipe_t *process = nullptr;
	circlebuf m_avBuffer;
	MirrorBackEnd m_backend = None;
	MirrorBackEnd m_lastStopType = None;
	std::string m_backendProcessName;
	std::string m_backendStopImagePath;
	std::string m_backendLostImagePath;

	std::vector<std::string> m_resourceImgs;

	DXVA2Renderer *m_renderer = nullptr;
	AVDecoder *m_decoder = nullptr;
	AVFrame* m_decodedFrame = av_frame_alloc();
	AVPacket m_encodedPacket = { 0 };
	obs_source_frame2 m_softOutputFrame;

	uint8_t *pps_cache = nullptr;
	size_t pps_cache_len;

	QObject *m_timerHelperObject = nullptr;
	QTimer *m_helperTimer = nullptr;
	QTimer *m_tickTimer = nullptr;
};
