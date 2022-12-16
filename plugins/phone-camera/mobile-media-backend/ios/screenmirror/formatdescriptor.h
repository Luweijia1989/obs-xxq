#pragma once

#include <stdint.h>
#include "dict.h"
#include "list.h"
#include "audio_stream_basic_description.h"

#define FormatDescriptorMagic            0x66647363
#define MediaTypeVideo                   0x76696465 
#define MediaTypeSound                   0x736F756E

struct FormatDescriptor {
	uint32_t MediaType;
	uint32_t VideoDimensionWidth;
	uint32_t VideoDimensionHeight;
	uint32_t Codec;
	list_t *Extensions;
	//PPS contains bytes of the Picture Parameter Set h264 NALu
	uint8_t	PPS[256];
	size_t PPS_len;
	//SPS contains bytes of the Picture Parameter Set h264 NALu
	uint8_t	SPS[256];
	size_t SPS_len;
	struct AudioStreamBasicDescription AudioDescription;
};

int NewFormatDescriptorFromBytes(uint8_t *data, size_t data_len, struct FormatDescriptor *desc);

int parseMediaType(uint8_t *data, size_t data_len, uint32_t *out_media_type, uint8_t **remaining_bytes);

int parseVideoFdsc(uint8_t *data, size_t data_len, struct FormatDescriptor *desc);

int parseAudioFdsc(uint8_t *data, size_t data_len, struct FormatDescriptor *desc);

int parseVideoDimension(uint8_t *data, size_t data_len, uint32_t *out_width, uint32_t *out_height, uint8_t **next_data_pointer);

int parseCodec(uint8_t *data, size_t data_len, uint32_t *out_codec, uint8_t **next_data_pointer);

int extractPPS(list_t *extensions, uint8_t *pps, size_t *pps_len, uint8_t *sps, size_t *sps_len);

void clearFormatDescriptor(struct FormatDescriptor *fd);
