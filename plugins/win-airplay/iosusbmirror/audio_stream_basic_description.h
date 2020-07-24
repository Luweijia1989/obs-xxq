#pragma once

#include <stdint.h>

#define AudioFormatIDLpcm 0x6C70636D

#pragma pack(1)
struct AudioStreamBasicDescription {
	double SampleRate;
	uint32_t FormatID;
	uint32_t FormatFlags;
	uint32_t BytesPerPacket;
	uint32_t FramesPerPacket;
	uint32_t BytesPerFrame;
	uint32_t ChannelsPerFrame;
	uint32_t BitsPerChannel;
	uint32_t Reserved;
};
#pragma pack()

struct AudioStreamBasicDescription DefaultAudioStreamBasicDescription();
struct AudioStreamBasicDescription NewAudioStreamBasicDescriptionFromBytes(uint8_t *bytes, size_t bytes_len);
void SerializeAudioStreamBasicDescription(struct AudioStreamBasicDescription *asbd, uint8_t **out, size_t *out_len);