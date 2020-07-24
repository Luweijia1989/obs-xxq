#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "ringbuf.h"

struct LengthFieldBasedFrameExtractor
{
	ringbuf_t buffer;
	bool ready_for_next_frame;
	size_t next_frame_size;
};

bool handleNewFrame(struct LengthFieldBasedFrameExtractor *extractor, uint8_t *data, size_t data_len, uint8_t **out, size_t *out_len);

bool extractFrame(struct LengthFieldBasedFrameExtractor *extractor, uint8_t *data, size_t data_len, uint8_t **out, size_t *out_len);