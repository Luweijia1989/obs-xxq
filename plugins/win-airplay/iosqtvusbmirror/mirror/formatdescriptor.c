#include "formatdescriptor.h"
#include "parse_header.h"
#include "log.h"
#include "byteutils.h"
#include "audio_stream_basic_description.h"
#include <memory.h>

uint8_t start_code[] = { 0x00, 0x00, 0x00, 0x01 };

#define MediaTypeMagic                   0x6D646961 
#define VideoDimensionMagic              0x7664696D
#define CodecMagic                       0x636F6463 
#define CodecAvc1                        0x61766331 
#define ExtensionMagic                   0x6578746E 
#define AudioStreamBasicDescriptionMagic 0x61736264 

int NewFormatDescriptorFromBytes(uint8_t *data, size_t data_len, struct FormatDescriptor *desc)
{
	size_t key_len;
	uint8_t *remaining_bytes;
	if (ParseLengthAndMagic(data, data_len, FormatDescriptorMagic, &remaining_bytes, &key_len) < 0)
		return -1;

	uint32_t media_type;
	uint8_t *remaining_bytes2;
	if (parseMediaType(remaining_bytes, data_len-(remaining_bytes-data), &media_type, &remaining_bytes2) < 0)
		return -1;

	if (media_type == MediaTypeSound)
	{
		return parseAudioFdsc(remaining_bytes2, data_len-(remaining_bytes2-data), desc);
	}
	
	return parseVideoFdsc(remaining_bytes2, data_len-(remaining_bytes2-data), desc);
}

int parseMediaType(uint8_t *data, size_t data_len, uint32_t *out_media_type, uint8_t **remaining_bytes)
{
	size_t key_len;
	uint8_t *temp;
	if (ParseLengthAndMagic(data, data_len, MediaTypeMagic, &temp, &key_len) < 0)
		return -1;

	if (key_len != 12)
	{
		usbmuxd_log(LL_ERROR, "invalid length for media type: %d", key_len);
		return -1;
	}

	*out_media_type = byteutils_get_int(data, 8);
	*remaining_bytes = data+key_len;
	return 0;
}

int parseVideoFdsc(uint8_t *data, size_t data_len, struct FormatDescriptor *desc)
{
	uint32_t width; 
	uint32_t height;
	uint8_t *remaining_bytes;
	if (parseVideoDimension(data, data_len, &width, &height, &remaining_bytes) < 0)
		return -1;

	uint32_t codec;
	uint8_t *index_dic_data_pointer;
	if (parseCodec(remaining_bytes, data_len-(remaining_bytes-data), &codec, &index_dic_data_pointer) < 0)
		return -1;

	list_t *extensions = NewIndexDictFromBytesWithCustomMarker(index_dic_data_pointer, data_len-(index_dic_data_pointer-data), ExtensionMagic);
	if (!extensions)
		return -1;
	 
	if (extractPPS(extensions, desc->PPS, &desc->PPS_len, desc->SPS, &desc->SPS_len) < 0)
		return -1;

	desc->MediaType = MediaTypeVideo;
	desc->VideoDimensionWidth = width;
	desc->VideoDimensionHeight = height;
	desc->Codec = codec;
	desc->Extensions = extensions;

	return 0;
}

int parseAudioFdsc(uint8_t *data, size_t data_len, struct FormatDescriptor *desc)
{
	size_t length;
	uint8_t *temp;
	if (ParseLengthAndMagic(data, data_len, AudioStreamBasicDescriptionMagic, &temp, &length) < 0)
		return -1;

	desc->AudioDescription = NewAudioStreamBasicDescriptionFromBytes(data+8, length-8);
	return 0;
}

int parseVideoDimension(uint8_t *data, size_t data_len, uint32_t *out_width, uint32_t *out_height, uint8_t **next_data_pointer)
{
	uint8_t *remaining_bytes;
	size_t key_len;
	if (ParseLengthAndMagic(data, data_len, VideoDimensionMagic, &remaining_bytes, &key_len) < 0)
		return -1;

	if (key_len != 16)
	{
		usbmuxd_log(LL_ERROR, "invalid length for video dimension: %d", key_len);
		return -1;
	}

	*out_width = byteutils_get_int(data, 8);
	*out_height = byteutils_get_int(data, 12);
	*next_data_pointer = data+key_len;
	return 0;
}

int parseCodec(uint8_t *data, size_t data_len, uint32_t *out_codec, uint8_t **next_data_pointer)
{
	uint8_t *remaining_bytes;
	size_t key_len;

	if (ParseLengthAndMagic(data, data_len, CodecMagic, &remaining_bytes, &key_len) < 0)
		return -1;

	if (key_len != 12)
	{
		usbmuxd_log(LL_ERROR, "invalid length for codec: %d", key_len);
		return -1;
	}

	*out_codec = byteutils_get_int(data, 8);
	*next_data_pointer = data+key_len;
	return 0;
}

int extractPPS(list_t *extensions, uint8_t *pps, size_t *pps_len, uint8_t *sps, size_t *sps_len)
{
	(void)sps;

	if (!extensions)
		return -1;

	struct IndexKeyEntry *entry = getIndexValue(extensions, 49);
	if (!entry)
		return -1;

	struct IndexKeyEntry *PPSEntry = getIndexValue(entry->children, 105);
	if (!PPSEntry)
		return -1;

	uint8_t *data = PPSEntry->serialized_data+8;
	size_t p_len = data[7];
	size_t s_len = data[10 + p_len];
	*pps_len = p_len + s_len + 8;
	memcpy(pps, start_code, 4);
	memcpy(pps+4, data+8, p_len);
	memcpy(pps+4+p_len, start_code, 4);
	memcpy(pps+4+p_len+4, data + 11 + p_len, s_len);
	*sps_len = 0;

	return 0; 
}

void clearFormatDescriptor(struct FormatDescriptor *fd)
{
	if (fd && fd->Extensions)
		list_destroy(fd->Extensions);
}
