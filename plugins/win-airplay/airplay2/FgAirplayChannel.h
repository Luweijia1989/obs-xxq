#pragma once
#include <queue>
#include "Airplay2Head.h"
#include "stream.h"

class FgAirplayChannel {
public:
	FgAirplayChannel(IAirServerCallback *pCallback);
	~FgAirplayChannel();
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
	bool m_receiveAudioInfo = false;
	bool m_receiveVideoInfo = false;
	media_info m_mediaInfo;
};
