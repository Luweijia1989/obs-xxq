#include <obs-module.h>
#include "airplay-server.h"
#include <cstdio>
#include <util/dstr.h>

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
	m_cropFilter = obs_source_filter_get_by_name(m_source, "cropFilter");
	if (!m_cropFilter) {
		m_cropFilter = obs_source_create("crop_filter", "cropFilter",
						 nullptr, nullptr);
		obs_source_filter_add(m_source, m_cropFilter);
		obs_source_release(m_cropFilter);
	}

	initAudioRenderer();

	pthread_mutex_init(&m_typeChangeMutex, nullptr);
	pthread_mutex_init(&m_dataMutex, nullptr);
	memset(m_videoFrame.data, 0, sizeof(m_videoFrame.data));
	memset(&m_videoFrame, 0, sizeof(&m_videoFrame));

	m_videoFrame.range = VIDEO_RANGE_PARTIAL;
	video_format_get_parameters(VIDEO_CS_601, m_videoFrame.range,
				    m_videoFrame.color_matrix,
				    m_videoFrame.color_range_min,
				    m_videoFrame.color_range_max);
	memcpy(&m_imageFrame, &m_videoFrame, sizeof(struct obs_source_frame2));

#ifdef DUMPFILE
	m_auioFile = fopen("audio.pcm", "wb");
	m_videoFile = fopen("video.x264", "wb");
#endif

	circlebuf_init(&m_avBuffer);
	ipcSetup();

	updateStatusImage();
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
	pthread_mutex_destroy(&m_dataMutex);

	destroyAudioRenderer();
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

	resetState();
}

const char *ScreenMirrorServer::killProc()
{
	const char *processName = nullptr;
	if (m_backend == IOS_USB_CABLE) {
		processName = DRIVER_EXE;
	} else if (m_backend == IOS_AIRPLAY) {
		processName = AIRPLAY_EXE;
	} else if (m_backend == ANDROID_USB_CABLE)
		processName = ANDROID_USB_EXE;

	os_kill_process(processName);
	return processName;
}

static video_format gs_format_to_video_format(gs_color_format format)
{
	if (format == GS_RGBA)
		return VIDEO_FORMAT_RGBA;
	else if (format == GS_BGRA)
		return VIDEO_FORMAT_BGRA;
	else if (format == GS_BGRX)
		return VIDEO_FORMAT_BGRX;

	return VIDEO_FORMAT_NONE;
}

void ScreenMirrorServer::updateStatusImage()
{
	updateCropFilter(0, 0);
	const char *path = nullptr;
	switch (mirror_status)
	{
	case OBS_SOURCE_MIRROR_START:
		path = "E:\\test.gif";
		break;
	case OBS_SOURCE_MIRROR_STOP:
		path = "E:\\test.jpg";
		break;
	case OBS_SOURCE_MIRROR_DEVICE_LOST: // 连接失败，检测超时
		path = "E:\\test1.jpg";
		break;
	default:
		break;
	}
	loadImage(path);

	updateImageTexture();
}

void ScreenMirrorServer::updateImageTexture()
{
	m_imageFrame.timestamp = 0;
	m_imageFrame.width = if2->image.is_animated_gif ? if2->image.gif.width : if2->image.cx;
	m_imageFrame.height = if2->image.is_animated_gif ? if2->image.gif.height : if2->image.cy;
	m_imageFrame.format = gs_format_to_video_format(if2->image.format);
	m_imageFrame.flip = false;
	m_imageFrame.flip_h = false;

	m_imageFrame.data[0] = if2->image.is_animated_gif ? if2->image.animation_frame_cache[if2->image.cur_frame] : if2->image.texture_data;
	m_imageFrame.data[1] = NULL;

	m_imageFrame.data[2] = NULL;

	m_imageFrame.linesize[0] = if2->image.is_animated_gif ? if2->image.gif.width*4 : if2->image.cx * 4;
	m_imageFrame.linesize[1] = 0;
	m_imageFrame.linesize[2] = 0;
	m_width = m_imageFrame.width;
	m_height = m_imageFrame.height;
	obs_source_set_videoframe(m_source, &m_imageFrame);
}

void ScreenMirrorServer::updateCropFilter(int lineSize, int frameWidth)
{
	if (!m_cropFilter)
		return;
	obs_data_t *setting = obs_source_get_settings(m_cropFilter);
	obs_data_set_int(setting, "right",
			 labs(lineSize - frameWidth));
	obs_source_update(m_cropFilter, setting);
	obs_data_release(setting);
}

void ScreenMirrorServer::setBackendType(int type)
{
	m_backend = (MirrorBackEnd)type;
	if (m_backend == IOS_AIRPLAY)
		m_extraDelay = 100000000;
	else
		m_extraDelay = 0;
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
	}
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

