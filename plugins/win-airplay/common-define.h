#pragma once

#include "../../libobs/media-io/audio-io.h"
#include "ipc.h"
#define PIPE_NAME "ScreenMirror_Pipe"

enum av_packet_type {
	FFM_PACKET_VIDEO,
	FFM_PACKET_AUDIO,
	FFM_MEDIA_INFO,
	FFM_MIRROR_STATUS,
};

enum mirror_status { // same as obs_source_mirror_status
	MIRROR_ERROR,
	MIRROR_DEVICE_CONNECTED,
	MIRROR_DEVICE_LOST,
	MIRROR_START,
	MIRROR_STOP,
	MIRROR_OUTPUT,
};

#define FFM_SUCCESS 0
#define FFM_ERROR -1
#define FFM_UNSUPPORTED -2

struct av_packet_info {
	int64_t pts;
	uint32_t size;
	int serial;
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

static void send_status(struct IPCClient *c, int status)
{
	struct av_packet_info info;
	info.pts = 0;
	info.size = sizeof(int);
	info.type = FFM_MIRROR_STATUS;

	ipc_client_write(c, &info, sizeof(struct av_packet_info), INFINITE);
	ipc_client_write(c, &status, sizeof(int), INFINITE);
}
