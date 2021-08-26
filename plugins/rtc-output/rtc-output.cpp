#include <util/threading.h>
#include "rtc-output.h"

static const char *rtc_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "YPP Rtc Output";
}

static void *rtc_output_create(obs_data_t *settings, obs_output_t *output)
{
	int rtc_type = obs_data_get_int(settings, "rtc_type");
	RTCOutput *context = new RTCOutput((RTCOutput::RTC_TYPE)rtc_type, output);

	audio_convert_info info;
	info.format = AUDIO_FORMAT_16BIT;
	info.samples_per_sec = 48000;
	info.speakers = SPEAKERS_STEREO;
	obs_output_set_audio_conversion(output, &info);

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	video_scale_info vInfo;
	vInfo.format = VIDEO_FORMAT_I420;
	vInfo.colorspace = ovi.colorspace;
	vInfo.range = ovi.range;
	vInfo.width = ovi.output_width;
	vInfo.height = ovi.output_height;
	obs_output_set_video_conversion(output, &vInfo);
	return context;
}

static void rtc_output_destroy(void *data)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);
	if (context->stop_thread_active)
		pthread_join(context->stop_thread, NULL);
	delete context;
}

static bool rtc_output_start(void *data)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);

	if (!obs_output_can_begin_data_capture(context->m_output, 0))
		return false;

	context->m_rtcBase->enterRoom();

	obs_output_initialize_encoders(context->m_output, 0);

	if (context->stop_thread_active)
		pthread_join(context->stop_thread, NULL);

	return true;
}

static void *stop_thread(void *data)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);
	obs_output_end_data_capture(context->m_output);
	context->stop_thread_active = false;
	return NULL;
}

static void rtc_output_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(ts);

	RTCOutput *context = static_cast<RTCOutput *>(data);
	context->m_rtcBase->exitRoom();
	context->stop_thread_active = pthread_create(&context->stop_thread,
		NULL, stop_thread,
		data) == 0;
}

static void rtc_output_raw_video(void *data, struct video_data *frame)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);
	context->m_rtcBase->sendVideo(frame);
}
static void rtc_output_raw_audio(void *data, struct audio_data *frames)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);
	context->m_rtcBase->sendAudio(frames);
}

static void rtc_output_custom_command(void *data, obs_data_t *param)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);
	const char * func = obs_data_get_string(param, "func");

	if (strcmp(func, "sei") == 0)
	{
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setSei(obj["sei_content"].toObject(), obj["sei_type"].toInt());
	}
	else if (strcmp(func, "device_mute") == 0)
	{
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setAudioInputMute(obj["mute"].toBool());
	}
	else if (strcmp(func, "device_volume") == 0)
	{
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setAudioInputVolume(obj["volume"].toInt());
	}
	else if (strcmp(func, "device_id") == 0) {
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setAudioInputDevice(obj["device"].toString());
	}
	else if (strcmp(func, "output_device_id") == 0) {
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setAudioOutputDevice(obj["device"].toString());
	}
	else if (strcmp(func, "output_device_mute") == 0)
	{
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setAudioOutputMute(obj["mute"].toBool());
	}
	else if (strcmp(func, "output_device_volume") == 0)
	{
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setAudioOutputVolume(obj["volume"].toInt());
	}
	else if (strcmp(func, "startRecord") == 0) {
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->startRecord(obj["path"].toString());
	}
	else if (strcmp(func, "stopRecord") == 0) {
		context->m_rtcBase->stopRecord();
	}
	else if (strcmp(func, "connectOtherRoom") == 0) {
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->connectOtherRoom(obj["userId"].toString(), obj["roomId"].toInt(), obj["uid"].toString(), obj["selfDoConnect"].toBool());
	}
	else if (strcmp(func, "disconnectOtherRtcRoom") == 0) {
		context->m_rtcBase->disconnectOtherRoom();
	}
	else if (strcmp(func, "muteRemoteAnchor") == 0) {
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->muteRemoteAnchor(obj["mute"].toBool());
	}
	else if (strcmp(func, "setRemoteVolume") == 0)
	{
		//连麦pk音量调节
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setRemoteVolume(obj["volume"].toInt());
	}
	else if (strcmp(func, "setRemoteMute") == 0)
	{
		//连麦pk静音调节
		auto obj = QJsonDocument::fromJson(obs_data_get_string(param, "param")).object();
		context->m_rtcBase->setRemoteMute(obj["mute"].toBool());
	}
}

static void rtc_output_update(void *data, obs_data_t *settings)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);

	context->m_rtcBase->setRtcEnterInfo(obs_data_get_string(settings, "rtcEnterRoomInfo"));
	context->m_rtcBase->setRemoteUserInfos(obs_data_get_string(settings, "remoteUserInfos"));
	context->m_rtcBase->setVideoEncodeInfo(obs_data_get_string(settings, "videoEncodeInfo"));
	context->m_rtcBase->setMixInfo(obs_data_get_string(settings, "mixInfo"));
	context->m_rtcBase->setCropInfo(obs_data_get_int(settings, "cropX"), obs_data_get_int(settings, "cropWidth"));
	context->m_rtcBase->setMicInfo(obs_data_get_string(settings, "micInfo"));
	context->m_rtcBase->setDesktopAudioInfo(obs_data_get_string(settings, "desktopAudioInfo"));
}

static uint64_t rtc_get_total_bytes(void *data)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);
	return context->m_rtcBase->getTotalBytes();
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-outputs", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "ypp rtc outputs";
}

bool obs_module_load(void)
{
	struct obs_output_info rtc_output_info = { 0 };
	rtc_output_info.id = "rtc_output";
	rtc_output_info.flags = OBS_OUTPUT_AV;
	rtc_output_info.get_name = rtc_output_getname;
	rtc_output_info.create = rtc_output_create;
	rtc_output_info.destroy = rtc_output_destroy;
	rtc_output_info.start = rtc_output_start;
	rtc_output_info.stop = rtc_output_stop;
	rtc_output_info.raw_audio = rtc_output_raw_audio;
	rtc_output_info.raw_video = rtc_output_raw_video;
	rtc_output_info.custom_command = rtc_output_custom_command;
	rtc_output_info.update = rtc_output_update;
	rtc_output_info.get_total_bytes = rtc_get_total_bytes;

	obs_register_output(&rtc_output_info);
	return true;
}

void obs_module_unload(void) {}

RTCOutput::RTCOutput(RTC_TYPE type, obs_output_t *output)
{
	m_output = output;
	m_rtcBase = new TRTC();
	m_rtcBase->setRtcEventCallback(std::bind(&RTCOutput::sigEvent, this, std::placeholders::_1, std::placeholders::_2));
}

RTCOutput::~RTCOutput()
{
	m_rtcBase->setRtcEventCallback(nullptr);

	if (m_rtcBase) {
		delete m_rtcBase;
		m_rtcBase = nullptr;
	}
}

void RTCOutput::sigEvent(int type, QJsonObject data)
{
	if (type == RTC_EVENT_SUCCESS)
		obs_output_begin_data_capture(m_output, 0);

	data["event_type"] = type;
	obs_data_t *p = obs_data_create();
	QString str = QJsonDocument(data).toJson(QJsonDocument::Compact);
	std::string sr = str.toStdString();
	obs_data_set_string(p, "param", sr.c_str());
	struct calldata params = { 0 };
	calldata_set_ptr(&params, "output", m_output);
	calldata_set_ptr(&params, "data", p);
	signal_handler_signal(obs_output_get_signal_handler(m_output), "sig_event", &params);
	obs_data_release(p);
	calldata_free(&params);
}
