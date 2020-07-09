#pragma once

#include <obs-module.h>
#include <obs.hpp>
#include "CAirServer.h"
#include "Airplay2Def.h"

class AirPlayServer {
public:
	AirPlayServer(obs_source_t *source);
	~AirPlayServer();
	void outputVideo(SFgVideoFrame *data);
	void outputAudio(SFgAudioFrame *data);

private:
	void clearVideoFrameBuffer();

private:
	obs_source_t *m_source = nullptr;
	obs_source_t *m_cropFilter = nullptr;
	CAirServer m_server;
	obs_source_audio m_audioFrame;
	obs_source_frame2 m_videoFrame;
	uint8_t buf[1080 * 512 * 3 / 2];
};