void ScreenMirrorServer::handleMirrorStatus()
{
	int status = -1;
	circlebuf_pop_front(&m_avBuffer, &status, sizeof(int));
	mirror_status = (obs_source_mirror_status)status;
	if (mirror_status == OBS_SOURCE_MIRROR_STOP)
	{
		resetState();
	}
	updateStatusImage();
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
		resetResampler(info.speakers, info.format, info.samples_per_sec);
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
				outputAudio(temp_buf, req_size, header_info.pts);
			}
		} else {
			AVFrame *frame = m_decoder.docode(temp_buf, req_size, false,
						   header_info.pts);
			if (frame)
				outputVideo(frame);
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
		resetResampler(info.speakers, info.format, info.samples_per_sec);
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
				outputAudio(temp_buf, req_size, header_info.pts);
			}
		} else {
			uint8_t *all = NULL;
			size_t all_len = 0;
			parseNalus(temp_buf, req_size, &all, &all_len);
			if (all_len) {
				AVFrame *frame = m_decoder.docode(all, all_len, false,
							   header_info.pts);
				if (frame)
					outputVideo(frame);
			}
			free(all);
		}

		free(temp_buf);
	}
	return true;
}

void ScreenMirrorServer::resetState()
{
	pthread_mutex_lock(&m_dataMutex);
	m_offset = LLONG_MAX;
	m_audioOffset = LLONG_MAX;
	for (auto iter = m_videoFrames.begin(); iter != m_videoFrames.end();
	     iter++) {
		AVFrame *f = *iter;
		av_frame_free(&f);
	}
	m_videoFrames.clear();
	for (auto iter = m_audioFrames.begin(); iter != m_audioFrames.end();
	     iter++) {
		AudioFrame *f = *iter;
		free(f->data);
	}
	m_audioFrames.clear();
	pthread_mutex_unlock(&m_dataMutex);
}

bool ScreenMirrorServer::initAudioRenderer()
{
	memset(&from, 0, sizeof(struct resample_info));
	PaError err = Pa_Initialize();
	if (err != paNoError) goto error;	

	err = Pa_OpenDefaultStream(&pa_stream_, 0, 2, paInt16, 48000, 2048, nullptr, nullptr);
	if (err != paNoError) goto error;

	err = Pa_StartStream(pa_stream_);
	if (err != paNoError) goto error;

	to.samples_per_sec = sample_rate = 48000;
	to.format = AUDIO_FORMAT_16BIT;
	to.speakers = SPEAKERS_STEREO;
	channels = 2;

	return err;
error:
	Pa_Terminate();
	return err;

}

void ScreenMirrorServer::destroyAudioRenderer()
{
	if (pa_stream_)
	{
		Pa_StopStream(pa_stream_);
		Pa_CloseStream(pa_stream_);
		Pa_Terminate();
	}

	audio_resampler_destroy(resampler);
}

void ScreenMirrorServer::resetResampler(enum speaker_layout speakers,
					enum audio_format format,
					uint32_t samples_per_sec)
{
	if (from.format == format && from.speakers == speakers && from.samples_per_sec == samples_per_sec && resampler)
		return;

	if (resampler)
	{
		audio_resampler_destroy(resampler);
		resampler = nullptr;
	}

	from.samples_per_sec = samples_per_sec;
	from.speakers = speakers;
	from.format = format;
	resampler = audio_resampler_create(&to, &from);
}

bool ScreenMirrorServer::initPipe()
{
	ipc_server_create(&m_ipcServer, ScreenMirrorServer::pipeCallback, this);
	return true;
}

void ScreenMirrorServer::outputAudioFrame(uint8_t *data, size_t size)
{
	uint8_t *resample_data[MAX_AV_PLANES];
	uint8_t *input[MAX_AV_PLANES] = {data};
	uint32_t resample_frames;
	uint64_t ts_offset;
	bool success;
	BYTE *output;

	success = audio_resampler_resample(
		resampler, resample_data, &resample_frames, &ts_offset,
		(const uint8_t *const *)input,
		(uint32_t)(get_audio_frames(from.format, from.speakers, size)));
	if (!success) {
		return;
	}

	auto writeableFrames = Pa_GetStreamWriteAvailable(pa_stream_);

	if (resample_frames > writeableFrames)
		return;

	bool muted = false;
	if (!muted) {
		/* apply volume */
		//if (!close_float(vol, 1.0f, EPSILON)) {
		//	register float *cur = (float *)resample_data[0];
		//	register float *end =
		//		cur + resample_frames * monitor->channels;

		//	while (cur < end)
		//		*(cur++) *= vol;
		//}
		Pa_WriteStream(pa_stream_, resample_data[0], resample_frames);
	}
}

