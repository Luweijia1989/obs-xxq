#pragma once

#include <objbase.h>

#include <obs-module.h>
#include <obs.hpp>
#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/windows/WinHandle.hpp>
#include <util/threading.h>
#include "libdshowcapture/dshowcapture.hpp"
#include "ffmpeg-decode.h"
#include "encode-dstr.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <QString>

/*
 * TODO:
 *   - handle disconnections and reconnections
 *   - if device not present, wait for device to be plugged in
 */

#undef min
#undef max

using namespace std;
using namespace DShow;

/* settings defines that will cause errors if there are typos */
#define VIDEO_DEVICE_ID "video_device_id"
#define RES_TYPE "res_type"
#define RESOLUTION "resolution"
#define FRAME_INTERVAL "frame_interval"
#define VIDEO_FORMAT "video_format"
#define LAST_VIDEO_DEV_ID "last_video_device_id"
#define LAST_RESOLUTION "last_resolution"
#define BUFFERING_VAL "buffering"
#define FLIP_IMAGE "flip_vertically"
#define AUDIO_OUTPUT_MODE "audio_output_mode"
#define USE_CUSTOM_AUDIO "use_custom_audio_device"
#define AUDIO_DEVICE_ID "audio_device_id"
#define COLOR_SPACE "color_space"
#define COLOR_RANGE "color_range"
#define DEACTIVATE_WNS "deactivate_when_not_showing"
#define FACE_STICKER_ID "face_sticker_info"

#define TEXT_INPUT_NAME obs_module_text("VideoCaptureDevice")
#define TEXT_DEVICE obs_module_text("Device")
#define TEXT_CONFIG_VIDEO obs_module_text("ConfigureVideo")
#define TEXT_CONFIG_XBAR obs_module_text("ConfigureCrossbar")
#define TEXT_RES_FPS_TYPE obs_module_text("ResFPSType")
#define TEXT_CUSTOM_RES obs_module_text("ResFPSType.Custom")
#define TEXT_PREFERRED_RES obs_module_text("ResFPSType.DevPreferred")
#define TEXT_FPS_MATCHING obs_module_text("FPS.Matching")
#define TEXT_FPS_HIGHEST obs_module_text("FPS.Highest")
#define TEXT_RESOLUTION obs_module_text("Resolution")
#define TEXT_VIDEO_FORMAT obs_module_text("VideoFormat")
#define TEXT_FORMAT_UNKNOWN obs_module_text("VideoFormat.Unknown")
#define TEXT_BUFFERING obs_module_text("Buffering")
#define TEXT_BUFFERING_AUTO obs_module_text("Buffering.AutoDetect")
#define TEXT_BUFFERING_ON obs_module_text("Buffering.Enable")
#define TEXT_BUFFERING_OFF obs_module_text("Buffering.Disable")
#define TEXT_FLIP_IMAGE obs_module_text("FlipVertically")
#define TEXT_AUDIO_MODE obs_module_text("AudioOutputMode")
#define TEXT_MODE_CAPTURE obs_module_text("AudioOutputMode.Capture")
#define TEXT_MODE_DSOUND obs_module_text("AudioOutputMode.DirectSound")
#define TEXT_MODE_WAVEOUT obs_module_text("AudioOutputMode.WaveOut")
#define TEXT_CUSTOM_AUDIO obs_module_text("UseCustomAudioDevice")
#define TEXT_AUDIO_DEVICE obs_module_text("AudioDevice")
#define TEXT_ACTIVATE obs_module_text("Activate")
#define TEXT_DEACTIVATE obs_module_text("Deactivate")
#define TEXT_COLOR_SPACE obs_module_text("ColorSpace")
#define TEXT_COLOR_DEFAULT obs_module_text("ColorSpace.Default")
#define TEXT_COLOR_RANGE obs_module_text("ColorRange")
#define TEXT_RANGE_PARTIAL obs_module_text("ColorRange.Partial")
#define TEXT_RANGE_FULL obs_module_text("ColorRange.Full")
#define TEXT_DWNS obs_module_text("DeactivateWhenNotShowing")

