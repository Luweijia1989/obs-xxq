#include "FgAirplayChannel.h"
#include "CAutoLock.h"

FgAirplayChannel::FgAirplayChannel(IAirServerCallback *pCallback)
	: m_nRef(1), m_pCallback(pCallback)
{
	memset(&m_mediaInfo, 0, sizeof(media_info));
}

FgAirplayChannel::~FgAirplayChannel() {}

void FgAirplayChannel::outputVideo(h264_decode_struct *data,
				   const char *remoteName,
				   const char *remoteDeviceId)
{
	if (data->frame_type == 0) // pps sps
	{
		m_mediaInfo.pps_len = data->data_len;
		memcpy(m_mediaInfo.pps, data->data, data->data_len);
		m_receiveVideoInfo = true;
		if (m_receiveAudioInfo) {
			m_pCallback->outputMediaInfo(&m_mediaInfo);
		}
	} else {
		m_pCallback->outputVideo(data->data, data->data_len, data->pts,
					 remoteName, remoteDeviceId);
	}
}

void FgAirplayChannel::outputAudio(pcm_data_struct *data,
				   const char *remoteName,
				   const char *remoteDeviceId)
{
	if (!m_receiveAudioInfo) {
		m_mediaInfo.samples_per_sec = data->sample_rate;
		m_mediaInfo.format = AUDIO_FORMAT_16BIT;
		m_mediaInfo.speakers = (speaker_layout)data->channels;
		m_mediaInfo.bytes_per_frame = data->bits_per_sample;
		m_receiveAudioInfo = true;
	}

	if (m_receiveVideoInfo)
		m_pCallback->outputAudio((uint8_t *)data->data, data->data_len,
					 data->pts, remoteName, remoteDeviceId);
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
