#include <obs-module.h>
#include "airplay-server.h"
#include <cstdio>
#include <util/dstr.h>
#include <util/platform.h>

#define DRIVER_EXE "driver-tool.exe"
#define AIRPLAY_EXE "airplay-server.exe"
#define ANDROID_USB_EXE "android-usb-mirror.exe"
uint8_t start_code[4] = {00, 00, 00, 01};

static uint32_t byteutils_get_int(unsigned char *b, int offset)
{
	return *((uint32_t *)(b + offset));
}

static uint32_t byteutils_get_int_be(unsigned char *b, int offset)
{
	return ntohl(byteutils_get_int(b, offset));
}

ScreenMirrorServer::ScreenMirrorServer(obs_source_t *source)
	: m_source(source),
	  if2((gs_image_file2_t *)bzalloc(sizeof(gs_image_file2_t)))
{
	pthread_mutex_init(&m_typeChangeMutex, nullptr);
	memset(m_videoFrame.data, 0, sizeof(m_videoFrame.data));
	memset(&m_audioFrame, 0, sizeof(&m_audioFrame));
	memset(&m_videoFrame, 0, sizeof(&m_videoFrame));

	m_videoFrame.range = VIDEO_RANGE_PARTIAL;
	video_format_get_parameters(VIDEO_CS_601, m_videoFrame.range,
				    m_videoFrame.color_matrix,
				    m_videoFrame.color_range_min,
				    m_videoFrame.color_range_max);

#ifdef DUMPFILE
	m_auioFile = fopen("audio.pcm", "wb");
	m_videoFile = fopen("video.x264", "wb");
#endif

	circlebuf_init(&m_avBuffer);
	ipcSetup();

	loadImage("E:\\test1.jpg");
}

ScreenMirrorServer::~ScreenMirrorServer()
{
	obs_enter_graphics();
	gs_image_file2_free(if2);
	obs_leave_graphics();
	bfree(if2);

	ipcDestroy();
	circlebuf_free(&m_avBuffer);
	pthread_mutex_destroy(&m_typeChangeMutex);
#ifdef DUMPFILE
	fclose(m_auioFile);
	fclose(m_videoFile);
#endif
}

void ScreenMirrorServer::pipeCallback(void *param, uint8_t *data, size_t size)
{
	ScreenMirrorServer *sm = (ScreenMirrorServer *)param;
	pthread_mutex_lock(&sm->m_typeChangeMutex);
	circlebuf_push_back(&sm->m_avBuffer, data, size);

	while (true) {
		if (sm->m_backend == IOS_USB_CABLE) {
			bool b = sm->handleUSBData();
			if (b)
				continue;
			else
				break;
		} else if (sm->m_backend == IOS_AIRPLAY ||
			   sm->m_backend == ANDROID_USB_CABLE) {
			bool b = sm->handleAirplayData();
			if (b)
				continue;
			else
				break;
		}
	}
	pthread_mutex_unlock(&sm->m_typeChangeMutex);
}

void ScreenMirrorServer::ipcSetup()
{
	if (!initPipe()) {
		blog(LOG_ERROR, "fail to create pipe");
		return;
	}
}

void ScreenMirrorServer::ipcDestroy()
{
	mirrorServerDestroy();
	ipc_server_destroy(&m_ipcServer);
}

void ScreenMirrorServer::mirrorServerSetup()
{
	if (process)
		return;

	const char *processName = killProc();
	struct dstr cmd;
	dstr_init_move_array(&cmd, os_get_executable_path_ptr(processName));
	dstr_insert_ch(&cmd, 0, '\"');
	dstr_cat(&cmd, "\" \"");
	process = os_process_pipe_create(cmd.array, "w");
	dstr_free(&cmd);
}

void ScreenMirrorServer::mirrorServerDestroy()
{
	killProc();
	if (process) {
		uint8_t data[1] = {1};
		os_process_pipe_write(process, data, 1);
		os_process_pipe_destroy(process);
		process = NULL;
	}

	circlebuf_free(&m_avBuffer);
	circlebuf_init(&m_avBuffer);
}

void ScreenMirrorServer::setBackendType(int type)
{
	m_backend = (MirrorBackEnd)type;
}

int ScreenMirrorServer::backendType()
{
	return m_backend;
}

void ScreenMirrorServer::loadImage(const char *path)
{
	obs_enter_graphics();
	gs_image_file2_free(if2);
	obs_leave_graphics();

	if (path) {
		gs_image_file2_init(if2, path);

		obs_enter_graphics();
		gs_image_file2_init_texture(if2);
		obs_leave_graphics();

		if (!if2->image.loaded)
			blog(LOG_ERROR, "failed to load texture '%s'", path);
	}
}

void ScreenMirrorServer::renderImage(gs_effect_t *effect)
{
	if (!if2->image.texture)
		return;

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			      if2->image.texture);
	gs_draw_sprite(if2->image.texture, 0, if2->image.cx, if2->image.cy);
}

