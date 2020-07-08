#pragma once

#include "Airplay2Def.h"

class IAirServerCallback {
public:
	virtual void connected(const char *remoteName,
			       const char *remoteDeviceId) = 0;
	virtual void disconnected(const char *remoteName,
				  const char *remoteDeviceId) = 0;
	virtual void outputAudio(SFgAudioFrame *data, const char *remoteName,
				 const char *remoteDeviceId) = 0;
	virtual void outputVideo(SFgVideoFrame *data, const char *remoteName,
				 const char *remoteDeviceId) = 0;

	virtual void videoPlay(char *url, double volume, double startPos) = 0;
	virtual void videoGetPlayInfo(double *duration, double *position,
				      double *rate) = 0;

	virtual void log(int level, const char *msg) = 0;
};

void *fgServerStart(const char serverName[AIRPLAY_NAME_LEN],
		    unsigned int raopPort, unsigned int airplayPort,
		    IAirServerCallback *callback);
void fgServerStop(void *handle);

float fgServerScale(void *handle, float fRatio);
