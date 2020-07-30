#pragma once
#include <Windows.h>
#include "Airplay2Head.h"

class CAirServer;

class CAirServerCallback : public IAirServerCallback {
public:
	CAirServerCallback();
	virtual ~CAirServerCallback();

public:
	void setAirplayServer(CAirServer *s);

public:
	virtual void connected(const char *remoteName,
			       const char *remoteDeviceId);
	virtual void disconnected(const char *remoteName,
				  const char *remoteDeviceId);
	virtual void outputAudio(uint8_t *data, size_t data_len, uint64_t pts,
				 const char *remoteName,
				 const char *remoteDeviceId);
	virtual void outputVideo(uint8_t *data, size_t data_len, uint64_t pts,
				 const char *remoteName,
				 const char *remoteDeviceId);
	virtual void outputMediaInfo(media_info *info);

	virtual void videoPlay(char *url, double volume, double startPos);
	virtual void videoGetPlayInfo(double *duration, double *position,
				      double *rate);

	virtual void log(int level, const char *msg);

protected:
	char m_chRemoteDeviceId[128];
	CAirServer *m_airplayServer = nullptr;
};
