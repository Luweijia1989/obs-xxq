#include "audio_stream_basic_description.h"
#include <memory.h>
#include <stdlib.h>
#include "byteutils.h"

struct AudioStreamBasicDescription DefaultAudioStreamBasicDescription()
{
	struct AudioStreamBasicDescription asb = {
		.FormatFlags = 12,
		.BytesPerPacket = 4,
		.FramesPerPacket = 1,
		.BytesPerFrame = 4,
		.ChannelsPerFrame = 2,
		.BitsPerChannel = 16,
		.Reserved = 0,
		.SampleRate = 48000,
		.FormatID = AudioFormatIDLpcm
	};

	return asb;
}

struct AudioStreamBasicDescription NewAudioStreamBasicDescriptionFromBytes(uint8_t *bytes, size_t bytes_len)
{
	struct AudioStreamBasicDescription res = { 0 };
	if (bytes_len != sizeof(struct AudioStreamBasicDescription))
		return res;

	memcpy(&res, bytes, bytes_len);
	return res;
}

void SerializeAudioStreamBasicDescription(struct AudioStreamBasicDescription *asbd, uint8_t **out, size_t *out_len)
{
	*out = calloc(1, 56);
	*out_len = 56;

	uint8_t *pos = *out;
	int index = 0;
	memcpy(pos+index, &asbd->SampleRate, 8);
	index += 8;

	byteutils_put_int(pos, index, AudioFormatIDLpcm);
	index += 4;

	byteutils_put_int(pos, index, asbd->FormatFlags);
	index += 4;

	byteutils_put_int(pos, index, asbd->BytesPerPacket);
	index += 4;

	byteutils_put_int(pos, index, asbd->FramesPerPacket);
	index += 4;

	byteutils_put_int(pos, index, asbd->BytesPerFrame);
	index += 4;

	byteutils_put_int(pos, index, asbd->ChannelsPerFrame);
	index += 4;

	byteutils_put_int(pos, index, asbd->BitsPerChannel);
	index += 4;

	byteutils_put_int(pos, index, asbd->Reserved);
	index += 4;

	memcpy(pos + index, &asbd->SampleRate, 8);
	index += 8;
	memcpy(pos + index, &asbd->SampleRate, 8);
}


