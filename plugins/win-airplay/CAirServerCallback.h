#pragma once
#include <Windows.h>
#include "Airplay2Head.h"

class AirPlayServer;

class CAirServerCallback : public IAirServerCallback {
public:
	CAirServerCallback();
	virtual ~CAirServerCallback();

public:
	void setAirplayServer(AirPlayServer *s);

public:
	virtual void connected(const char *remoteName,
			       const char *remoteDeviceId);
	virtual void disconnected(const char *remoteName,
				  const char *remoteDeviceId);
	virtual void outputAudio(SFgAudioFrame *data, const char *remoteName,
				 const char *remoteDeviceId);
	virtual void outputVideo(SFgVideoFrame *data, const char *remoteName,
				 const char *remoteDeviceId);

	virtual void videoPlay(char *url, double volume, double startPos);
	virtual void videoGetPlayInfo(double *duration, double *position,
				      double *rate);

	virtual void log(int level, const char *msg);

protected:
	char m_chRemoteDeviceId[128];
	AirPlayServer *m_airplayServer = nullptr;
};
