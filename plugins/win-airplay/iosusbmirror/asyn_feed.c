#include "asyn_feed.h"
#include "byteutils.h"

bool NewAsynCmSampleBufPacketFromBytes(uint8_t *data, size_t data_len, struct AsynCmSampleBufPacket *out)
{
	return newAsynCmSampleBufferPacketFromBytes(data, data_len, &out->clockRef, &out->CMSampleBuf);
}

bool newAsynCmSampleBufferPacketFromBytes(uint8_t *data, size_t data_len, CFTypeID *clockRef, struct CMSampleBuffer *cMSampleBuf)
{
	uint32_t magic = byteutils_get_int(data, 12);
	uint8_t *next_data_pointer;
	if (parseAsynHeader(data, magic, &next_data_pointer, clockRef) < 0)
		return false;

	if (magic == FEED)
	{
		if (!NewCMSampleBufferFromBytesVideo(data+16, data_len-16, cMSampleBuf))
		{
			clearCMSampleBuffer(cMSampleBuf);
			return false;
		}
	}
	else
	{
		if (!NewCMSampleBufferFromBytesAudio(data + 16, data_len - 16, cMSampleBuf))
		{
			clearCMSampleBuffer(cMSampleBuf);
			return false;
		}
	}
	return true;
}

void clearAsynCmSampleBufPacket(struct AsynCmSampleBufPacket *packet)
{
	if (packet)
		clearCMSampleBuffer(&packet->CMSampleBuf);
}

