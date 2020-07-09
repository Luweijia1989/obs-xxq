#include <obs-module.h>
#include "airplay-server.h"
#include <cstdio>

AirPlayServer::AirPlayServer(obs_source_t *source) : m_source(source)
{
	memset(m_videoFrame.data, 0, sizeof(m_videoFrame.data));
	memset(&m_audioFrame, 0, sizeof(&m_audioFrame));
	memset(&m_videoFrame, 0, sizeof(&m_videoFrame));

	m_videoFrame.range = VIDEO_RANGE_PARTIAL;
	video_format_get_parameters(VIDEO_CS_601, m_videoFrame.range,
				    m_videoFrame.color_matrix,
				    m_videoFrame.color_range_min,
				    m_videoFrame.color_range_max);

	m_server.start(this);
}

AirPlayServer::~AirPlayServer()
{
	m_server.stop();
	clearVideoFrameBuffer();
}

void AirPlayServer::clearVideoFrameBuffer()
{
	for (int i = 0; i < MAX_AV_PLANES; i++) {
		free(m_videoFrame.data[i]);
		m_videoFrame.data[i] = nullptr;
	}
}

void AirPlayServer::outputVideo(SFgVideoFrame *data)
{
	int picWidth = data->width;
	int picHeight = data->height;

	if (picWidth != m_videoFrame.width ||
	    picHeight != m_videoFrame.height) {
		clearVideoFrameBuffer();

		m_videoFrame.data[0] = (uint8_t *)malloc(picWidth * picHeight);
		m_videoFrame.data[1] =
			(uint8_t *)malloc(picWidth * picHeight / 4);
		m_videoFrame.data[2] =
			(uint8_t *)malloc(picWidth * picHeight / 4);
	}

	m_videoFrame.timestamp = data->pts;
	m_videoFrame.width = picWidth;
	m_videoFrame.height = picHeight;
	m_videoFrame.format = VIDEO_FORMAT_I420;
	m_videoFrame.flip = false;
	m_videoFrame.flip_h = false;

	for (size_t i = 0; i < data->height; i++) {
		memcpy(m_videoFrame.data[0] + i * picWidth,
		       data->data + i * data->pitch[0], picWidth);
		if (i % 2 == 0) {
			uint8_t *u_start = data->data + data->dataLen[0];
			memcpy(m_videoFrame.data[1] + (i >> 1) * picWidth,
			       u_start + (i >> 1) * data->pitch[1],
			       (i >> 1) * picWidth);
			/*memcpy(m_videoFrame.data[2] + (i >> 1) * picWidth,
			       data->data + data->dataLen[0] +
				       data->dataLen[1] +
				       (i >> 1) * data->pitch[2],
			       (i >> 1) * picWidth);*/
		}
	}

	m_videoFrame.linesize[0] = picWidth;
	m_videoFrame.linesize[1] = picWidth / 2;
	m_videoFrame.linesize[2] = picWidth / 2;
	obs_source_output_video2(m_source, &m_videoFrame);
}

void AirPlayServer::outputAudio(SFgAudioFrame *data)
{
	m_audioFrame.format = AUDIO_FORMAT_16BIT;
	m_audioFrame.samples_per_sec = data->sampleRate;
	m_audioFrame.speakers = data->channels == 2 ? SPEAKERS_STEREO
						    : SPEAKERS_MONO;
	m_audioFrame.data[0] = data->data;
	m_audioFrame.frames = data->dataLen / (data->channels * 2);
	m_audioFrame.timestamp = data->pts;
	obs_source_output_audio(m_source, &m_audioFrame);
}

OBS_DECLARE_MODULE()

OBS_MODULE_USE_DEFAULT_LOCALE("win-airplay", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "iOS screen mirroring source";
}

static const char *GetWinAirplayName(void *type_data)
{
	return obs_module_text("WindowsAirplay");
}

static void *CreateWinAirplay(obs_data_t *settings, obs_source_t *source)
{
	AirPlayServer *server = new AirPlayServer(source);
	return server;
}

static void DestroyWinAirplay(void *obj)
{
	delete reinterpret_cast<AirPlayServer *>(obj);
}

static void UpdateWinAirplaySource(void *obj, obs_data_t *settings) {}

static void GetWinAirplayDefaultsOutput(obs_data_t *settings) {}

static obs_properties_t *GetWinAirplayPropertiesOutput(void *)
{
	return nullptr;
}

static void HideWinAirplay(void *data) {}

static void ShowWinAirplay(void *data) {}

bool obs_module_load(void)
{
	obs_source_info info = {};
	info.id = "win_airplay";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO |
			    OBS_SOURCE_ASYNC | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.show = ShowWinAirplay;
	info.hide = HideWinAirplay;
	info.get_name = GetWinAirplayName;
	info.create = CreateWinAirplay;
	info.destroy = DestroyWinAirplay;
	info.update = UpdateWinAirplaySource;
	info.get_defaults = GetWinAirplayDefaultsOutput;
	info.get_properties = GetWinAirplayPropertiesOutput;
	obs_register_source(&info);

	return true;
}

void obs_module_unload(void) {}