enum ResType { ResType_Preferred, ResType_Custom };

enum class BufferingType : int64_t { Auto, On, Off };

void ffmpeg_log(void *bla, int level, const char *msg, va_list args);

class Decoder {
	struct ffmpeg_decode decode;

public:
	inline Decoder() { memset(&decode, 0, sizeof(decode)); }
	inline ~Decoder() { ffmpeg_decode_free(&decode); }

	inline operator ffmpeg_decode *() { return &decode; }
	inline ffmpeg_decode *operator->() { return &decode; }
};

class CriticalSection {
	CRITICAL_SECTION mutex;

public:
	inline CriticalSection() { InitializeCriticalSection(&mutex); }
	inline ~CriticalSection() { DeleteCriticalSection(&mutex); }

	inline operator CRITICAL_SECTION *() { return &mutex; }
};

class CriticalScope {
	CriticalSection &mutex;

	CriticalScope() = delete;
	CriticalScope &operator=(CriticalScope &cs) = delete;

public:
	inline CriticalScope(CriticalSection &mutex_) : mutex(mutex_)
	{
		EnterCriticalSection(mutex);
	}

	inline ~CriticalScope() { LeaveCriticalSection(mutex); }
};

enum class Action {
	None,
	Activate,
	ActivateBlock,
	Deactivate,
	Shutdown,
	ConfigVideo,
	ConfigAudio,
	ConfigCrossbar1,
	ConfigCrossbar2,
};

static DWORD CALLBACK DShowThread(LPVOID ptr);

class STThread;

class DShowInput {
public:
	obs_source_t *source;
	Device device;
	bool deactivateWhenNotShowing = false;
	bool deviceHasAudio = false;
	bool deviceHasSeparateAudioFilter = false;
	bool flip = false;
	bool active = false;

	//bool	     use_face_sticker = false;
	QString face_sticker_id =
		"C:\\Users\\luweijia.YUPAOPAO\\AppData\\Local\\yuerlive\\cache\\stickers\\1a493e27424248a0878d06c2bfe895d4.zip";
	STThread *stThread = nullptr;

	Decoder audio_decoder;
	Decoder video_decoder;

	VideoConfig videoConfig;
	AudioConfig audioConfig;

	obs_source_frame2 frame;
	obs_source_audio audio;

	WinHandle semaphore;
	WinHandle activated_event;
	WinHandle thread;
	CriticalSection mutex;
	vector<Action> actions;

	void QueueAction(Action action);

	void QueueActivate(obs_data_t *settings);

	DShowInput(obs_source_t *source_, obs_data_t *settings);

	~DShowInput();

	void updateInfo(const char *data);
	void OnEncodedVideoData(enum AVCodecID id, unsigned char *data,
				size_t size, long long ts);
	void OnEncodedAudioData(enum AVCodecID id, unsigned char *data,
				size_t size, long long ts);

	void OnVideoData(const VideoConfig &config, unsigned char *data,
			 size_t size, long long startTime, long long endTime);
	void OnAudioData(const AudioConfig &config, unsigned char *data,
			 size_t size, long long startTime, long long endTime);
	void OutputFrame(bool f, bool fh, VideoFormat vf, unsigned char *data,
			 size_t size, long long startTime, long long endTime);

	bool UpdateVideoConfig(obs_data_t *settings);
	bool UpdateAudioConfig(obs_data_t *settings);
	void SetActive(bool active);
	inline enum video_colorspace GetColorSpace(obs_data_t *settings) const;
	inline enum video_range_type GetColorRange(obs_data_t *settings) const;
	inline bool Activate(obs_data_t *settings);
	inline void Deactivate();

	inline void SetupBuffering(obs_data_t *settings);

	void DShowLoop();
};