void ScreenMirrorServer::parseNalus(uint8_t *data, size_t size, uint8_t **out,
				    size_t *out_len)
{
	int total_size = 0;
	uint8_t *slice = data;
	while (slice < data + size) {
		size_t length = byteutils_get_int_be(slice, 0);
		total_size += length + 4;
		slice += length + 4;
	}

	if (total_size) {
		*out = (uint8_t *)calloc(1, total_size);
		uint8_t *ptr = *out;
		*out_len = total_size;
		slice = data;
		size_t copy_index = 0;
		while (slice < data + size) {
			memcpy(ptr + copy_index, start_code, 4);
			copy_index += 4;
			size_t length = byteutils_get_int_be(slice, 0);
			total_size += length + 4;
			memcpy(ptr + copy_index, slice + 4, length);
			copy_index += length;
			slice += length + 4;
		}
	}
}

void ScreenMirrorServer::doWithNalu(uint8_t *data, size_t size)
{
#ifdef DUMPFILE
	fwrite(start_code, 1, 4, m_videoFile);
	fwrite(data, 1, size, m_videoFile);
#endif // DUMPFILE
}

void ScreenMirrorServer::handleMirrorStatus()
{
	int status = -1;
	circlebuf_pop_front(&m_avBuffer, &status, sizeof(int));
	mirror_status = (obs_source_mirror_status)status;
}

bool ScreenMirrorServer::handleAirplayData()
{
	size_t header_size = sizeof(struct av_packet_info);
	if (m_avBuffer.size < header_size)
		return false;

	struct av_packet_info header_info = {0};
	circlebuf_peek_front(&m_avBuffer, &header_info, header_size);

	size_t req_size = header_info.size;
	if (m_avBuffer.size < req_size + header_size)
		return false;

	circlebuf_pop_front(&m_avBuffer, &header_info,
			    header_size); // remove it

	if (header_info.type == FFM_MIRROR_STATUS)
		handleMirrorStatus();
	else if (header_info.type == FFM_MEDIA_INFO) {
		struct media_info info;
		memset(&info, 0, req_size);
		circlebuf_pop_front(&m_avBuffer, &info, req_size);

		blog(LOG_INFO, "recv media info, pps_len: %d, sps_len: %d",
		     info.pps_len, info.sps_len);

		if (info.sps_len == 0 && info.pps_len == 0)
			return true;

		m_mediaInfo = info;
		if (!m_infoReceived)
			m_infoReceived = true;
#ifdef DUMPFILE
		fwrite(info.pps, info.pps_len, 1, m_videoFile);
#endif // DUMPFILE
		m_decoder.docode(info.pps, info.pps_len, true, 0);
	} else {
		uint8_t *temp_buf = (uint8_t *)calloc(1, req_size);
		circlebuf_pop_front(&m_avBuffer, temp_buf, req_size);

		if (header_info.type == FFM_PACKET_AUDIO) {
#ifdef DUMPFILE
			fwrite(temp_buf, 1, req_size, m_auioFile);
#endif // DUMPFILE
			if (m_infoReceived) {
				SFgAudioFrame *frame = new SFgAudioFrame();
				frame->bitsPerSample =
					m_mediaInfo.bytes_per_frame;
				frame->channels = m_mediaInfo.speakers;
				frame->pts = header_info.pts;
				frame->sampleRate = m_mediaInfo.samples_per_sec;
				frame->dataLen = req_size;
				frame->data = temp_buf;
				outputAudio(frame);
				delete frame;
			}
		} else {
			int res = m_decoder.docode(temp_buf, req_size, false,
						   header_info.pts);
			if (res == 1)
				outputVideo(&m_decoder.m_sVideoFrameOri);
		}

		free(temp_buf);
	}
	return true;
}

