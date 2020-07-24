#pragma once

#define PIPE_NAME "ScreenMirror_Pipe"

enum ffm_packet_type {
	FFM_PACKET_VIDEO,
	FFM_PACKET_AUDIO,
};

#define FFM_SUCCESS 0
#define FFM_ERROR -1
#define FFM_UNSUPPORTED -2

struct ffm_packet_info {
	int64_t pts;
	int64_t dts;
	uint32_t size;
	uint32_t index;
	enum ffm_packet_type type;
	bool keyframe;
};
