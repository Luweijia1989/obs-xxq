#include "FgAirplayChannel.h"
#include "CAutoLock.h"
#include "../common-define.h"

FgAirplayChannel::FgAirplayChannel(IAirServerCallback *pCallback)
	: m_nRef(1), m_pCallback(pCallback)
{
}

FgAirplayChannel::~FgAirplayChannel()
{
	
}

void FgAirplayChannel::connected(const char *remoteName,
	const char *remoteDeviceId)
{
	if (m_pCallback)
		m_pCallback->connected(remoteName, remoteDeviceId);
}

void FgAirplayChannel::disconnected(const char *remoteName,
				 const char *remoteDeviceId)
{
	if (m_pCallback)
		m_pCallback->disconnected(remoteName, remoteDeviceId);
}

void FgAirplayChannel::outputVideo(h264_decode_struct *data,
				   const char *remoteName,
				   const char *remoteDeviceId)
{
	if (data->frame_type == 0) // pps
	{
		m_mediaInfo.video_extra_len = data->data_len;
		memcpy(m_mediaInfo.video_extra, data->data, data->data_len);
		m_pCallback->outputMediaInfo(&m_mediaInfo, remoteName,
					     remoteDeviceId);
	} else {
		m_pCallback->outputVideo(data->data, data->data_len, data->pts,
					 remoteName, remoteDeviceId);
		if (!m_videoSent) {
			m_videoSent = true;
		}
	}
}

void FgAirplayChannel::outputAudio(pcm_data_struct *data,
				   const char *remoteName,
				   const char *remoteDeviceId)
{
	if (m_videoSent) {
		m_pCallback->outputAudio((uint8_t *)data->data, data->data_len,
					 data->pts, data->serial, remoteName, remoteDeviceId);
	}
}

long FgAirplayChannel::addRef()
{
	long lRef = InterlockedIncrement(&m_nRef);
	return (m_nRef > 1 ? m_nRef : 1);
}

long FgAirplayChannel::release()
{
	LONG lRef = InterlockedDecrement(&m_nRef);
	if (0 == lRef) {
		delete this;
		return 0;
	}
	return (m_nRef > 1 ? m_nRef : 1);
}
