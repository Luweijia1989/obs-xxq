#include <obs-module.h>
#include "airplay-server.h"
#include "resource.h"
#include <cstdio>
#include <util/dstr.h>
#include <Shlwapi.h>
#include <QTimer>
#include <QStandardPaths>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QMessageBox>
#include <QApplication>
#include "common-define.h"

using namespace std;

#define DRIVER_EXE "driver-tool.exe"
#define AIRPLAY_EXE "airplay-server.exe"
#define ANDROID_USB_EXE "android-usb-mirror.exe"
#define ANDROID_AOA_EXE "android-aoa-server.exe"
#define ANDROID_WIRELESS_EXE "rtmp-server.exe"

static QMap<ScreenMirrorServer::MirrorBackEnd, QString> g_resource_map = {
	{ScreenMirrorServer::IOS_AIRPLAY, "iOSwuxian"},
	{ScreenMirrorServer::IOS_USB_CABLE, "iOSyouxian"},
	{ScreenMirrorServer::ANDROID_AOA, "AndroidAOA"},
	{ScreenMirrorServer::ANDROID_WIRELESS, "Androidwuxian"},
	{ScreenMirrorServer::ANDROID_USB_CABLE, "AndroidADB"},
	{ScreenMirrorServer::IOS_WIRELESS, "iOSsaoma"}
};

uint findProcessListeningToPort(uint port)
{
	QString netstatOutput;
	{
		QProcess process;
		process.start("netstat -ano -p tcp");
		process.waitForFinished();
		netstatOutput = process.readAllStandardOutput();
	}
	QRegularExpression processFinder;
	{
		const auto pattern =
			QStringLiteral(R"(TCP[^:]+:%1.+LISTENING\s+(\d+))")
				.arg(port);
		processFinder.setPattern(pattern);
	}
	const auto processInfo = processFinder.match(netstatOutput);
	if (processInfo.hasMatch()) {
		const auto processId = processInfo.captured(1).toUInt();
		return processId;
	}
	return 0;
}

ScreenMirrorServer::ScreenMirrorServer(obs_source_t *source, int type)
	: m_source(source),
	  if2((gs_image_file2_t *)bzalloc(sizeof(gs_image_file2_t)))
{
	m_commandIPC = new MirrorRPC;
	m_timerHelperObject = new QObject;
	m_helperTimer = new QTimer(m_timerHelperObject);
	m_helperTimer->setSingleShot(true);
	QObject::connect(m_helperTimer, &QTimer::timeout, m_timerHelperObject, [=](){
		handleMirrorStatus(OBS_SOURCE_MIRROR_DEVICE_LOST);
	});

	memset(&m_audioInfo, 0, sizeof(media_audio_info));
	initSoftOutputFrame();

	dumpResourceImgs();

	pthread_mutex_init(&m_videoDataMutex, nullptr);
	pthread_mutex_init(&m_audioDataMutex, nullptr);
	pthread_mutex_init(&m_ptsMutex, nullptr);

	circlebuf_init(&m_avBuffer);

	saveStatusSettings();

	m_renderer = new DXVA2Renderer;
	m_renderer->Init();

	m_stop = false;
	m_audioLoopThread =
		std::thread(ScreenMirrorServer::audio_tick_thread, this);
	m_audioLoopThread.detach();

	changeBackendType(type);
}

ScreenMirrorServer::~ScreenMirrorServer()
{
	mirrorServerDestroy();

	delete m_commandIPC;
	delete m_timerHelperObject;

	m_stop = true;
	if (m_audioLoopThread.joinable())
		m_audioLoopThread.join();

	obs_enter_graphics();
	gs_image_file2_free(if2);
	releaseRenderTexture();
	obs_leave_graphics();
	bfree(if2);

	circlebuf_free(&m_avBuffer);
	pthread_mutex_destroy(&m_videoDataMutex);
	pthread_mutex_destroy(&m_audioDataMutex);
	pthread_mutex_destroy(&m_ptsMutex);

	av_frame_free(&m_decodedFrame);

	if (m_decoder)
		delete m_decoder;
	if (m_renderer)
		delete m_renderer;

	if (pps_cache)
		free(pps_cache);
#ifdef DUMPFILE
	fclose(m_auioFile);
	fclose(m_videoFile);
#endif
}

