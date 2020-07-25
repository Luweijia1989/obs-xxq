#pragma once

#include <obs-module.h>
#include <obs.hpp>
#include "CAirServer.h"
#include "Airplay2Def.h"
#include <ipc-util/pipe.h>
#include <util/circlebuf.h>
extern "C" {
#include <util/pipe.h>
}

class ScreenMirrorServer {
public:
	ScreenMirrorServer(obs_source_t *source);
	~ScreenMirrorServer();
	void outputVideo(SFgVideoFrame *data);
	void outputAudio(SFgAudioFrame *data);

	void ipcSetup();
	void ipcDestroy();

	static void pipeCallback(void *param, uint8_t *data, size_t size);

private:
	bool initPipe();
	void checkAndOpenUsbMirror();
	void quitUsbMirror();

private:
	obs_source_t *m_source = nullptr;
	obs_source_t *m_cropFilter = nullptr;
	CAirServer m_server;
	obs_source_audio m_audioFrame;
	obs_source_frame2 m_videoFrame;
	long long lastPts = 0;

	ipc_pipe_server_t pipe;
	os_process_pipe_t *process;
	circlebuf m_avBuffer;
};
