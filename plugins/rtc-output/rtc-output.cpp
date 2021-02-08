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
	context->m_rtcBase->setVideoInfo(obs_data_get_int(settings, "audiobitrate"), obs_data_get_int(settings, "videobitrate"), obs_data_get_int(settings, "fps"), obs_data_get_int(settings, "v_width"), obs_data_get_int(settings, "v_height"));
	context->m_rtcBase->setLinkInfo(obs_data_get_string(settings, "linkInfo"));
	context->m_rtcBase->setCropInfo(obs_data_get_int(settings, "cropX"), obs_data_get_int(settings, "cropWidth"));
	context->m_rtcBase->setRemoteViewHwnd(obs_data_get_int(settings, "hwnd"));
	context->m_rtcBase->setMicInfo(obs_data_get_string(settings, "micInfo"));
	context->m_rtcBase->setAudioOutputDevice(obs_data_get_string(settings, "playout_device"));
	audio_convert_info info;
	info.format = AUDIO_FORMAT_16BIT;
	info.samples_per_sec = 44100;
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

	obs_output_begin_data_capture(context->m_output, 0);
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

	obs_register_output(&rtc_output_info);
	return true;
}

void obs_module_unload(void) {}

RTCOutput::RTCOutput(RTC_TYPE type, obs_output_t *output)
{
	m_output = output;
	if(type == RTC_TYPE_TRTC)
		m_rtcBase = new TRTC();
	else if (type == RTC_TYPE_QINIU)
		m_rtcBase = new QINIURTC();

	m_rtcBase->setRtcEventCallback(std::bind(&RTCOutput::sigEvent, this, std::placeholders::_1, std::placeholders::_2));

	obs_source_t *mic = obs_get_output_source(3); //主麦克风
	if (mic)
	{
		connectMicSignals(mic);
		obs_source_release(mic);
	}

	obs_source_t *playout = obs_get_output_source(1); //主扬声器
	if (playout) {
		connectPlayoutSignals(playout);
		obs_source_release(playout);
	}

	auto channelChange = [](void *data, calldata_t *param) {
		auto     self = static_cast<RTCOutput *>(data);
		auto     source = static_cast<obs_source_t *>(calldata_ptr(param, "source"));
		uint32_t channel = (uint32_t)calldata_int(param, "channel");
		bool isMic = channel == 3;

		if (channel != 3 && channel != 1)
			return;

		if (isMic)
		{
			if (!source)
			{
				self->m_rtcBase->setAudioInputMute(true);
			}
			else
			{
				self->m_rtcBase->setAudioInputMute(obs_source_muted(source));
				self->m_rtcBase->setAudioInputVolume(obs_source_get_volume(source));
				obs_data_t *s = obs_source_get_settings(source);
				self->m_rtcBase->setAudioInputDevice(obs_data_get_string(s, "device_id"));
				obs_data_release(s);
				self->connectMicSignals(source);
			}
		}
		else
			self->connectPlayoutSignals(source);
	};
	channelChangeSignal.Connect(obs_get_signal_handler(), "channel_change", channelChange, this);
}

RTCOutput::~RTCOutput()
{
	m_rtcBase->setRtcEventCallback(nullptr);

	if (m_rtcBase)
		m_rtcBase->deleteLater();
}

void RTCOutput::sigEvent(int type, QJsonObject data)
{
	data["event_type"] = type;
	obs_data_t *p = obs_data_create_from_json(QJsonDocument(data).toJson(QJsonDocument::Compact).data());
	struct calldata params = { 0 };
	calldata_set_ptr(&params, "output", m_output);
	calldata_set_ptr(&params, "data", p);
	signal_handler_signal(obs_output_get_signal_handler(m_output), "sig_event", &params);
	obs_data_release(p);
	calldata_free(&params);
}

void RTCOutput::muteChanged(void *param, calldata_t *calldata)
{
	RTCOutput *output = (RTCOutput *)param;
	bool mute = calldata_bool(calldata, "muted");
	output->m_rtcBase->setAudioInputMute(mute);
}

void RTCOutput::volumeChanged(void *param, calldata_t *calldata)
{
	RTCOutput *output = (RTCOutput *)param;
	float volume = (float)calldata_float(calldata, "volume");
	output->m_rtcBase->setAudioInputVolume(volume);
}

void RTCOutput::settingChanged(void *param, calldata_t *calldata)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");
	RTCOutput *output = (RTCOutput *)param;
	obs_data_t *settings = obs_source_get_settings(source);
	output->m_rtcBase->setAudioInputDevice(QString(obs_data_get_string(settings, "device_id")));
	obs_data_release(settings);
}

void RTCOutput::playoutSettingChanged(void *param, calldata_t *calldata)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");
	RTCOutput *output = (RTCOutput *)param;
	obs_data_t *settings = obs_source_get_settings(source);
	output->m_rtcBase->setAudioOutputDevice(QString(obs_data_get_string(settings, "device_id")));
	obs_data_release(settings);
}

void RTCOutput::connectMicSignals(obs_source_t *source)
{
	micMuteSignal.Connect(obs_source_get_signal_handler(source), "mute", muteChanged, this);
	micVolumeSignal.Connect(obs_source_get_signal_handler(source), "volume", volumeChanged, this);
	micSettingSignal.Connect(obs_source_get_signal_handler(source), "settings_update", settingChanged, this);
}

void RTCOutput::connectPlayoutSignals(obs_source_t *source)
{
	playoutSettingSignal.Connect(obs_source_get_signal_handler(source), "settings_update", playoutSettingChanged, this);
}