void ScreenMirrorServer::dumpResourceImgs()
{
	auto checkWrite = [](QString path){
		QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		auto p = QString("%1/%2").arg(tempPath).arg(path);
		if (QFile::exists(p))
			return;

		auto src = QString(":/%1").arg(path);
		QFile::copy(src, p);
	};

	for (auto iter = g_resource_map.begin(); iter != g_resource_map.end(); iter++) {
		auto str = iter.value();
		checkWrite(str + ".png");
		checkWrite(str + "zhong.png");
		checkWrite(str + "shibai.png");
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

	os_kill_process(m_backendProcessName.toStdString().c_str());
	ipc_server_create(&m_ipcServer, ScreenMirrorServer::pipeCallback, this);

	struct dstr cmd;
	dstr_init_move_array(&cmd, os_get_executable_path_ptr(m_backendProcessName.toStdString().c_str()));
	dstr_insert_ch(&cmd, 0, '\"');
	dstr_cat(&cmd, "\" \"");
	process = os_process_pipe_create(cmd.array, "w");
	dstr_free(&cmd);
}

void ScreenMirrorServer::mirrorServerDestroy()
{
	if (process) {
		m_commandIPC->requestQuit();
		os_process_pipe_destroy_timeout(process, 1500);
		process = NULL;
	}

	if (m_ipcServer)
		ipc_server_destroy(&m_ipcServer);

	circlebuf_free(&m_avBuffer);
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
		break;
	case ScreenMirrorServer::IOS_AIRPLAY:
		m_backendProcessName = AIRPLAY_EXE;
		break;
	case ScreenMirrorServer::ANDROID_USB_CABLE:
		m_backendProcessName = ANDROID_USB_EXE;
		break;
	case ScreenMirrorServer::ANDROID_AOA:
		m_backendProcessName = ANDROID_AOA_EXE;
		break;
	case ScreenMirrorServer::ANDROID_WIRELESS:
	case ScreenMirrorServer::IOS_WIRELESS:
		m_backendProcessName = ANDROID_WIRELESS_EXE;
		break;
	default:
		break;
	}
	
	auto prefix = g_resource_map.value(m_backend);
	QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	m_backendStopImagePath = QString("%1/%2.png").arg(tempPath).arg(prefix);
	m_backendLostImagePath = QString("%1/%2shibai.png").arg(tempPath).arg(prefix);
	m_backendConnectingImagePath = QString("%1/%2zhong.png").arg(tempPath).arg(prefix);
	if (m_backend == ANDROID_WIRELESS || m_backend == IOS_WIRELESS) {
		auto qrcode = streamUrlImage();
		if (!qrcode.isEmpty()) {
			auto writeQRCodeImage = [=](QString srcFile, QString dstFile){
				QImage image(srcFile);
				image = image.convertToFormat(QImage::Format_RGBA8888);
				QPainter p(&image);
				p.drawImage(QRect(174, 223, 260, 260), QImage(qrcode));
				QImageWriter writer(dstFile);
				writer.write(image);
				return dstFile;
			};

			m_backendStopImagePath = writeQRCodeImage(m_backendStopImagePath, QString("%1/%2_bak.png").arg(tempPath).arg(prefix));
			m_backendLostImagePath = writeQRCodeImage(m_backendLostImagePath, QString("%1/%2shibai_bak.png").arg(tempPath).arg(prefix));
			m_backendConnectingImagePath = writeQRCodeImage(m_backendConnectingImagePath, QString("%1/%2zhong_bak.png").arg(tempPath).arg(prefix));
		}
	}

	if (m_backend == IOS_AIRPLAY)
		m_extraDelay = 500;
	else if (m_backend == ANDROID_WIRELESS)
		m_extraDelay = 300;
	else if (m_backend == IOS_WIRELESS)
		m_extraDelay = 400;
	else if (m_backend == ANDROID_AOA)
		m_extraDelay = 0;
	else
		m_extraDelay = 0;

	obs_source_set_monitoring_type(
		m_source,
		(m_backend == ANDROID_AOA
			|| m_backend == ANDROID_WIRELESS
			|| m_backend == IOS_WIRELESS)
			? OBS_MONITORING_TYPE_NONE
			: OBS_MONITORING_TYPE_MONITOR_ONLY);

	resetState();
	handleMirrorStatus(MIRROR_STOP);
}

void ScreenMirrorServer::changeBackendType(int type)
{
	if (type == m_backend)
		return;

	if (type == ANDROID_WIRELESS || type == IOS_WIRELESS) {
		os_kill_process(ANDROID_WIRELESS_EXE);
		auto pid = findProcessListeningToPort(1935);
		if (pid != 0) {
			blog(LOG_INFO, "rtmp port used, cannot listen to it");
			QMessageBox *msgBox = new QMessageBox(QApplication::activeModalWidget());
			msgBox->setWindowFlag(Qt::WindowStaysOnTopHint);
			msgBox->setIcon(QMessageBox::Information);
			msgBox->setText(u8"无线投屏服务启动失败\n请关闭其他投屏服务软件后重启助手");
			msgBox->addButton(u8"确定", QMessageBox::RejectRole);
			msgBox->setAttribute(Qt::WA_DeleteOnClose); // delete pointer after close
			msgBox->setModal(false);
			msgBox->show();
			return;
		}
	}

	mirrorServerDestroy();
	setBackendType((int)type);
	mirrorServerSetup();
}

