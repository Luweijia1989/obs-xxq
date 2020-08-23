#include "FgAirplayChannel.h"
#include "CAutoLock.h"
#include "../common-define.h"

FgAirplayChannel::FgAirplayChannel(IAirServerCallback *pCallback)
	: m_nRef(1), m_pCallback(pCallback)
{
	memset(&m_mediaInfo, 0, sizeof(media_info));
	m_mediaInfo.samples_per_sec = 44100;
	m_mediaInfo.format = AUDIO_FORMAT_16BIT;
	m_mediaInfo.speakers = (speaker_layout)2;
	m_mediaInfo.bytes_per_frame = 16;
}

FgAirplayChannel::~FgAirplayChannel()
{
	
}

void FgAirplayChannel::connected(const char *remoteName,
	const char *remoteDeviceId)
{
	m_sendMutex.lock();
	if (m_pCallback)
		m_pCallback->connected(remoteName, remoteDeviceId);
	m_sendMutex.unlock();
}

void FgAirplayChannel::disconnected(const char *remoteName,
				 const char *remoteDeviceId)
{
	m_sendMutex.lock();
	if (m_pCallback)
		m_pCallback->disconnected(remoteName, remoteDeviceId);
	m_sendMutex.unlock();
}

void FgAirplayChannel::outputVideo(h264_decode_struct *data,
				   const char *remoteName,
				   const char *remoteDeviceId)
{
	m_sendMutex.lock();
	if (data->frame_type == 0) // pps sps
	{
		m_mediaInfo.pps_len = data->data_len;
		memcpy(m_mediaInfo.pps, data->data, data->data_len);
		m_pCallback->outputMediaInfo(&m_mediaInfo, remoteName,
					     remoteDeviceId);
	} else {
		m_pCallback->outputVideo(data->data, data->data_len, data->pts,
					 remoteName, remoteDeviceId);
		if (!m_videoSent) {
			m_videoSent = true;
		}
	}
	m_sendMutex.unlock();
}

void FgAirplayChannel::outputAudio(pcm_data_struct *data,
				   const char *remoteName,
				   const char *remoteDeviceId)
{
	m_sendMutex.lock();
	if (m_videoSent) {
		m_pCallback->outputAudio((uint8_t *)data->data, data->data_len,
					 data->pts, remoteName, remoteDeviceId);
	}
	m_sendMutex.unlock();
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