bool ScreenMirrorServer::handleUSBData()
{
	size_t header_size = sizeof(struct av_packet_info);
	if (m_avBuffer.size < header_size)
		return false;

	struct av_packet_info header_info = {0};
	circlebuf_peek_front(&m_avBuffer, &header_info, header_size);

	size_t req_size = header_info.size;
	if (m_avBuffer.size < req_size + header_size)
		return false;

	circlebuf_pop_front(&m_avBuffer, &header_info,
			    header_size); // remove it

	if (header_info.type == FFM_MIRROR_STATUS)
		handleMirrorStatus();
	else if (header_info.type == FFM_MEDIA_INFO) {
		struct media_info info;
		memset(&info, 0, req_size);
		circlebuf_pop_front(&m_avBuffer, &info, req_size);

		blog(LOG_INFO, "recv media info, pps_len: %d, sps_len: %d",
		     info.pps_len, info.sps_len);

		if (info.sps_len == 0 || info.pps_len == 0)
			return true;

		m_mediaInfo = info;
		if (!m_infoReceived)
			m_infoReceived = true;
#ifdef DUMPFILE
		doWithNalu(info.pps, info.pps_len);
		doWithNalu(info.sps, info.sps_len);
#endif // DUMPFILE
		uint8_t *temp_buff =
			(uint8_t *)calloc(1, info.sps_len + info.pps_len + 8);
		memcpy(temp_buff, start_code, 4);
		memcpy(temp_buff + 4, info.pps, info.pps_len);
		memcpy(temp_buff + 4 + info.pps_len, start_code, 4);
		memcpy(temp_buff + 4 + info.pps_len + 4, info.sps,
		       info.sps_len);
		m_decoder.docode(temp_buff, info.sps_len + info.pps_len + 8,
				 true, 0);
		free(temp_buff);
	} else {
		uint8_t *temp_buf = (uint8_t *)calloc(1, req_size);
		circlebuf_pop_front(&m_avBuffer, temp_buf, req_size);

		if (header_info.type == FFM_PACKET_AUDIO) {
#ifdef DUMPFILE
			fwrite(temp_buf, 1, req_size, m_auioFile);
#endif // DUMPFILE
			if (m_infoReceived) {
				SFgAudioFrame *frame = new SFgAudioFrame();
				frame->bitsPerSample =
					m_mediaInfo.bytes_per_frame;
				frame->channels = m_mediaInfo.speakers;
				frame->pts = header_info.pts;
				frame->sampleRate = m_mediaInfo.samples_per_sec;
				frame->dataLen = req_size;
				frame->data = temp_buf;
				outputAudio(frame);
				delete frame;
			}
		} else {
			uint8_t *all = NULL;
			size_t all_len = 0;
			parseNalus(temp_buf, req_size, &all, &all_len);
			if (all_len) {
				int res = m_decoder.docode(all, all_len, false,
							   header_info.pts);
				if (res == 1)
					outputVideo(
						&m_decoder.m_sVideoFrameOri);
			}
			free(all);
		}

		free(temp_buf);
	}
	return true;
}

const char *ScreenMirrorServer::killProc()
{
	const char *processName = nullptr;
	if (m_backend == IOS_USB_CABLE) {
		processName = DRIVER_EXE;
	} else if (m_backend == IOS_AIRPLAY) {
		processName = AIRPLAY_EXE;
	} else if (m_backend = ANDROID_USB_CABLE)
		processName = ANDROID_USB_EXE;

	os_kill_process(processName);
	return processName;
}

bool ScreenMirrorServer::initPipe()
{
	ipc_server_create(&m_ipcServer, ScreenMirrorServer::pipeCallback, this);
	return true;
}

void ScreenMirrorServer::outputVideo(SFgVideoFrame *data)
{
	//blog(LOG_INFO, "*****%lld", data->pts);
	lastPts = data->pts;
	if (data->pitch[0] != m_videoFrame.width ||
	    data->height != m_videoFrame.height) {
		m_cropFilter =
			obs_source_filter_get_by_name(m_source, "cropFilter");
		if (!m_cropFilter) {
			m_cropFilter = obs_source_create(
				"crop_filter", "cropFilter", nullptr, nullptr);
			obs_source_filter_add(m_source, m_cropFilter);
			obs_source_release(m_cropFilter);
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
	m_width = m_videoFrame.width;
	m_height = m_videoFrame.height;
	if (mirror_status != OBS_SOURCE_MIRROR_OUTPUT)
		mirror_status = OBS_SOURCE_MIRROR_OUTPUT;
	obs_source_output_video2(m_source, &m_videoFrame);
}

void ScreenMirrorServer::outputAudio(SFgAudioFrame *data)
{
	blog(LOG_INFO, "=====%lld", data->pts-lastPts);
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

static void UpdateWinAirplaySource(void *obj, obs_data_t *settings)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)obj;
	int type = obs_data_get_int(settings, "type");
	if (type == s->backendType())
		return;

	pthread_mutex_lock(&s->m_typeChangeMutex);
	s->mirrorServerDestroy();
	s->setBackendType(type);
	s->mirrorServerSetup();
	pthread_mutex_unlock(&s->m_typeChangeMutex);
}

static void GetWinAirplayDefaultsOutput(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "type",
				 ScreenMirrorServer::IOS_USB_CABLE);
}

static obs_properties_t *GetWinAirplayPropertiesOutput(void *data)
{
	if (!data)
		return nullptr;
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_int(props, "type", u8"投屏类型", 0, 2, 1);
	return props;
}

static void *CreateWinAirplay(obs_data_t *settings, obs_source_t *source)
{
	ScreenMirrorServer *server = new ScreenMirrorServer(source);
	UpdateWinAirplaySource(server, settings);
	return server;
}

static void DestroyWinAirplay(void *obj)
{
	delete reinterpret_cast<ScreenMirrorServer *>(obj);
}

static void HideWinAirplay(void *data) {}

static void ShowWinAirplay(void *data) {}

bool obs_module_load(void)
{
	obs_source_info info = {};
	info.id = "win_airplay";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC |
			    OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
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
