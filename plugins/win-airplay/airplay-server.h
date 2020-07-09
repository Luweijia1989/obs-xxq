#pragma once

#include <obs-module.h>
#include <obs.hpp>
#include "CAirServer.h"
#include "Airplay2Def.h"

class AirPlayServer {
public:
	AirPlayServer(obs_source_t *source);
	void outputVideo(SFgVideoFrame *data);
	void outputAudio(SFgAudioFrame *data);

private:
	obs_source_t *m_source = nullptr;
	CAirServer m_server;
};
