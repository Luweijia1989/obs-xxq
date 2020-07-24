#pragma once

#include "parse_header.h"
#include "cmsamplebuf.h"
#include <stdbool.h>

#pragma pack(1)
struct AsynCmSampleBufPacket
{
	CFTypeID clockRef;
	struct CMSampleBuffer CMSampleBuf;
};
#pragma pack()

bool NewAsynCmSampleBufPacketFromBytes(uint8_t *data, size_t data_len, struct AsynCmSampleBufPacket *out);

bool newAsynCmSampleBufferPacketFromBytes(uint8_t *data, size_t data_len, CFTypeID *clockRef, struct CMSampleBuffer *cMSampleBuf);

void clearAsynCmSampleBufPacket(struct AsynCmSampleBufPacket *packet);