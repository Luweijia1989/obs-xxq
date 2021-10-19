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
#define ANDROID_AOA_EXE "android-aoa-server.exe"
#define ANDROID_WIRELESS_EXE "rtmp-server.exe"
#define INSTANCE_LOCK L"AIRPLAY-ONE-INSTANCE"

#define AIRPLAY_AUDIO_PKT_SIZE 1920
#define ANDROID_AOA_AUDIO_PKT_SIZE 1920

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
	  if2((gs_image_file2_t *)bzalloc(sizeof(gs_image_file2_t))),
	  m_audioTempBuffer((uint8_t *)bzalloc(ANDROID_AOA_AUDIO_PKT_SIZE +
					       2 * sizeof(uint64_t)))
{
	initSoftOutputFrame();

	dumpResourceImgs();

	pthread_mutex_init(&m_videoDataMutex, nullptr);
	pthread_mutex_init(&m_audioDataMutex, nullptr);
	pthread_mutex_init(&m_statusMutex, nullptr);

	circlebuf_init(&m_avBuffer);
	circlebuf_init(&m_audioFrames);

	saveStatusSettings();

	m_renderer = new DXVA2Renderer;
	m_renderer->Init();

	m_stop = false;
	m_audioLoopThread =
		std::thread(ScreenMirrorServer::audio_tick_thread, this);
	m_audioLoopThread.detach();
}

