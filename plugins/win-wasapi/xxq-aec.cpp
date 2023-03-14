#include "xxq-aec.h"
#include <util/bmem.h>
#include <util/platform.h>
#include <obs.h>

#define FRAME_SIZE_IN_MS 10
#define FILTER_SIZE_IN_MS 100
#define SAMPLERATE 48000

XXQAec::XXQAec()
{
	frame_size = (FRAME_SIZE_IN_MS * SAMPLERATE) / 1000;
	filter_size = (FILTER_SIZE_IN_MS * SAMPLERATE) / 1000;
	buffer = (uint8_t *)bmalloc(frame_size * 4);
	middle_buffer = (uint8_t *)bmalloc(frame_size * 4);

	initSpeex();
}

XXQAec::~XXQAec()
{
	destroySpeex();

	if (convert2S16)
		audio_resampler_destroy(convert2S16);

	if (convert_back)
		audio_resampler_destroy(convert_back);

	bfree(buffer);
	bfree(middle_buffer);
}

void XXQAec::initSpeex()
{
	echo_state = speex_echo_state_init_mc(frame_size, filter_size, 2, 2); // for stereo
	preprocess_state = speex_preprocess_state_init(frame_size * 2, SAMPLERATE);

	int sampleRate = SAMPLERATE;
	speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
	speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);
}

void XXQAec::destroySpeex()
{
	speex_echo_state_destroy(echo_state);
	speex_preprocess_state_destroy(preprocess_state);
}


//target always 48000hz, 2channels, s16
void XXQAec::initResamplers(uint32_t samplerate, audio_format format, speaker_layout layout)
{
	src_channel = get_audio_channels(layout);

	resample_info dst;
	dst.format = AUDIO_FORMAT_16BIT;
	dst.samples_per_sec = 48000;
	dst.speakers = SPEAKERS_STEREO;

	resample_info src;
	src.samples_per_sec = samplerate;
	src.format = format;
	src.speakers = layout;

	convert2S16 = audio_resampler_create(&dst, &src);
	convert_back = audio_resampler_create(&src, &dst);
}

bool XXQAec::processData(bool needAec, uint8_t *data, int frames, uint8_t **output)
{
	bool res = obs_get_playing_audio_data(buffer, frame_size * 4);
	if (!res || !needAec) {
		*output = data;
		return false;
	}

	auto current_ts = os_gettime_ns() / 1000000;
	if (current_ts - last_audio_ts > 500)
		volume = 0.0f;

	last_audio_ts = current_ts;

	uint8_t *input[MAX_AV_PLANES] = {nullptr};
	input[0] = data;

	uint8_t *resample_data[MAX_AV_PLANES];
	uint32_t resample_frames;
	uint64_t ts_offset;
	bool success = audio_resampler_resample(convert2S16, resample_data, &resample_frames, &ts_offset, input, frames);
	if (!success) {
		*output = data;
		return false;
	}

	speex_echo_cancellation(echo_state, (const spx_int16_t *)resample_data[0], (const spx_int16_t *)buffer, (spx_int16_t *)middle_buffer);
	speex_preprocess_run(preprocess_state, (spx_int16_t *)middle_buffer);

	uint8_t *out[MAX_AV_PLANES];
	uint32_t out_frames;
	input[0] = middle_buffer;
	success = audio_resampler_resample(convert_back, out, &out_frames, &ts_offset, input, resample_frames);
	if (!success) {
		*output = data;
		return false;
	}

	if (volume < 1) {
		volume += 0.005f;
		float *o = (float *)out[0];
		float *end = o + out_frames * src_channel;
		while (o < end)
			*(o++) *= volume;
	}

	*output = out[0];
	return true;
}
