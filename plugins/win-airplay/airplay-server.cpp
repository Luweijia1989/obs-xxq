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

static enum speaker_layout convert_speaker_layout(DWORD layout, WORD channels)
{
	switch (layout) {
	case KSAUDIO_SPEAKER_2POINT1:
		return SPEAKERS_2POINT1;
	case KSAUDIO_SPEAKER_SURROUND:
		return SPEAKERS_4POINT0;
	case KSAUDIO_SPEAKER_4POINT1:
		return SPEAKERS_4POINT1;
	case KSAUDIO_SPEAKER_5POINT1:
		return SPEAKERS_5POINT1;
	case KSAUDIO_SPEAKER_7POINT1:
		return SPEAKERS_7POINT1;
	}

	return (enum speaker_layout)channels;
}

ScreenMirrorServer::ScreenMirrorServer(obs_source_t *source)
	: m_source(source),
	  if2((gs_image_file2_t *)bzalloc(sizeof(gs_image_file2_t)))
{
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

void ScreenMirrorServer::handleMirrorStatus()
{
	int status = -1;
	circlebuf_pop_front(&m_avBuffer, &status, sizeof(int));
	mirror_status = (obs_source_mirror_status)status;
	if (mirror_status == OBS_SOURCE_MIRROR_STOP)
	{
		client->Stop();
		resetState();
	}
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
	IMMDeviceEnumerator *immde = NULL;
	WAVEFORMATEX *wfex = NULL;
	bool success = false;
	UINT32 frames;
	HRESULT hr;

	/* ------------------------------------------ *
	 * Init device                                */

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), (void **)&immde);
	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to create IMMDeviceEnumerator: %08lX",
			__FUNCTION__, hr);
		return false;
	}

	hr = immde->GetDefaultAudioEndpoint(eRender, eConsole, &device);

	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to get device: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	/* ------------------------------------------ *
	 * Init client                                */

	hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
		NULL, (void **)&client);
	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to activate device: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	hr = client->GetMixFormat(&wfex);
	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to get mix format: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, wfex, NULL);
	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to initialize: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	/* ------------------------------------------ *
	 * Init resampler                             */
	WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wfex;

	to.samples_per_sec = (uint32_t)wfex->nSamplesPerSec;
	to.speakers =
		convert_speaker_layout(ext->dwChannelMask, wfex->nChannels);
	to.format = AUDIO_FORMAT_FLOAT;
	channels = wfex->nChannels;
	sample_rate = wfex->nSamplesPerSec;

	/* ------------------------------------------ *
	 * Init client                                */

	hr = client->GetBufferSize(&frames);
	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to get buffer size: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	hr = client->GetService(__uuidof(IAudioRenderClient), (void **)&render);
	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to get IAudioRenderClient: %08lX",
			__FUNCTION__, hr);
		goto fail;
	}

	hr = client->Start();
	if (FAILED(hr)) {
		blog(LOG_WARNING, "%s: Failed to start audio: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	success = true;

fail:
	immde->Release();
	if (wfex)
		CoTaskMemFree(wfex);
	return success;
}

void ScreenMirrorServer::destroyAudioRenderer()
{
	if (client)
		client->Stop();

	if (device)
		device->Release();
	if (client)
		client->Release();
	if (render)
		render->Release();

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

	HRESULT hr = render->GetBuffer(resample_frames, &output);
	if (FAILED(hr)) {
		return;
	}

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
		memcpy(output, resample_data[0],
		       resample_frames * channels * sizeof(float));
	}

	render->ReleaseBuffer(resample_frames,
			      muted ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
}

void ScreenMirrorServer::outputVideoFrame(AVFrame *frame)
{
	if (frame->linesize[0] != m_videoFrame.width ||
	    frame->height != m_videoFrame.height) {
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
				 labs(frame->linesize[0] - frame->width));
		obs_source_update(m_cropFilter, setting);
		obs_data_release(setting);
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
	if (mirror_status != OBS_SOURCE_MIRROR_OUTPUT)
		mirror_status = OBS_SOURCE_MIRROR_OUTPUT;
	//obs_source_output_video2(m_source, &m_videoFrame);
	obs_source_set_videoframe(m_source, &m_videoFrame);
}

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
	if (s->mirror_status == OBS_SOURCE_MIRROR_STOP) {
		s->renderImage(effect);
	} else
		obs_source_draw_videoframe(s->m_source);
}
static uint32_t WinAirplayWidth(void *data)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	int width = s->mirror_status == OBS_SOURCE_MIRROR_OUTPUT
			    ? s->m_width
			    : s->if2->image.cx;
	return width;
}
static uint32_t WinAirplayHeight(void *data)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	int height = s->mirror_status == OBS_SOURCE_MIRROR_OUTPUT
			     ? s->m_height
			     : s->if2->image.cy;
	return height;
}
void  ScreenMirrorServer::WinAirplayVideoTick(void *data, float seconds)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	pthread_mutex_lock(&s->m_dataMutex);
	if (s->m_videoFrames.size() > 0) {
		AVFrame *frame = s->m_videoFrames.front();
		if (s->m_offset == LLONG_MAX)
			s->m_offset = os_gettime_ns() - frame->pts+s->m_extraDelay;

		if (s->m_offset + frame->pts <= os_gettime_ns()) {
			s->outputVideoFrame(frame);
			s->m_videoFrames.pop_front();

			av_frame_free(&frame);
		}
	}

	if (s->m_audioFrames.size() > 0) {
		AudioFrame *frame = s->m_audioFrames.front();
		if (s->m_audioOffset == LLONG_MAX)
			s->m_audioOffset = os_gettime_ns() - frame->pts + s->m_extraDelay;

		if (s->m_audioOffset + frame->pts <= os_gettime_ns()) {
			s->outputAudioFrame(frame->data, frame->data_len);
			s->m_audioFrames.pop_front();
			free(frame->data);
		}
	}
	pthread_mutex_unlock(&s->m_dataMutex);
	
}
bool obs_module_load(void)
{
	obs_source_info info = {};
	info.id = "win_airplay";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO 
			     | OBS_SOURCE_DO_NOT_DUPLICATE;
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
