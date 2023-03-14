#pragma once

#include <media-io/audio-io.h>
#include <media-io/audio-resampler.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

class XXQAec
{
public:
	XXQAec();
	~XXQAec();

	void initResamplers(uint32_t samplerate, audio_format format, speaker_layout layout);
	bool processData(bool needAec, uint8_t *data, int frames, uint8_t **output);

private:
	void initSpeex();
	void destroySpeex();

private:
	audio_resampler_t *convert2S16 = nullptr;
	audio_resampler_t *convert_back = nullptr;

	SpeexEchoState       *echo_state = NULL;
	SpeexPreprocessState *preprocess_state = NULL;

	uint8_t *buffer = nullptr;
	uint8_t *middle_buffer = nullptr;

	int frame_size = 0;
	int filter_size = 0;
	int64_t last_audio_ts = 0;
	float volume = 0.0f;
	int src_channel = 2;
};
