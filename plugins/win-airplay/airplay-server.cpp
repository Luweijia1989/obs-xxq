#include <obs-module.h>
#include "airplay-server.h"
#include "resource.h"
#include <cstdio>
#include <util/dstr.h>
#include <Shlwapi.h>

using namespace std;

#define DRIVER_EXE "driver-tool.exe"
#define AIRPLAY_EXE "airplay-server.exe"
#define ANDROID_USB_EXE "android-usb-mirror.exe"
#define INSTANCE_LOCK L"AIRPLAY-ONE-INSTANCE"
uint8_t start_code[4] = {00, 00, 00, 01};

HMODULE DllHandle;

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
	dumpResourceImgs();
	initAudioRenderer();

	pthread_mutex_init(&m_videoDataMutex, nullptr);
	pthread_mutex_init(&m_audioDataMutex, nullptr);
	pthread_mutex_init(&m_statusMutex, nullptr);

	circlebuf_init(&m_avBuffer);
	circlebuf_init(&m_audioFrames);

	saveStatusSettings();

	m_renderer = new DXVA2Renderer;
	m_renderer->Init();
}

ScreenMirrorServer::~ScreenMirrorServer()
{
	mirrorServerDestroy();

	destroyAudioRenderer();

	obs_enter_graphics();
	gs_image_file2_free(if2);
	if (m_renderTexture)
		gs_texture_destroy(m_renderTexture);
	obs_leave_graphics();
	bfree(if2);

	circlebuf_free(&m_avBuffer);
	circlebuf_free(&m_audioFrames);
	pthread_mutex_destroy(&m_videoDataMutex);
	pthread_mutex_destroy(&m_audioDataMutex);
	pthread_mutex_destroy(&m_statusMutex);

	if (m_audioCacheBuffer)
		free(m_audioCacheBuffer);

	if (m_handler != INVALID_HANDLE_VALUE)
		CloseHandle(m_handler);

	av_frame_free(&m_decodedFrame);

	if (m_decoder)
		delete m_decoder;
	if (m_renderer)
		delete m_renderer;

#ifdef DUMPFILE
	fclose(m_auioFile);
	fclose(m_videoFile);
#endif
}

void ScreenMirrorServer::dumpResourceImgs()
{
	string prefix;
	prefix.resize(MAX_PATH);
	DWORD len = GetTempPathA(MAX_PATH, (char *)prefix.data());
	prefix.resize(len);
	m_resourceImgs.push_back(prefix + "pic_mirror_connecting.gif");
	m_resourceImgs.push_back(prefix + "pic_android_cableprojection2.png");
	m_resourceImgs.push_back(prefix + "pic_android_screencastfailed_cableprojection.png");
	m_resourceImgs.push_back(prefix + "pic_ios_cableprojection.png");
	m_resourceImgs.push_back(prefix + "pic_ios_screencastfailed_cableprojection.png");
	m_resourceImgs.push_back(prefix + "pic_ios_wirelessprojection.png");
	m_resourceImgs.push_back(prefix + "pic_ios_screencastfailed_wirelessprojection.png");

	std::vector<int> ids = { IDB_PNG1, IDB_PNG2, IDB_PNG3, IDB_PNG4, IDB_PNG5, IDB_PNG6 };

	for (auto iter = 0; iter < m_resourceImgs.size(); iter++)
	{
		const string &img = m_resourceImgs.at(iter);
		if (!PathFileExistsA(img.c_str()))
		{
			HANDLE hFile = CreateFileA(img.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				HRSRC res = NULL;
				if (iter == 0)
				{
					res = FindResource(DllHandle, MAKEINTRESOURCE(IDB_BITMAP1), L"GIF");
				}
				else
				{
					res = FindResource(DllHandle, MAKEINTRESOURCE(ids[iter-1]), L"PNG");
				}
				auto g  = GetLastError();
				HGLOBAL res_handle = LoadResource(DllHandle, res);
				auto res_data = LockResource(res_handle);
				auto res_size = SizeofResource(DllHandle, res);

				DWORD byteWritten = 0;
				WriteFile(hFile, res_data, res_size, &byteWritten, NULL);
				CloseHandle(hFile);
			}
		}
	}
}

