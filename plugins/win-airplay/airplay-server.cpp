#include <obs-module.h>
#include "airplay-server.h"
#include <cstdio>
#include "common-define.h"
#include <util/dstr.h>
#include <util/platform.h>

#define IOS_USB_EXE "ios-usb-mirror.exe"
uint8_t start_code[4] = {00, 00, 00, 01};

static uint32_t byteutils_get_int(unsigned char *b, int offset)
{
	return *((uint32_t *)(b + offset));
}

static uint32_t byteutils_get_int_be(unsigned char *b, int offset)
{
	return ntohl(byteutils_get_int(b, offset));
}

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

#ifdef DUMPFILE
	m_auioFile = fopen("audio.pcm", "wb");
	m_videoFile = fopen("video.x264", "wb");
#endif	

	circlebuf_init(&m_avBuffer);
	ipcSetup();
}

ScreenMirrorServer::~ScreenMirrorServer()
{
	ipcDestroy();
	circlebuf_free(&m_avBuffer);
	//m_server.stop();
#ifdef DUMPFILE
	fclose(m_auioFile);
	fclose(m_videoFile);
#endif
}

void ScreenMirrorServer::pipeCallback(void *param, uint8_t *data, size_t size)
{
	ScreenMirrorServer *sm = (ScreenMirrorServer *)param;
	circlebuf_push_back(&sm->m_avBuffer, data, size);
	
	while (true) {
		size_t header_size = sizeof(struct av_packet_info);
		if (sm->m_avBuffer.size < header_size)
			break;

		struct av_packet_info header_info = { 0 };
		circlebuf_peek_front(&sm->m_avBuffer, &header_info, header_size);

		size_t req_size = header_info.size;
		if (sm->m_avBuffer.size < req_size + header_size)
			break;

		circlebuf_pop_front(&sm->m_avBuffer, &header_info, header_size); // remove it

		if (header_info.type == FFM_MEDIA_INFO)
		{
			struct media_info info;
			memset(&info, 0, req_size);
			circlebuf_pop_front(&sm->m_avBuffer, &info, req_size);

			blog(LOG_INFO,
			     "recv media info, pps_len: %d, sps_len: %d", info.pps_len, info.sps_len);
#ifdef DUMPFILE
			sm->doWithNalu(info.pps, info.pps_len);
			sm->doWithNalu(info.sps, info.sps_len);
#endif // DUMPFILE
		}
		else
		{
			uint8_t *temp_buf = (uint8_t *)calloc(1, req_size);
			circlebuf_pop_front(&sm->m_avBuffer, temp_buf, req_size);

			if (header_info.type == FFM_PACKET_AUDIO)
			{
#ifdef DUMPFILE
				fwrite(temp_buf, 1, req_size, sm->m_auioFile);
#endif // DUMPFILE

			}
			else
			{
				sm->parseNalus(temp_buf, req_size);
			}

			free(temp_buf);
		}
	}
}

void ScreenMirrorServer::ipcSetup()
{
	if (!initPipe())
		blog(LOG_ERROR, "fail to create pipe");
	else {
		checkAndOpenUsbMirror();
	}
}

void ScreenMirrorServer::ipcDestroy()
{
	quitUsbMirror();
	ipc_pipe_server_free(&pipe);
}

void ScreenMirrorServer::checkAndOpenUsbMirror()
{
	os_kill_process(IOS_USB_EXE);
	struct dstr cmd;
	dstr_init_move_array(&cmd, os_get_executable_path_ptr(IOS_USB_EXE));
	dstr_insert_ch(&cmd, 0, '\"');
	dstr_cat(&cmd, "\" \"");
	process = os_process_pipe_create(cmd.array, "w");
	dstr_free(&cmd);
}

void ScreenMirrorServer::quitUsbMirror()
{
	uint8_t data[1] = {1};
	os_process_pipe_write(process, data, 1);
	os_process_pipe_destroy(process);
}

void ScreenMirrorServer::parseNalus(uint8_t *data, size_t size)
{
	uint8_t *slice = data;
	while (slice < data + size) {
		size_t length = byteutils_get_int_be(slice, 0);
		doWithNalu(slice + 4, length);
		slice += length + 4;
	}
}

void ScreenMirrorServer::doWithNalu(uint8_t *data, size_t size)
{
#ifdef DUMPFILE
	fwrite(start_code, 1, 4, m_videoFile);
	fwrite(data, 1, size, m_videoFile);
#endif // DUMPFILE

}

bool ScreenMirrorServer::initPipe()
{
	char name[64];
	sprintf(name, "%s", PIPE_NAME);
	memset(&pipe, 0, sizeof(ipc_pipe_server_t));
	if (!ipc_pipe_server_start(&pipe, name, pipeCallback, this)) {
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
