#pragma once

#include <obs-module.h>
#include <obs.hpp>
#include "Airplay2Def.h"
#include <util/circlebuf.h>
#include "VideoDecoder.h"
#include "common-define.h"
#include "ipc.h"
extern "C" {
#include <util/pipe.h>
}

//#define DUMPFILE

class ScreenMirrorServer {
public:
	enum MirrorBackEnd {
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

	static void pipeCallback(void *param, uint8_t *data, size_t size);

private:
	bool initPipe();
	void parseNalus(uint8_t *data, size_t size, uint8_t **out,
			size_t *out_len);
	void doWithNalu(uint8_t *data, size_t size);
	void handleMirrorStatus();
	bool handleAirplayData();
	bool handleUSBData();

private:
	obs_source_t *m_source = nullptr;
	obs_source_t *m_cropFilter = nullptr;
	obs_source_audio m_audioFrame;
	obs_source_frame2 m_videoFrame;
	long long lastPts = 0;

	struct IPCServer *m_ipcServer = nullptr;
	os_process_pipe_t *process;
	circlebuf m_avBuffer;
	VideoDecoder m_decoder;
	struct media_info m_mediaInfo;
	bool m_infoReceived = false;
	MirrorBackEnd m_backend = IOS_AIRPLAY;

#ifdef DUMPFILE
	FILE *m_auioFile;
	FILE *m_videoFile;
#endif
};
