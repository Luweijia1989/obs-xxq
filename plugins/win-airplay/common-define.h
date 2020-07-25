#pragma once

#include "../../libobs/media-io/audio-io.h"
#define PIPE_NAME "ScreenMirror_Pipe"

enum av_packet_type {
	FFM_PACKET_VIDEO,
	FFM_PACKET_AUDIO,
	FFM_MEDIA_INFO,
};

#define FFM_SUCCESS 0
#define FFM_ERROR -1
#define FFM_UNSUPPORTED -2

struct av_packet_info {
	int64_t pts;
	uint32_t size;
	enum av_packet_type type;
};

struct media_info {
	enum speaker_layout speakers;
	enum audio_format format;
	uint32_t samples_per_sec;
	size_t bytes_per_frame;

	uint8_t sps[256];
	size_t sps_len;
	uint8_t pps[256];
	size_t pps_len;
};