void ScreenMirrorServer::pipeCallback(void *param, uint8_t *data, size_t size)
{
	ScreenMirrorServer *sm = (ScreenMirrorServer *)param;
	circlebuf_push_back(&sm->m_avBuffer, data, size);

	while (true) {
		bool b = sm->handleMediaData();
		if (b)
			continue;
		else
			break;
	}
}

void ScreenMirrorServer::mirrorServerSetup()
{
	if (process)
		return;

	const char *processName = killProc();

	ipc_server_create(&m_ipcServer, ScreenMirrorServer::pipeCallback, this);

	struct dstr cmd;
	dstr_init_move_array(&cmd, os_get_executable_path_ptr(processName));
	dstr_insert_ch(&cmd, 0, '\"');
	dstr_cat(&cmd, "\" \"");
	process = os_process_pipe_create(cmd.array, "w");
	dstr_free(&cmd);
}

void ScreenMirrorServer::mirrorServerDestroy()
{
	if (process) {
		uint8_t data[1] = {1};
		os_process_pipe_write(process, data, 1);
		os_process_pipe_destroy_timeout(process, 1500);
		process = NULL;
	}

	if (m_ipcServer)
		ipc_server_destroy(&m_ipcServer);

	circlebuf_free(&m_avBuffer);
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

void ScreenMirrorServer::updateStatusImage()
{
	if (mirror_status == OBS_SOURCE_MIRROR_OUTPUT)
		return;
	const char *path = nullptr;
	switch (mirror_status) {
	case OBS_SOURCE_MIRROR_START:
		path = m_resourceImgs[0].c_str();
		break;
	case OBS_SOURCE_MIRROR_STOP:
		if (m_backend == IOS_USB_CABLE)
			path = m_resourceImgs[3].c_str();
		else if (m_backend == IOS_AIRPLAY)
			path = m_resourceImgs[5].c_str();
		else if (m_backend == ANDROID_USB_CABLE)
			path = m_resourceImgs[1].c_str();
		break;
	case OBS_SOURCE_MIRROR_DEVICE_LOST: // 连接失败，检测超时
		if (m_backend == IOS_USB_CABLE)
			path = m_resourceImgs[4].c_str();
		else if (m_backend == IOS_AIRPLAY)
			path = m_resourceImgs[6].c_str();
		else if (m_backend == ANDROID_USB_CABLE)
			path = m_resourceImgs[2].c_str();
		break;
	default:
		break;
	}

	if (path)
	{
		loadImage(path);
	}
}

void ScreenMirrorServer::setBackendType(int type)
{
	m_backend = (MirrorBackEnd)type;
	if (m_backend == IOS_AIRPLAY)
		m_extraDelay = 250;
	else
		m_extraDelay = 0;

	resetState();
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
	}
}

void ScreenMirrorServer::saveStatusSettings()
{
	obs_data_t *setting = obs_source_get_settings(m_source);
	obs_data_release(setting);
	obs_data_set_int(setting, "status", mirror_status);
}

