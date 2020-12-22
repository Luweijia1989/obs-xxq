#include <util/threading.h>
#include "rtc-output.h"

static const char *rtc_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "YPP Rtc Output";
}

static void *rtc_output_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);
	RTCOutput *context = new RTCOutput;
	context->m_output = output;
	return context;
}

static void rtc_output_destroy(void *data)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);
	if (context->stop_thread_active)
		pthread_join(context->stop_thread, NULL);
	bfree(context);
}

static bool rtc_output_start(void *data)
{
	RTCOutput *context = static_cast<RTCOutput *>(data);

	if (!obs_output_can_begin_data_capture(context->m_output, 0))
		return false;
	if (!obs_output_initialize_encoders(context->m_output, 0))
		return false;

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
	context->stop_thread_active = pthread_create(&context->stop_thread,
						     NULL, stop_thread,
						     data) == 0;
}

static void rtc_output_data(void *data, struct encoder_packet *packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(packet);
}

static void rtc_output_custom_command(void *data, obs_data_t *param)
{
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
	rtc_output_info.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK;
	rtc_output_info.get_name = rtc_output_getname;
	rtc_output_info.create = rtc_output_create;
	rtc_output_info.destroy = rtc_output_destroy;
	rtc_output_info.start = rtc_output_start;
	rtc_output_info.stop = rtc_output_stop;
	rtc_output_info.encoded_packet = rtc_output_data;
	rtc_output_info.custom_command = rtc_output_custom_command;

	obs_register_output(&rtc_output_info);
	return true;
}

void obs_module_unload(void) {}
