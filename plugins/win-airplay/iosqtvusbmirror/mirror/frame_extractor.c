#include "frame_extractor.h"
#include "log.h"
#include "byteutils.h"
#include <memory.h>

bool handleNewFrame(struct LengthFieldBasedFrameExtractor *extractor, uint8_t *data, size_t data_len, uint8_t **out, size_t *out_len)
{
	if (data_len == 0)
		return false;

	if (data_len < 4)
	{
		usbmuxd_log(LL_INFO, "Received less than four bytes, cannot read a valid frameLength field");
		return false;
	}

	size_t frame_length = byteutils_get_int(data, 0);

	if (data_len >= frame_length)
	{
		*out_len = frame_length - 4;
		*out = calloc(1, *out_len);
		memcpy(*out, data+4, *out_len);

		if (data_len > frame_length)
			ringbuf_memcpy_into(extractor->buffer, data+frame_length, data_len-frame_length);

		return true;
	}

	extractor->ready_for_next_frame = false;
	ringbuf_memcpy_into(extractor->buffer, data+4, data_len-4);
	extractor->next_frame_size = frame_length - 4;
	return false;
}

bool extractFrame(struct LengthFieldBasedFrameExtractor *extractor, uint8_t *data, size_t data_len, uint8_t **out, size_t *out_len)
{
	if (extractor->ready_for_next_frame && ringbuf_bytes_used(extractor->buffer) == 0)
		return handleNewFrame(extractor, data, data_len, out, out_len);

	size_t byte_remain = ringbuf_bytes_used(extractor->buffer);
	if (extractor->ready_for_next_frame && byte_remain != 0)
	{
		if (byte_remain >= 4)
		{
			unsigned char buff_len[4] = { 0 };
			ringbuf_memcpy_from(buff_len, extractor->buffer, 4);
			extractor->next_frame_size = byteutils_get_int(buff_len, 0) - 4;
			extractor->ready_for_next_frame = false;
			return extractFrame(extractor, data, data_len, out, out_len);
		}
	}

	ringbuf_memcpy_into(extractor->buffer, data, data_len);
	if (ringbuf_bytes_used(extractor->buffer) >= extractor->next_frame_size)
	{
		*out_len = extractor->next_frame_size;
		*out = calloc(1, extractor->next_frame_size);
		ringbuf_memcpy_from(*out, extractor->buffer, *out_len);
		extractor->ready_for_next_frame = true;
		return true;
	}
	return false;
}

