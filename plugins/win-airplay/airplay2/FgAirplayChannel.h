#pragma once
#include <queue>
#include "Airplay2Head.h"
#include "stream.h"
#include <mutex>

class FgAirplayChannel {
public:
	FgAirplayChannel(IAirServerCallback *pCallback);
	~FgAirplayChannel();
	void connected(const char *remoteName,
			 const char *remoteDeviceId);
	void disconnected(const char *remoteName, const char *remoteDeviceId);
	void outputVideo(h264_decode_struct *data, const char *remoteName,
			 const char *remoteDeviceId);
	void outputAudio(pcm_data_struct *data, const char *remoteName,
			 const char *remoteDeviceId);

public:
	long addRef();
	long release();

protected:
	long m_nRef;
	IAirServerCallback *m_pCallback;
	bool m_videoSent = false;
	media_info m_mediaInfo;
	std::mutex m_sendMutex;
};