ScreenMirrorServer::~ScreenMirrorServer()
{
	m_stop = true;
	if (m_audioLoopThread.joinable())
		m_audioLoopThread.join();

	mirrorServerDestroy();

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

	bfree(m_audioTempBuffer);

	if (pps_cache)
		free(pps_cache);

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
	m_resourceImgs.push_back(
		prefix + "pic_android_screencastfailed_cableprojection.png");
	m_resourceImgs.push_back(prefix + "pic_ios_cableprojection.png");
	m_resourceImgs.push_back(
		prefix + "pic_ios_screencastfailed_cableprojection.png");
	m_resourceImgs.push_back(prefix + "pic_ios_wirelessprojection.png");
	m_resourceImgs.push_back(
		prefix + "pic_ios_screencastfailed_wirelessprojection.png");
	m_resourceImgs.push_back(prefix + "pic_android_aoa.png");
	m_resourceImgs.push_back(prefix + "pic_android_aoa_fail.png");

	std::vector<int> ids = {IDB_PNG1, IDB_PNG2, IDB_PNG3, IDB_PNG4,
				IDB_PNG5, IDB_PNG6, IDB_PNG7, IDB_PNG8};

	for (auto iter = 0; iter < m_resourceImgs.size(); iter++) {
		const string &img = m_resourceImgs.at(iter);
		if (!PathFileExistsA(img.c_str())) {
			HANDLE hFile = CreateFileA(img.c_str(), GENERIC_WRITE,
						   FILE_SHARE_READ, NULL,
						   CREATE_ALWAYS,
						   FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				HRSRC res = NULL;
				if (iter == 0) {
					res = FindResource(
						DllHandle,
						MAKEINTRESOURCE(IDB_BITMAP1),
						L"GIF");
				} else {
					res = FindResource(
						DllHandle,
						MAKEINTRESOURCE(ids[iter - 1]),
						L"PNG");
				}
				auto g = GetLastError();
				HGLOBAL res_handle =
					LoadResource(DllHandle, res);
				auto res_data = LockResource(res_handle);
				auto res_size = SizeofResource(DllHandle, res);

				DWORD byteWritten = 0;
				WriteFile(hFile, res_data, res_size,
					  &byteWritten, NULL);
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

	os_kill_process(m_backendProcessName.c_str());
	ipc_server_create(&m_ipcServer, ScreenMirrorServer::pipeCallback, this);

	struct dstr cmd;
	dstr_init_move_array(&cmd, os_get_executable_path_ptr(m_backendProcessName.c_str()));
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

void ScreenMirrorServer::updateStatusImage()
{
	if (mirror_status == OBS_SOURCE_MIRROR_OUTPUT)
		return;
	std::string path;
	switch (mirror_status) {
	case OBS_SOURCE_MIRROR_START:
		path = m_resourceImgs[0].c_str();
		break;
	case OBS_SOURCE_MIRROR_STOP:
		path = m_backendStopImagePath;
		break;
	case OBS_SOURCE_MIRROR_DEVICE_LOST: // 连接失败，检测超时
		path = m_backendLostImagePath;
	default:
		break;
	}

	loadImage(path);
}

void ScreenMirrorServer::setBackendType(int type)
{
	m_backend = (MirrorBackEnd)type;
	switch (m_backend)
	{
	case ScreenMirrorServer::None:
		m_backendProcessName = "";
		break;
	case ScreenMirrorServer::IOS_USB_CABLE:
		m_backendProcessName = DRIVER_EXE;
		m_backendLostImagePath = m_resourceImgs[4];
		m_backendStopImagePath = m_resourceImgs[3];
		break;
	case ScreenMirrorServer::IOS_AIRPLAY:
		m_backendProcessName = AIRPLAY_EXE;
		m_backendLostImagePath = m_resourceImgs[6];
		m_backendStopImagePath = m_resourceImgs[5];
		break;
	case ScreenMirrorServer::ANDROID_USB_CABLE:
		m_backendProcessName = ANDROID_USB_EXE;
		m_backendLostImagePath = m_resourceImgs[2];
		m_backendStopImagePath = m_resourceImgs[1];
		break;
	case ScreenMirrorServer::ANDROID_AOA:
		m_backendProcessName = ANDROID_AOA_EXE;
		m_backendLostImagePath = m_resourceImgs[8];
		m_backendStopImagePath = m_resourceImgs[7];
		break;
	case ScreenMirrorServer::ANDROID_WIRELESS:
	{
		m_backendProcessName = ANDROID_WIRELESS_EXE;
		auto list = streamUrlImages();
		if (!list.isEmpty()) {
			auto s = list.first().toStdString();
			m_backendStopImagePath = s;
		}
	}
		break;
	default:
		break;
	}

	if (m_backend == IOS_AIRPLAY || m_backend == ANDROID_WIRELESS)
		m_extraDelay = 500;
	else if (m_backend == ANDROID_AOA)
		m_extraDelay = 0;
	else
		m_extraDelay = 0;

	obs_source_set_monitoring_type(
		m_source,
		m_backend == ANDROID_AOA || m_backend == ANDROID_WIRELESS
			? OBS_MONITORING_TYPE_NONE
			: OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);

	resetState();
	handleMirrorStatus(MIRROR_STOP);
}

void ScreenMirrorServer::changeBackendType(int type)
{
	if (type == m_backend)
		return;

	mirrorServerDestroy();
	setBackendType((int)type);
	mirrorServerSetup();
}

void ScreenMirrorServer::loadImage(std::string path)
{
	obs_enter_graphics();
	gs_image_file2_free(if2);
	obs_leave_graphics();

	if (path.length() > 0) {
		gs_image_file2_init(if2, path.c_str());
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

void ScreenMirrorServer::initDecoder(uint8_t *data, size_t len, bool forceRecreate, bool forceSoftware)
{
	if (!forceRecreate && (m_decoder && !m_decoder->CheckSPSChanged(data, len)))
		return;
	if (m_decoder)
		delete m_decoder;

	if (m_renderTexture) {
		gs_texture_destroy(m_renderTexture);
		m_renderTexture = nullptr;
	}

	m_decoder = new AVDecoder;
	m_decoder->Init(data, len, m_renderer->GetDevice(), forceSoftware);

	static bool loged = false;
	if (!loged) {
		blog(LOG_INFO, "mirror decoder init complete, use hardware: %s",
		     m_decoder->IsHWDecode() ? "true" : "false");
		loged = true;
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
		if (status == OBS_SOURCE_MIRROR_STOP) {
			if (m_lastStopType != m_backend) {
				m_lastStopType = m_backend;
				status_func();
			}
		}
	} else {
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

		m_audioSampleRate = info.samples_per_sec;

		pthread_mutex_lock(&m_videoDataMutex);
		auto cache = (uint8_t *)malloc(info.pps_len);
		memcpy(cache, info.pps, info.pps_len);
		m_videoInfoIndex++;
		VideoInfo vi;
		vi.data = cache;
		vi.data_len = info.pps_len;
		m_videoInfos.insert(
			std::make_pair(m_videoInfoIndex, std::move(vi)));
		pthread_mutex_unlock(&m_videoDataMutex);

		handleMirrorStatus(OBS_SOURCE_MIRROR_OUTPUT);
	} else {
		if (header_info.type == FFM_PACKET_AUDIO) {
			outputAudio(req_size, header_info.pts,
				    header_info.serial);
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
	for (auto iter = m_videoFrames.begin(); iter != m_videoFrames.end();
	     iter++) {
		VideoFrame &f = *iter;
		free(f.data);
	}
	m_videoFrames.clear();
	m_videoInfoIndex = 0;
	m_lastVideoInfoIndex = 0;
	for (auto iter = m_videoInfos.begin(); iter != m_videoInfos.end();
	     iter++) {
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

bool ScreenMirrorServer::initPipe()
{
	ipc_server_create(&m_ipcServer, ScreenMirrorServer::pipeCallback, this);
	return true;
}

void ScreenMirrorServer::dropAudioFrame(int64_t now_ms)
{
	size_t pktSize = AIRPLAY_AUDIO_PKT_SIZE;
	auto two_pkt_size = 2 * (pktSize + sizeof(uint64_t));
	while (m_audioFrames.size >= two_pkt_size) {
		circlebuf_peek_front(&m_audioFrames, m_audioTempBuffer,
				     pktSize + 2 * sizeof(uint64_t));
		uint64_t p1 = 0;
		uint64_t p2 = 0;
		memcpy(&p1, m_audioTempBuffer, 4);
		memcpy(&p2, m_audioTempBuffer + pktSize + sizeof(uint64_t), 4);

		auto p1ts = p1 + m_audioOffset + m_extraDelay;
		auto p2ts = p2 + m_audioOffset + m_extraDelay;

		if (p1ts < now_ms && p2ts < now_ms && now_ms - p2ts > 60) {
			circlebuf_pop_front(&m_audioFrames, m_audioTempBuffer,
					    pktSize + sizeof(uint64_t));
		} else
			break;
	}
}

void ScreenMirrorServer::initSoftOutputFrame()
{
	memset(m_softOutputFrame.data, 0, sizeof(m_softOutputFrame.data));
	memset(&m_softOutputFrame, 0, sizeof(&m_softOutputFrame));

	m_softOutputFrame.range = VIDEO_RANGE_PARTIAL;
	video_format_get_parameters(VIDEO_CS_601, m_softOutputFrame.range,
				    m_softOutputFrame.color_matrix,
				    m_softOutputFrame.color_range_min,
				    m_softOutputFrame.color_range_max);
}

void ScreenMirrorServer::updateSoftOutputFrame(AVFrame *frame)
{
	auto ffmpeg_to_obs_video_format = [=](enum AVPixelFormat format) {
		switch (format) {
		case AV_PIX_FMT_YUV444P:
			return VIDEO_FORMAT_I444;
		case AV_PIX_FMT_YUV420P:
		case AV_PIX_FMT_YUVJ420P:
			return VIDEO_FORMAT_I420;
		case AV_PIX_FMT_NV12:
			return VIDEO_FORMAT_NV12;
		case AV_PIX_FMT_YUYV422:
			return VIDEO_FORMAT_YUY2;
		case AV_PIX_FMT_UYVY422:
			return VIDEO_FORMAT_UYVY;
		case AV_PIX_FMT_RGBA:
			return VIDEO_FORMAT_RGBA;
		case AV_PIX_FMT_BGRA:
			return VIDEO_FORMAT_BGRA;
		case AV_PIX_FMT_GRAY8:
			return VIDEO_FORMAT_Y800;
		case AV_PIX_FMT_BGR24:
			return VIDEO_FORMAT_BGR3;
		case AV_PIX_FMT_YUV422P:
			return VIDEO_FORMAT_I422;
		case AV_PIX_FMT_YUVA420P:
			return VIDEO_FORMAT_I40A;
		case AV_PIX_FMT_YUVA422P:
			return VIDEO_FORMAT_I42A;
		case AV_PIX_FMT_YUVA444P:
			return VIDEO_FORMAT_YUVA;
		case AV_PIX_FMT_NONE:
		default:
			return VIDEO_FORMAT_NONE;
		}
	};

	m_softOutputFrame.timestamp = frame->pts;
	m_softOutputFrame.width = frame->width;
	m_softOutputFrame.height = frame->height;
	m_softOutputFrame.format =
		ffmpeg_to_obs_video_format((AVPixelFormat)frame->format);
	m_softOutputFrame.flip = false;
	m_softOutputFrame.flip_h = false;

	m_softOutputFrame.data[0] = frame->data[0];
	m_softOutputFrame.data[1] = frame->data[1];
	m_softOutputFrame.data[2] = frame->data[2];

	m_softOutputFrame.linesize[0] = frame->linesize[0];
	m_softOutputFrame.linesize[1] = frame->linesize[1];
	m_softOutputFrame.linesize[2] = frame->linesize[2];

	obs_source_set_videoframe(m_source, &m_softOutputFrame);
}

void *ScreenMirrorServer::audio_tick_thread(void *data)
{
	uint8_t *popBuffer = nullptr;

	auto func = [=, &popBuffer](obs_source_t *source, circlebuf *frameBuf,
				    size_t buffLen, uint32_t sampleRate) {
		static size_t max_len = buffLen;
		if (!popBuffer)
			popBuffer = (uint8_t *)malloc(buffLen);

		if (max_len < buffLen) {
			max_len = buffLen;
			popBuffer = (uint8_t *)realloc(popBuffer, max_len);
		}

		circlebuf_pop_front(frameBuf, popBuffer, buffLen);

		obs_source_audio audio;
		audio.format = AUDIO_FORMAT_16BIT;
		audio.samples_per_sec = sampleRate;
		audio.speakers = SPEAKERS_STEREO;
		audio.frames = buffLen / 4;
		audio.timestamp = os_gettime_ns();
		audio.data[0] = popBuffer;

		if (sampleRate)
			obs_source_output_audio(source, &audio);
	};

	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	while (!s->m_stop) {
		pthread_mutex_lock(&s->m_audioDataMutex);
		if (s->m_audioFrames.size > 0) {
			if (s->m_audioFrameType == IOS_AIRPLAY || s->m_audioFrameType == ANDROID_AOA) {
				int64_t target_pts = 0;
				int64_t now_ms =
					(int64_t)os_gettime_ns() / 1000000;

				if (s->m_audioOffset == LLONG_MAX) {
					uint64_t pts = 0;
					circlebuf_peek_front(&s->m_audioFrames,
							     &pts,
							     sizeof(uint64_t));
					if (s->m_audioFrameType == IOS_AIRPLAY)
						s->m_audioOffset =
							now_ms - pts -
							100; // 音频接收到的就有点慢，延迟减去100ms
					else
						s->m_audioOffset = now_ms - pts;
				}

				s->dropAudioFrame(now_ms);

				uint64_t pts = 0;
				circlebuf_peek_front(&s->m_audioFrames, &pts,
						     sizeof(uint64_t));
				target_pts = pts + s->m_audioOffset +
					     s->m_extraDelay;
				if (target_pts <= now_ms) {
					circlebuf_pop_front(&s->m_audioFrames,
							    &pts,
							    sizeof(uint64_t));
					func(s->m_source, &s->m_audioFrames,
					     s->m_audioFrameType == IOS_AIRPLAY ? AIRPLAY_AUDIO_PKT_SIZE : ANDROID_AOA_AUDIO_PKT_SIZE,
					     s->m_audioSampleRate);
				}
			} else {
				size_t audioSize = s->m_audioFrames.size;
				func(s->m_source, &s->m_audioFrames, audioSize,
				     s->m_audioSampleRate);
			}
		}
		pthread_mutex_unlock(&s->m_audioDataMutex);
		os_sleep_ms(8);
	}

	if (popBuffer)
		free(popBuffer);

	return NULL;
}

void ScreenMirrorServer::outputAudio(size_t data_len, uint64_t pts, int serial)
{
	pts = pts / 1000000;

	static size_t max_len = data_len;
	if (!m_audioCacheBuffer)
		m_audioCacheBuffer = (uint8_t *)malloc(data_len);

	if (max_len < data_len) {
		max_len = data_len;
		m_audioCacheBuffer =
			(uint8_t *)realloc(m_audioCacheBuffer, max_len);
	}

	circlebuf_pop_front(&m_avBuffer, m_audioCacheBuffer, data_len);

	pthread_mutex_lock(&m_audioDataMutex);
	if (m_audioFrameType == IOS_AIRPLAY || m_audioFrameType == ANDROID_AOA)
		circlebuf_push_back(&m_audioFrames, &pts, sizeof(uint64_t));
	circlebuf_push_back(&m_audioFrames, m_audioCacheBuffer, data_len);
	pthread_mutex_unlock(&m_audioDataMutex);
}

void ScreenMirrorServer::outputVideo(uint8_t *data, size_t data_len,
				     int64_t pts)
{
	pthread_mutex_lock(&m_videoDataMutex);
	m_videoFrames.push_back(
		{m_videoInfoIndex, data, data_len, pts / 1000000});
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
}

static void GetWinAirplayDefaultsOutput(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "type",
				 ScreenMirrorServer::ANDROID_AOA);
	obs_data_set_default_int(settings, "status", MIRROR_STOP);
}

static obs_properties_t *GetWinAirplayPropertiesOutput(void *data)
{
	if (!data)
		return nullptr;
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_int(props, "type", u8"投屏类型", 0, 4, 1);
	return props;
}

void *ScreenMirrorServer::CreateWinAirplay(obs_data_t *settings,
					   obs_source_t *source)
{
	auto handler = CreateEvent(NULL, FALSE, FALSE, INSTANCE_LOCK);
	auto lastError = GetLastError();
	if (handler == NULL)
		return nullptr;

	if (lastError == ERROR_ALREADY_EXISTS) {
		CloseHandle(handler);
		return nullptr;
	}

	ScreenMirrorServer *server = new ScreenMirrorServer(source);
	server->m_handler = handler;
	server->changeBackendType(obs_data_get_int(settings, "type"));
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

		VideoFrame &frame = m_videoFrames.front();
		if (frame.video_info_index != m_lastVideoInfoIndex)
			break;

		if ((p1 == 0 && p2 == 0) ||
		    p1ts < now_ms && p2ts < now_ms && now_ms - p2ts > 60) {
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
		gs_effect_set_texture(gs_effect_get_param_by_name(effect,
								  "image"),
				      if2->image.texture);
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
		if (target_pts <= now_ms + 2 ||
		    framev.pts ==
			    0) { //+2 为了pts的误差，视频帧时间戳超过可播放时间2ms，也认为当前是可以播放的。
			if (framev.video_info_index != m_lastVideoInfoIndex) {
				const VideoInfo &vi =
					m_videoInfos[framev.video_info_index];
				if (!pps_cache) {
					pps_cache = (uint8_t *)malloc(vi.data_len);
					pps_cache_len = vi.data_len;
				} else if (pps_cache_len < vi.data_len) {
					pps_cache = (uint8_t *)realloc(pps_cache, vi.data_len);
					pps_cache_len = vi.data_len;
				}

				memcpy(pps_cache, vi.data, vi.data_len);

				initDecoder(vi.data, vi.data_len, false, false);
				free(vi.data);
				m_videoInfos.erase(framev.video_info_index);
				m_lastVideoInfoIndex = framev.video_info_index;
			}

			if (m_decoder) {
				m_encodedPacket.data = framev.data;
				m_encodedPacket.size = framev.data_len;
				int ret = m_decoder->Send(&m_encodedPacket);
				if (ret == AVERROR_INVALIDDATA) {
					initDecoder(pps_cache, pps_cache_len, true, true);
					ret = m_decoder->Send(&m_encodedPacket);
				}
				while (ret >= 0) {
					ret = m_decoder->Recv(m_decodedFrame);
					if (ret >= 0) {
						m_width = m_decodedFrame->width;
						m_height =
							m_decodedFrame->height;
						if (m_decoder->IsHWDecode()) {
							m_renderer->RenderFrame(
								m_decodedFrame);
							if (!m_renderTexture) {
								m_renderTexture = gs_texture_open_shared(
									(uint32_t)m_renderer
										->GetTextureSharedHandle());
							}
						} else {
							updateSoftOutputFrame(
								m_decodedFrame);
						}
					}
				}
			}

			free(framev.data);
			m_videoFrames.pop_front();
		}
		break;
	}
	pthread_mutex_unlock(&m_videoDataMutex);

	if (m_decoder) {
		if (m_decoder->IsHWDecode()) {
			if (m_renderTexture) {
				gs_effect_set_texture(
					gs_effect_get_param_by_name(effect,
								    "image"),
					m_renderTexture);
				gs_draw_sprite(
					m_renderTexture, 0,
					gs_texture_get_width(m_renderTexture),
					gs_texture_get_height(m_renderTexture));
			}
		} else
			obs_source_draw_videoframe(m_source);
	}
}

void ScreenMirrorServer::WinAirplayVideoTick(void *data, float seconds)
{
	ScreenMirrorServer *s = (ScreenMirrorServer *)data;

	pthread_mutex_lock(&s->m_statusMutex);
	if (s->mirror_status == OBS_SOURCE_MIRROR_START) {
		s->m_startTimeElapsed += seconds;
		if (s->m_startTimeElapsed > 30.0) {
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
	} else if (strcmp("changeBackend", cmdType) == 0) {
		int t = obs_data_get_int(cmd, "backendType");
		s->changeBackendType(t);
		
		obs_data_t *settings = obs_source_get_settings(s->m_source);
		obs_data_set_int(settings, "type", t);
		obs_data_release(settings);

		struct calldata data;
		uint8_t stack[128];
		calldata_init_fixed(&data, stack, sizeof(stack));
		calldata_set_ptr(&data, "source", s->m_source);
		signal_handler_signal(obs_source_get_signal_handler(s->m_source), "settings_update", &data);
	}
}

bool obs_module_load(void)
{
	obs_source_info info = {};
	info.id = "win_airplay";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE |
			    OBS_SOURCE_AUDIO;
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

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
		DllHandle = hModule;
	return TRUE;
}
