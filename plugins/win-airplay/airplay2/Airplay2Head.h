#pragma once

#include "../common-define.h"

#define AIRPLAY_NAME_LEN 128

class IAirServerCallback {
public:
	virtual void connected(const char *remoteName,
			       const char *remoteDeviceId) = 0;
	virtual void disconnected(const char *remoteName,
				  const char *remoteDeviceId) = 0;
	virtual void outputAudio(uint8_t *data, size_t data_len, uint64_t pts,
				 const char *remoteName,
				 const char *remoteDeviceId) = 0;
	virtual void outputVideo(uint8_t *data, size_t data_len, uint64_t pts,
				 const char *remoteName,
				 const char *remoteDeviceId) = 0;
	virtual void outputMediaInfo(media_info *info, const char *remoteName,
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