void ScreenMirrorServer::updateStatusImage()
{
	obs_data_t *event = obs_data_create();
	obs_data_set_string(event, "eventType", "MirrorStatus");
	obs_data_set_int(event, "value", mirror_status);
	obs_source_signal_event(m_source, event);
	obs_data_release(event);

	if (mirror_status == OBS_SOURCE_MIRROR_OUTPUT)
		return;
	QString path;
	switch (mirror_status) {
	case OBS_SOURCE_MIRROR_START:
		path = m_backendConnectingImagePath;
		break;
	case OBS_SOURCE_MIRROR_STOP:
		path = m_backendStopImagePath;
		break;
	case OBS_SOURCE_MIRROR_DEVICE_LOST: // 连接失败，检测超时
		path = m_backendLostImagePath;
	default:
		break;
	}

	obs_enter_graphics();
	gs_image_file2_free(if2);
	obs_leave_graphics();

	if (path.length() > 0) {
		gs_image_file2_init(if2, path.toStdString().c_str());
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

void ScreenMirrorServer::releaseRenderTexture()
{
	if (m_renderTexture) {
		gs_texture_destroy(m_renderTexture);
		m_renderTexture = nullptr;
	}
}

void ScreenMirrorServer::initDecoder(uint8_t *data, size_t len, bool forceRecreate, bool forceSoftware)
{
	if (!forceRecreate && (m_decoder && !m_decoder->CheckSPSChanged(data, len)))
		return;
	if (m_decoder)
		delete m_decoder;

	releaseRenderTexture();

	m_decoder = new AVDecoder;
	m_decoder->Init(data, len, m_renderer->GetDevice(), forceSoftware);

	blog(LOG_INFO, "mirror decoder init complete, use hardware: %s", m_decoder->IsHWDecode() ? "true" : "false");
}

void ScreenMirrorServer::handleMirrorStatusInternal(int status)
{
	if (status == mirror_status) {
		if (status == OBS_SOURCE_MIRROR_STOP) {
			if (m_lastStopType != m_backend) {
				m_lastStopType = m_backend;
				updateStatusImage();
			}
		}
	} else {
		mirror_status = (obs_source_mirror_status)status;
		saveStatusSettings();
		updateStatusImage();

		if (mirror_status == OBS_SOURCE_MIRROR_STOP) {
			resetState();
		} else if (mirror_status == OBS_SOURCE_MIRROR_START) {
			m_helperTimer->start(30000);
		} else if (mirror_status == OBS_SOURCE_MIRROR_OUTPUT)
			m_helperTimer->stop();
	}
}

void ScreenMirrorServer::handleMirrorStatus(int status)
{
	QMetaObject::invokeMethod(m_timerHelperObject, [status, this](){
		handleMirrorStatusInternal(status);
	});
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
	} else if (header_info.type == FFM_MEDIA_VIDEO_INFO) {
		if (req_size != sizeof(struct media_video_info)) {
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
		handleMirrorStatus(status);
	} else if (header_info.type == FFM_MEDIA_VIDEO_INFO) {
		struct media_video_info info;
		memset(&info, 0, req_size);
		circlebuf_pop_front(&m_avBuffer, &info, req_size);
		if (info.video_extra_len == 0)
			return true;

		pthread_mutex_lock(&m_videoDataMutex);
		auto cache = (uint8_t *)malloc(info.video_extra_len);
		memcpy(cache, info.video_extra, info.video_extra_len);
		m_videoInfoIndex++;
		VideoInfo vi;
		vi.data = cache;
		vi.data_len = info.video_extra_len;
		m_videoInfos.insert(
			std::make_pair(m_videoInfoIndex, std::move(vi)));
		pthread_mutex_unlock(&m_videoDataMutex);

		handleMirrorStatus(OBS_SOURCE_MIRROR_OUTPUT);
	} else if (header_info.type == FFM_MEDIA_AUDIO_INFO) {
		pthread_mutex_lock(&m_audioDataMutex);
		circlebuf_pop_front(&m_avBuffer, &m_audioInfo, req_size);
		pthread_mutex_unlock(&m_audioDataMutex);
	} else {
		uint8_t *temp_buf = (uint8_t *)calloc(1, req_size);
		circlebuf_pop_front(&m_avBuffer, temp_buf, req_size);
		if (header_info.type == FFM_PACKET_AUDIO)
			outputAudio(temp_buf, req_size, header_info.pts, header_info.serial);
		else
			outputVideo(temp_buf, req_size, header_info.pts);
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
	for (auto iter = m_audioFrames.begin(); iter != m_audioFrames.end();
	     iter++) {
		AudioFrame &f = *iter;
		free(f.data);
	}
	m_audioFrames.clear();
	memset(&m_audioInfo, 0, sizeof(media_audio_info));
	pthread_mutex_unlock(&m_audioDataMutex);

	pthread_mutex_lock(&m_ptsMutex);
	if (m_backend == ANDROID_WIRELESS || m_backend == IOS_WIRELESS)
		m_audioExtraOffset = LLONG_MAX;
	else if (m_backend == IOS_AIRPLAY)
		m_audioExtraOffset = -100;
	else
		m_audioExtraOffset = 0;

	m_firstAudioPTS = LLONG_MAX;
	m_firstVideoPTS = LLONG_MAX;

	if (m_backend == ANDROID_WIRELESS || m_backend == IOS_WIRELESS)
		m_videoExtraOffset = LLONG_MAX;
	else
		m_videoExtraOffset = 0;
	pthread_mutex_unlock(&m_ptsMutex);

	obs_enter_graphics();
	releaseRenderTexture();
	obs_leave_graphics();
	updateSoftOutputFrame(nullptr);
}

bool ScreenMirrorServer::initPipe()
{
	ipc_server_create(&m_ipcServer, ScreenMirrorServer::pipeCallback, this);
	return true;
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
	if (!frame) {
		obs_source_set_videoframe(m_source, nullptr);
		return;
	}
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

void ScreenMirrorServer::dropAudioFrame(int64_t now_ms)
{
	while (m_audioFrames.size() >= 2) {
		auto iter = m_audioFrames.begin();
		auto p1 = iter->pts;

		auto next = ++iter;
		auto p2 = next->pts;

		auto p1ts = p1 + m_audioOffset + m_extraDelay;
		auto p2ts = p2 + m_audioOffset + m_extraDelay;

		if (p1ts < now_ms && p2ts < now_ms && now_ms - p2ts > 150) {
			free(m_audioFrames.front().data);
			m_audioFrames.pop_front();
		} else
			break;
	}
}

void *ScreenMirrorServer::audio_tick_thread(void *data)
{
	auto func = [](std::list<AudioFrame> *frames, obs_source_t *source, media_audio_info *audioInfo) {
		if (frames->size() <= 0
		    || !audioInfo->samples_per_sec
		    || audioInfo->format == AUDIO_FORMAT_UNKNOWN
		    || audioInfo->speakers == SPEAKERS_UNKNOWN)
			return;
		AudioFrame &frame = frames->front();
		obs_source_audio audio;
		audio.format = audioInfo->format;
		audio.samples_per_sec = audioInfo->samples_per_sec;
		audio.speakers = audioInfo->speakers;
		audio.frames = frame.data_len / (audioInfo->speakers * sizeof(short));
		audio.timestamp = os_gettime_ns();
		audio.data[0] = frame.data;
		obs_source_output_audio(source, &audio);

		free(frame.data);
		frames->pop_front();
	};

	ScreenMirrorServer *s = (ScreenMirrorServer *)data;
	while (!s->m_stop) {
		pthread_mutex_lock(&s->m_audioDataMutex);
		if (s->canProcessMediaData() && s->m_audioFrames.size() > 0) {
			int64_t pts = s->m_audioFrames.front().pts;
			if (pts > 0) {
				int64_t now_ms = (int64_t)os_gettime_ns() / 1000000;

				if (s->m_audioOffset == LLONG_MAX) {
					s->m_audioOffset = now_ms - pts + s->m_audioExtraOffset; 
				}

				s->dropAudioFrame(now_ms);

				int64_t target_pts = s->m_audioFrames.front().pts + s->m_audioOffset + s->m_extraDelay;
				if (target_pts <= now_ms) {
					func(&s->m_audioFrames, s->m_source, &s->m_audioInfo);
				}
			} else {
				func(&s->m_audioFrames, s->m_source, &s->m_audioInfo);
			}
		}
		pthread_mutex_unlock(&s->m_audioDataMutex);
		os_sleep_ms(8);
	}

	return NULL;
}

void ScreenMirrorServer::outputAudio(uint8_t *data, size_t data_len, int64_t pts, int serial)
{
	pthread_mutex_lock(&m_ptsMutex);
	if (m_firstAudioPTS == LLONG_MAX)
		m_firstAudioPTS = pts / 1000000;
	pthread_mutex_unlock(&m_ptsMutex);

	pts = pts / 1000000;
	pthread_mutex_lock(&m_audioDataMutex);
	m_audioFrames.push_back({data, data_len, pts, serial});
	pthread_mutex_unlock(&m_audioDataMutex);
}

void ScreenMirrorServer::outputVideo(uint8_t *data, size_t data_len,
				     int64_t pts)
{
	pthread_mutex_lock(&m_ptsMutex);
	if (m_firstVideoPTS == LLONG_MAX)
		m_firstVideoPTS = pts / 1000000;
	pthread_mutex_unlock(&m_ptsMutex);

	pthread_mutex_lock(&m_videoDataMutex);
	m_videoFrames.push_back({m_videoInfoIndex, data, data_len, pts / 1000000});
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
	(void)type_data;
	return obs_module_text("WindowsAirplay");
}

static void UpdateWinAirplaySource(void *obj, obs_data_t *settings)
{
	(void)obj;
	(void)settings;
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
	ScreenMirrorServer *server = new ScreenMirrorServer(source, obs_data_get_int(settings, "type"));
	return server;
}

static void DestroyWinAirplay(void *obj)
{
	delete reinterpret_cast<ScreenMirrorServer *>(obj);
}

static void HideWinAirplay(void *data) { (void)data; }

static void ShowWinAirplay(void *data) { (void)data; }
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

bool ScreenMirrorServer::canProcessMediaData()
{
	bool ret = false;
	pthread_mutex_lock(&m_ptsMutex);
	bool timeOffsetInited = (m_videoExtraOffset != LLONG_MAX || m_audioExtraOffset != LLONG_MAX);
	if (!timeOffsetInited) {
		if (m_firstAudioPTS != LLONG_MAX && m_firstVideoPTS != LLONG_MAX) {
			if (m_firstVideoPTS > m_firstAudioPTS) {
				m_videoExtraOffset = 0;
				m_audioExtraOffset = m_firstAudioPTS - m_firstVideoPTS;
			} else {
				m_videoExtraOffset = m_firstVideoPTS - m_firstAudioPTS;
				m_audioExtraOffset = 0;
			}
			ret = true;
		}
	}
	else
		ret = true;
	pthread_mutex_unlock(&m_ptsMutex);

	return ret;
}

void ScreenMirrorServer::doRenderer(gs_effect_t *effect)
{
	if (mirror_status != OBS_SOURCE_MIRROR_OUTPUT) {
		if (if2->image.texture) {
			m_width = gs_texture_get_width(if2->image.texture);
			m_height = gs_texture_get_height(if2->image.texture);
			gs_effect_set_texture(gs_effect_get_param_by_name(effect,
									  "image"),
					      if2->image.texture);
			gs_draw_sprite(if2->image.texture, 0, m_width, m_height);
		}
		return;
	}

	pthread_mutex_lock(&m_videoDataMutex);
	while (canProcessMediaData() && m_videoFrames.size() > 0) {
		int64_t now_ms = (int64_t)os_gettime_ns() / 1000000;
		int64_t target_pts = 0;

		if (m_offset == LLONG_MAX) {
			VideoFrame &frame = m_videoFrames.front();
			m_offset = now_ms - frame.pts + m_videoExtraOffset;
		}

		dropFrame(now_ms);

		VideoFrame &framev = m_videoFrames.front();
		target_pts = framev.pts + m_offset + m_extraDelay;
		if (target_pts <= now_ms + 2 || framev.pts == 0) { //+2 为了pts的误差，视频帧时间戳超过可播放时间2ms，也认为当前是可以播放的。
			if (framev.video_info_index != m_lastVideoInfoIndex) {
				const VideoInfo &vi = m_videoInfos[framev.video_info_index];
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
						m_height = m_decodedFrame->height;
						if (m_decoder->IsHWDecode()) {
							m_renderer->RenderFrame(m_decodedFrame);
							if (!m_renderTexture) {
								m_renderTexture = gs_texture_open_shared((uint32_t)m_renderer->GetTextureSharedHandle());
							}
						} else {
							updateSoftOutputFrame(m_decodedFrame);
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
				gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), m_renderTexture);
				gs_draw_sprite(m_renderTexture, 0, gs_texture_get_width(m_renderTexture), gs_texture_get_height(m_renderTexture));
			}
		} else
			obs_source_draw_videoframe(m_source);
	}

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
	info.make_command = WinAirplayCustomCommand;
	obs_register_source(&info);

	return true;
}

void obs_module_unload(void) {}