void ScreenMirrorServer::outputVideoFrame(AVFrame *frame)
{
	if (mirror_status != OBS_SOURCE_MIRROR_OUTPUT)
		mirror_status = OBS_SOURCE_MIRROR_OUTPUT;

	if (frame->linesize[0] != m_videoFrame.width ||
	    frame->height != m_videoFrame.height) {
		updateCropFilter(frame->linesize[0], frame->width);
	}
	m_videoFrame.timestamp = frame->pts;
	m_videoFrame.width = frame->linesize[0];
	m_videoFrame.height = frame->height;
	m_videoFrame.format = VIDEO_FORMAT_I420;
	m_videoFrame.flip = false;
	m_videoFrame.flip_h = false;

	m_videoFrame.data[0] = frame->data[0];
	m_videoFrame.data[1] =	frame->data[1];
		
	m_videoFrame.data[2] = frame->data[2];

	m_videoFrame.linesize[0] = frame->linesize[0];
	m_videoFrame.linesize[1] = frame->linesize[1];
	m_videoFrame.linesize[2] = frame->linesize[2];
	m_width = m_videoFrame.width;
	m_height = m_videoFrame.height;
	obs_source_set_videoframe(m_source, &m_videoFrame);
}

//static int64_t dddd = 0;
void ScreenMirrorServer::outputAudio(uint8_t *data, size_t data_len, uint64_t pts)
{
	pthread_mutex_lock(&m_dataMutex);
	auto frame = new AudioFrame;
	frame->data = (uint8_t *)malloc(data_len);
	memcpy(frame->data, data, data_len);
	frame->data_len = data_len;
	frame->pts = pts;
	m_audioFrames.push_back(frame);
	pthread_mutex_unlock(&m_dataMutex);
	//blog(LOG_INFO,"%lld", (frame->pts-dddd)/1000000);
	//dddd = frame->pts;
}

void ScreenMirrorServer::outputVideo(AVFrame *frame)
{
	pthread_mutex_lock(&m_dataMutex);
	m_videoFrames.push_back(frame);
	pthread_mutex_unlock(&m_dataMutex);
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
	auto type = obs_data_get_int(settings, "type");
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
static void WinAirplayRender(void *data, gs_effect_t *effect)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	obs_source_draw_videoframe(s->m_source);
}
static uint32_t WinAirplayWidth(void *data)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	return s->m_width;
}
static uint32_t WinAirplayHeight(void *data)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	return s->m_height;
}

void  ScreenMirrorServer::WinAirplayVideoTick(void *data, float seconds)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;

	if (s->mirror_status != OBS_SOURCE_MIRROR_OUTPUT)
	{
		uint64_t frame_time = obs_get_video_frame_time();
		if (s->last_time && s->if2->image.is_animated_gif) {
			uint64_t elapsed = frame_time - s->last_time;
			bool updated = gs_image_file2_tick(s->if2, elapsed);

			if (updated) {
				s->updateImageTexture();
			}
		}
		s->last_time = frame_time;
	}

	pthread_mutex_lock(&s->m_dataMutex);
	while (s->m_videoFrames.size() > 0) {
		AVFrame *frame = s->m_videoFrames.front();
		int64_t now = (int64_t)os_gettime_ns();
		if (s->m_offset == LLONG_MAX)
			s->m_offset = now - frame->pts+s->m_extraDelay;

		if (s->m_offset + frame->pts <= now) {
			s->outputVideoFrame(frame);
			s->m_videoFrames.pop_front();

			av_frame_free(&frame);
		}
		else
			break;
	}

	while (s->m_audioFrames.size() > 0) {
		AudioFrame *frame = s->m_audioFrames.front();
		int64_t now = (int64_t)os_gettime_ns();
		if (s->m_audioOffset == LLONG_MAX )
			s->m_audioOffset = now - frame->pts + s->m_extraDelay;

		if (s->m_audioOffset + frame->pts <= now) {
			s->outputAudioFrame(frame->data, frame->data_len);
			s->m_audioFrames.pop_front();
			free(frame->data);
		}
		else
			break;
	}
	pthread_mutex_unlock(&s->m_dataMutex);
	
}

bool obs_module_load(void)
{
	obs_source_info info = {};
	info.id = "win_airplay";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.show = ShowWinAirplay;
	info.hide = HideWinAirplay;
	info.get_name = GetWinAirplayName;
	info.create = CreateWinAirplay;
	info.destroy = DestroyWinAirplay;
	info.update = UpdateWinAirplaySource;
	info.get_defaults = GetWinAirplayDefaultsOutput;
	info.get_properties = GetWinAirplayPropertiesOutput;
	info.video_render = WinAirplayRender;
	info.get_width = WinAirplayWidth;
	info.get_height = WinAirplayHeight;
	info.video_tick = ScreenMirrorServer::WinAirplayVideoTick;
	obs_register_source(&info);

	return true;
}

void obs_module_unload(void) {}
