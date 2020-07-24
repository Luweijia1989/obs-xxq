#include <obs-module.h>
#include "airplay-server.h"
#include <cstdio>
#include "common-define.h"
#include <util/dstr.h>
#include <util/platform.h>

#define IOS_USB_EXE "ios-usb-mirror.exe"

ScreenMirrorServer::ScreenMirrorServer(obs_source_t *source) : m_source(source)
{
	memset(m_videoFrame.data, 0, sizeof(m_videoFrame.data));
	memset(&m_audioFrame, 0, sizeof(&m_audioFrame));
	memset(&m_videoFrame, 0, sizeof(&m_videoFrame));

	m_videoFrame.range = VIDEO_RANGE_PARTIAL;
	video_format_get_parameters(VIDEO_CS_601, m_videoFrame.range,
				    m_videoFrame.color_matrix,
				    m_videoFrame.color_range_min,
				    m_videoFrame.color_range_max);

	//bool success = m_server.start(this); //返回失败可以搞一个默认图片显示

	if (!init_pipe())
		blog(LOG_ERROR, "fail to create pipe");
	else
	{
		//struct dstr cmd;
		//dstr_init_move_array(&cmd, os_get_executable_path_ptr(IOS_USB_EXE));
		//dstr_insert_ch(&cmd, 0, '\"');
		//dstr_cat(&cmd, "\" \"");
		//process = os_process_pipe_create(cmd.array, "w");
		//dstr_free(&cmd);
	}
}

ScreenMirrorServer::~ScreenMirrorServer()
{
	m_server.stop();
}

static void pipe_log(void *param, uint8_t *data, size_t size)
{
	struct ScreenMirrorServer *sm = (ScreenMirrorServer *)param;
	if (data && size)
		blog(LOG_INFO, "%s", data);
}

bool ScreenMirrorServer::init_pipe()
{
	char name[64];
	sprintf(name, "%s", PIPE_NAME);

	if (!ipc_pipe_server_start(&pipe, name, pipe_log, this)) {
		blog(LOG_WARNING, "init_pipe: failed to start pipe");
		return false;
	}

	return true;
}

void ScreenMirrorServer::outputVideo(SFgVideoFrame *data)
{
	lastPts = data->pts;
	if (data->pitch[0] != m_videoFrame.width ||
	    data->height != m_videoFrame.height) {

		m_cropFilter =
			obs_source_filter_get_by_name(m_source, "cropFilter");
		if (!m_cropFilter) {
			m_cropFilter = obs_source_create(
				"crop_filter", "cropFilter", nullptr, nullptr);
			obs_source_filter_add(m_source, m_cropFilter);
		}

		obs_data_t *setting = obs_source_get_settings(m_cropFilter);
		obs_data_set_int(setting, "right",
				 labs(data->pitch[0] - data->width));
		obs_source_update(m_cropFilter, setting);
		obs_data_release(setting);
	}
	m_videoFrame.timestamp = data->pts;
	m_videoFrame.width = data->pitch[0];
	m_videoFrame.height = data->height;
	m_videoFrame.format = VIDEO_FORMAT_I420;
	m_videoFrame.flip = false;
	m_videoFrame.flip_h = false;

	m_videoFrame.data[0] = data->data;
	m_videoFrame.data[1] =
		m_videoFrame.data[0] + m_videoFrame.width * m_videoFrame.height;
	m_videoFrame.data[2] = m_videoFrame.data[1] +
			       m_videoFrame.width * m_videoFrame.height / 4;

	m_videoFrame.linesize[0] = data->pitch[0];
	m_videoFrame.linesize[1] = data->pitch[1];
	m_videoFrame.linesize[2] = data->pitch[2];
	obs_source_output_video2(m_source, &m_videoFrame);
}

void ScreenMirrorServer::outputAudio(SFgAudioFrame *data)
{
	blog(LOG_INFO, "=================%lld", data->pts - lastPts);
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
	ScreenMirrorServer *server = new ScreenMirrorServer(source);
	return server;
}

static void DestroyWinAirplay(void *obj)
{
	delete reinterpret_cast<ScreenMirrorServer *>(obj);
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