void ScreenMirrorServer::initDecoder(uint8_t *data, size_t len)
{
	if (m_decoder && !m_decoder->CheckSPSChanged(data, len))
		return;
	if (m_decoder)
		delete m_decoder;

	if (m_renderTexture) {
		gs_texture_destroy(m_renderTexture);
		m_renderTexture = nullptr;
	}

	m_decoder = new AVDecoder;
	m_decoder->Init(data, len, m_renderer->GetDevice());
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

void ScreenMirrorServer::handleMirrorStatus(int status)
{
	auto status_func = [=]() {
		updateStatusImage();

		obs_data_t *event = obs_data_create();
		obs_data_set_string(event, "eventType", "MirrorStatus");
		obs_data_set_int(event, "value", mirror_status);
		obs_source_signal_event(m_source, event);
		obs_data_release(event);
	};

	if (status == mirror_status) {
		if (status == OBS_SOURCE_MIRROR_STOP)
		{
			if (m_lastStopType != m_backend)
			{
				m_lastStopType = m_backend;
				status_func();
			}
		}
	}
	else
	{
		mirror_status = (obs_source_mirror_status)status;
		saveStatusSettings();
		status_func();

		if (mirror_status == OBS_SOURCE_MIRROR_STOP) {
			resetState();
		} else if (mirror_status == OBS_SOURCE_MIRROR_START) {
			m_startTimeElapsed = 0;
		}
	}
}

bool ScreenMirrorServer::handleMediaData()
{
	static size_t max_packet_size = 1024 * 1024 * 10;

	size_t header_size = sizeof(struct av_packet_info);
	if (m_avBuffer.size < header_size)
		return false;

	struct av_packet_info header_info = {0};
	circlebuf_peek_front(&m_avBuffer, &header_info, header_size);

	size_t req_size = header_info.size;
	if (m_avBuffer.size < req_size + header_size)
		return false;

	if (header_info.type == FFM_MIRROR_STATUS) {
		if (req_size != sizeof(int)) {
			circlebuf_free(&m_avBuffer);
			return false;
		}
	} else if (header_info.type == FFM_MEDIA_INFO) {
		if (req_size != sizeof(struct media_info)) {
			circlebuf_free(&m_avBuffer);
			return false;
		}
	} else {
		if (req_size > max_packet_size) {
			circlebuf_free(&m_avBuffer);
			return false;
		}
	}

	circlebuf_pop_front(&m_avBuffer, &header_info,
			    header_size); // remove it

	if (header_info.type == FFM_MIRROR_STATUS) {
		int status = -1;
		circlebuf_pop_front(&m_avBuffer, &status, sizeof(int));

		pthread_mutex_lock(&m_statusMutex);
		handleMirrorStatus(status);
		pthread_mutex_unlock(&m_statusMutex);
	} else if (header_info.type == FFM_MEDIA_INFO) {
		struct media_info info;
		memset(&info, 0, req_size);
		circlebuf_pop_front(&m_avBuffer, &info, req_size);
		if (info.sps_len == 0 && info.pps_len == 0)
			return true;

		resetResampler(info.speakers, info.format, info.samples_per_sec);

		pthread_mutex_lock(&m_videoDataMutex);
		auto cache = (uint8_t *)malloc(info.pps_len);
		memcpy(cache, info.pps, info.pps_len);
		m_videoInfoIndex++;
		VideoInfo vi;
		vi.data = cache;
		vi.data_len = info.pps_len;
		m_videoInfos.insert(std::make_pair(m_videoInfoIndex, std::move(vi)));
		pthread_mutex_unlock(&m_videoDataMutex);

		handleMirrorStatus(OBS_SOURCE_MIRROR_OUTPUT);
	} else {
		if (header_info.type == FFM_PACKET_AUDIO) {
			outputAudio(req_size, header_info.pts, header_info.serial);
		} else {
			uint8_t *temp_buf = (uint8_t *)calloc(1, req_size);
			circlebuf_pop_front(&m_avBuffer, temp_buf, req_size);
			outputVideo(temp_buf, req_size, header_info.pts);
		}
	}
	return true;
}

void ScreenMirrorServer::resetState()
{
	pthread_mutex_lock(&m_videoDataMutex);
	m_offset = LLONG_MAX;
	for (auto iter = m_videoFrames.begin(); iter != m_videoFrames.end(); iter++) {
		VideoFrame &f = *iter;
		free(f.data);
	}
	m_videoFrames.clear();
	m_videoInfoIndex = 0;
	m_lastVideoInfoIndex = 0;
	for (auto iter = m_videoInfos.begin(); iter != m_videoInfos.end(); iter++) {
		VideoInfo &f = iter->second;
		free(f.data);
	}
	m_videoInfos.clear();
	pthread_mutex_unlock(&m_videoDataMutex);

	pthread_mutex_lock(&m_audioDataMutex);
	m_audioOffset = LLONG_MAX;
	m_audioFrameType = m_backend;
	circlebuf_free(&m_audioFrames);
	pthread_mutex_unlock(&m_audioDataMutex);
}

bool ScreenMirrorServer::initAudioRenderer()
{
	memset(&from, 0, sizeof(struct resample_info));
	PaError err = Pa_Initialize();
	if (err != paNoError)
		goto error;

	//20ms 
	err = Pa_OpenDefaultStream(&pa_stream_, 0, 2, paInt16, 44100, 480,
				   audiocb, this);
	if (err != paNoError)
		goto error;

	err = Pa_StartStream(pa_stream_);
	if (err != paNoError)
		goto error;

	to.samples_per_sec = sample_rate = 44100;
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
	if (pa_stream_) {
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
	if (format == AUDIO_FORMAT_UNKNOWN || samples_per_sec == 0 ||
	    speakers == SPEAKERS_UNKNOWN)
		return;

	if (from.format == format && from.speakers == speakers &&
	    from.samples_per_sec == samples_per_sec && resampler)
		return;

	if (resampler) {
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

void ScreenMirrorServer::dropAudioFrame(int64_t now_ms)
{
	auto two_pkt_size = 2 * (1920 + sizeof(uint64_t));
	while (m_audioFrames.size >= two_pkt_size)
	{
		uint8_t temp[1920 + 2 * sizeof(uint64_t)] = { 0 };
		circlebuf_peek_front(&m_audioFrames, temp, 1920 + 2 * sizeof(uint64_t));
		uint64_t p1 = 0;
		uint64_t p2 = 0;
		memcpy(&p1, temp, 4);
		memcpy(&p2, temp + 1920 + sizeof(uint64_t), 4);

		auto p1ts = p1 + m_audioOffset + m_extraDelay;
		auto p2ts = p2 + m_audioOffset + m_extraDelay;

		if (p1ts < now_ms && p2ts < now_ms && now_ms - p2ts > 60) {
			circlebuf_pop_front(&m_audioFrames, temp, 1920 + sizeof(uint64_t));
		} else
			break;
	}
}

int ScreenMirrorServer::audiocb(const void *input, void *output,
			     unsigned long frameCount,
			     const PaStreamCallbackTimeInfo *timeInfo,
			     PaStreamCallbackFlags statusFlags, void *userData)
{
	
	ScreenMirrorServer *s = (ScreenMirrorServer *)userData;
	pthread_mutex_lock(&s->m_audioDataMutex);

	if (s->m_audioFrameType == IOS_AIRPLAY) {
		bool copyed = false;
		if (s->m_audioFrames.size > 0) {
			int64_t target_pts = 0;
			int64_t now_ms = (int64_t)os_gettime_ns() / 1000000;

			if (s->m_audioOffset == LLONG_MAX) {
				uint64_t pts = 0;
				circlebuf_peek_front(&s->m_audioFrames, &pts, sizeof(uint64_t));
				s->m_audioOffset = now_ms - pts - 100; // 音频接收到的就有点慢，延迟减去100ms
			}

			s->dropAudioFrame(now_ms);

			uint64_t pts = 0;
			circlebuf_peek_front(&s->m_audioFrames, &pts, sizeof(uint64_t));
			target_pts = pts + s->m_audioOffset + s->m_extraDelay;
			if (target_pts <= now_ms) {
				circlebuf_pop_front(&s->m_audioFrames, &pts, sizeof(uint64_t));
				circlebuf_pop_front(&s->m_audioFrames, output, frameCount * 4);
				copyed = true;
			} 
		}
		if (!copyed)
			memset(output, 0, frameCount * 4);
			
	} else {
		auto requestSize = frameCount * 4;
		auto targetSize = s->m_audioFrames.size > requestSize ? requestSize : s->m_audioFrames.size;
		circlebuf_pop_front(&s->m_audioFrames, output, targetSize);
		if (targetSize < requestSize)
			memset((uint8_t *)output + targetSize, 0, requestSize - targetSize);
	}

	pthread_mutex_unlock(&s->m_audioDataMutex);

	return paContinue;
}

void ScreenMirrorServer::outputAudio(size_t data_len, uint64_t pts, int serial)
{
	pts = pts / 1000000;
	if (!resampler)
		return;

	static size_t max_len = data_len;
	if (!m_audioCacheBuffer)
		m_audioCacheBuffer = (uint8_t *)malloc(data_len);

	if (max_len < data_len) {
		max_len = data_len;
		m_audioCacheBuffer = (uint8_t *)realloc(m_audioCacheBuffer, max_len);
	}

	circlebuf_pop_front(&m_avBuffer, m_audioCacheBuffer, data_len);

	uint8_t *resample_data[MAX_AV_PLANES];
	uint8_t *input[MAX_AV_PLANES] = {m_audioCacheBuffer};
	uint32_t resample_frames = 0;
	uint64_t ts_offset = 0;
	bool success = false;

	success = audio_resampler_resample(
		resampler, resample_data, &resample_frames, &ts_offset,
		(const uint8_t *const *)input,
		(uint32_t)(get_audio_frames(from.format, from.speakers, data_len)));
	if (!success) {
		return;
	}

	pthread_mutex_lock(&m_audioDataMutex);
	if (m_audioFrameType == IOS_AIRPLAY)
		circlebuf_push_back(&m_audioFrames, &pts, sizeof(uint64_t));
	circlebuf_push_back(&m_audioFrames, resample_data[0], resample_frames * 2 * 2);
	pthread_mutex_unlock(&m_audioDataMutex);
}

void ScreenMirrorServer::outputVideo(uint8_t *data, size_t data_len, int64_t pts)
{
	pthread_mutex_lock(&m_videoDataMutex);
	m_videoFrames.push_back({ m_videoInfoIndex, data, data_len,  pts / 1000000 });
	pthread_mutex_unlock(&m_videoDataMutex);
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

	s->mirrorServerDestroy();
	s->setBackendType((int)type);
	s->mirrorServerSetup();
}

static void GetWinAirplayDefaultsOutput(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "type",
				 ScreenMirrorServer::IOS_USB_CABLE);
	obs_data_set_default_int(settings, "status", MIRROR_STOP);
}

static obs_properties_t *GetWinAirplayPropertiesOutput(void *data)
{
	if (!data)
		return nullptr;
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_int(props, "type", u8"投屏类型", 0, 2, 1);
	return props;
}

void *ScreenMirrorServer::CreateWinAirplay(obs_data_t *settings, obs_source_t *source)
{
	auto handler = CreateEvent(NULL, FALSE, FALSE, INSTANCE_LOCK);
	auto lastError = GetLastError();
	if (handler == NULL)
		return nullptr;

	if (lastError == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(handler);
		return nullptr;
	}

	ScreenMirrorServer *server = new ScreenMirrorServer(source);
	server->m_handler = handler;
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
	s->doRenderer(effect);
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

void ScreenMirrorServer::dropFrame(int64_t now_ms)
{
	while (m_videoFrames.size() >= 2) {
		auto iter = m_videoFrames.begin();
		auto p1 = iter->pts;
		auto p1ts = iter->pts + m_offset + m_extraDelay;

		auto next = ++iter;
		auto p2 = next->pts;
		auto p2ts = next->pts + m_offset + m_extraDelay;

		if ((p1 == 0 && p2 == 0) || p1ts < now_ms && p2ts < now_ms && now_ms - p2ts > 60) {
			VideoFrame &frame = m_videoFrames.front();
			if (m_decoder) {
				m_encodedPacket.data = frame.data;
				m_encodedPacket.size = frame.data_len;
				int ret = m_decoder->Send(&m_encodedPacket);
				while (ret >= 0) {
					ret = m_decoder->Recv(m_decodedFrame);
				}
			}
			free(frame.data);
			m_videoFrames.pop_front();
		} else
			break;
	}
}

void ScreenMirrorServer::doRenderer(gs_effect_t *effect)
{
	if (mirror_status != OBS_SOURCE_MIRROR_OUTPUT && if2->image.texture) {
		m_width = gs_texture_get_width(if2->image.texture);
		m_height = gs_texture_get_height(if2->image.texture);
		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), if2->image.texture);
		gs_draw_sprite(if2->image.texture, 0, m_width, m_height);
		return;
	}

	pthread_mutex_lock(&m_videoDataMutex);
	while (m_videoFrames.size() > 0) {
		
		int64_t now_ms = (int64_t)os_gettime_ns() / 1000000;
		int64_t target_pts = 0;

		if (m_offset == LLONG_MAX) {
			VideoFrame &frame = m_videoFrames.front();
			m_offset = now_ms - frame.pts;
		}

		dropFrame(now_ms);

		VideoFrame &framev = m_videoFrames.front();
		target_pts = framev.pts + m_offset + m_extraDelay;
		if (target_pts <= now_ms || framev.pts == 0) {
			if (framev.video_info_index != m_lastVideoInfoIndex) {
				const VideoInfo &vi = m_videoInfos[framev.video_info_index];
				initDecoder(vi.data, vi.data_len);
				free(vi.data);
				m_videoInfos.erase(framev.video_info_index);
				m_lastVideoInfoIndex = framev.video_info_index;
			}
			
			m_encodedPacket.data = framev.data;
			m_encodedPacket.size = framev.data_len;
			int ret = m_decoder->Send(&m_encodedPacket);
			while (ret >= 0) {
				ret = m_decoder->Recv(m_decodedFrame);
				if (ret >= 0) {
					m_renderer->RenderFrame(m_decodedFrame);
					m_width = m_decodedFrame->width;
					m_height = m_decodedFrame->height;
					if (!m_renderTexture) {
						m_renderTexture = gs_texture_open_shared((uint32_t)m_renderer->GetTextureSharedHandle());
					}
				}
			}

			free(framev.data);
			m_videoFrames.pop_front();
		}
		break;
	}
	pthread_mutex_unlock(&m_videoDataMutex);

	if (m_renderTexture) {
		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), m_renderTexture);
		gs_draw_sprite(m_renderTexture, 0, gs_texture_get_width(m_renderTexture), gs_texture_get_height(m_renderTexture));
	}
}

void ScreenMirrorServer::WinAirplayVideoTick(void *data, float seconds)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;

	pthread_mutex_lock(&s->m_statusMutex);
	if (s->mirror_status == OBS_SOURCE_MIRROR_START) {
		s->m_startTimeElapsed += seconds;
		if (s->m_startTimeElapsed > 10.0) {
			s->handleMirrorStatus(OBS_SOURCE_MIRROR_DEVICE_LOST);
		}
	}

	if (s->mirror_status != OBS_SOURCE_MIRROR_OUTPUT) {
		uint64_t frame_time = obs_get_video_frame_time();
		if (s->last_time && s->if2->image.is_animated_gif) {
			uint64_t elapsed = frame_time - s->last_time;
			bool updated = gs_image_file2_tick(s->if2, elapsed);

			if (updated) {
				obs_enter_graphics();
				gs_image_file2_update_texture(s->if2);
				obs_leave_graphics();
			}
		}
		s->last_time = frame_time;
	}
	pthread_mutex_unlock(&s->m_statusMutex);
}

static void WinAirplayCustomCommand(void *data, obs_data_t *cmd)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	const char *cmdType = obs_data_get_string(cmd, "type");
	if (strcmp("airplayRestart", cmdType) == 0) {
		s->mirrorServerDestroy();
		s->mirrorServerSetup();
	}
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
	info.create = ScreenMirrorServer::CreateWinAirplay;
	info.destroy = DestroyWinAirplay;
	info.update = UpdateWinAirplaySource;
	info.get_defaults = GetWinAirplayDefaultsOutput;
	info.get_properties = GetWinAirplayPropertiesOutput;
	info.video_render = WinAirplayRender;
	info.get_width = WinAirplayWidth;
	info.get_height = WinAirplayHeight;
	info.video_tick = ScreenMirrorServer::WinAirplayVideoTick;
	info.make_command = WinAirplayCustomCommand;
	obs_register_source(&info);

	return true;
}

void obs_module_unload(void) {}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
	if (dwReason == DLL_PROCESS_ATTACH) DllHandle = hModule;
	return TRUE;
}
