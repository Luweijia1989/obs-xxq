#include "cmsamplebuf.h"
#include "log.h"
#include "byteutils.h"
#include <memory.h>

extern uint8_t start_code[];

bool NewCMSampleBufferFromBytesAudio(uint8_t *data, size_t data_len, struct CMSampleBuffer *sb_buffer)
{
	return NewCMSampleBufferFromBytes(data, data_len, MediaTypeSound, sb_buffer);
}

bool NewCMSampleBufferFromBytesVideo(uint8_t *data, size_t data_len, struct CMSampleBuffer *sb_buffer)
{
	return NewCMSampleBufferFromBytes(data, data_len, MediaTypeVideo, sb_buffer);
}

bool NewCMSampleBufferFromBytes(uint8_t *data, size_t data_len, uint32_t media_type, struct CMSampleBuffer *sb_buffer)
{
	if (!sb_buffer)
		return false;

	sb_buffer->MediaType = media_type;
	sb_buffer->HasFormatDescription = false;

	uint8_t *next_data_pointer;
	size_t key_len;
	if (ParseLengthAndMagic(data, data_len, sbuf, &next_data_pointer, &key_len) < 0)
		return false;

	if (key_len > data_len)
	{
		usbmuxd_log(LL_ERROR, "less data (%d bytes) in buffer than expected (%d bytes)", data_len, key_len);
		return false;
	}

	while (next_data_pointer < data + data_len)
	{
		uint32_t type = byteutils_get_int(next_data_pointer, 4);
		switch (type)
		{
		case opts:
			if (NewCMTimeFromBytes(&sb_buffer->OutputPresentationTimestamp, next_data_pointer + 8) < 0)
				return false;

			next_data_pointer += 32;
			break;
		case stia:
		{
			list_t *l = parseStia(&next_data_pointer, data_len - (next_data_pointer - data));
			if (!l)
				return false;
			sb_buffer->SampleTimingInfoArray = l;
		}
		break;
		case sdat:
		{
			size_t length;
			if (ParseLengthAndMagic(next_data_pointer, data_len - (next_data_pointer - data), sdat, &next_data_pointer, &length) < 0)
				return false;

			if (media_type == MediaTypeSound)
			{
				sb_buffer->SampleData_len = length - 8;
				sb_buffer->SampleData = calloc(1, sb_buffer->SampleData_len);
				memcpy(sb_buffer->SampleData, next_data_pointer, sb_buffer->SampleData_len);
			}
			else
			{
				if (!FillCMSampleBufferData(next_data_pointer, length - 8, sb_buffer))
					return false;
			}
			
			next_data_pointer += length - 8;
		}
		break;
		case nsmp:
		{
			size_t length;
			if (ParseLengthAndMagic(next_data_pointer, data_len - (next_data_pointer - data), nsmp, &next_data_pointer, &length) < 0)
				return false;

			if (length != 12)
			{
				usbmuxd_log(LL_ERROR, "invalid length for nsmp %d, should be 12", length);
				return false;
			}

			sb_buffer->NumSamples = byteutils_get_int(next_data_pointer, 0);
			next_data_pointer += 4;
		}
		break;
		case ssiz:
		{
			size_t *res = parseSampleSizeArray(&next_data_pointer, data_len - (next_data_pointer - data));
			if (!res)
				return false;

			sb_buffer->SampleSizes = res;
		}
		break;
		case FormatDescriptorMagic:
		{
			sb_buffer->HasFormatDescription = true;
			size_t fdscLength = byteutils_get_int(next_data_pointer, 0);
			if (NewFormatDescriptorFromBytes(next_data_pointer, fdscLength, &sb_buffer->FormatDescription) < 0)
				return false;

			next_data_pointer += fdscLength;
		}
		break;
		case satt:
		{
			size_t attachmentsLength = byteutils_get_int(next_data_pointer, 0);
			list_t *res = NewIndexDictFromBytesWithCustomMarker(next_data_pointer, attachmentsLength, satt);
			if (!res)
				return false;

			sb_buffer->Attachments = res;
			next_data_pointer += attachmentsLength;
		}
		break;
		case sary:
		{
			size_t saryLength = byteutils_get_int(next_data_pointer, 0);
			list_t *res = NewIndexDictFromBytes(next_data_pointer + 8, saryLength-8);
			if (!res)
				return false;

			sb_buffer->Sary = res;
			next_data_pointer += saryLength;
		}
		break;
		default:
			usbmuxd_log(LL_ERROR, "unknown magic type");
			return false;
		}
	}
	return true;
}

bool FillCMSampleBufferData(uint8_t *data, size_t data_len,
			    struct CMSampleBuffer *sb_buffer)
{
	int total_size = 0;
	uint8_t *slice = data;
	while (slice < data + data_len) {
		size_t length = byteutils_get_int_be(slice, 0);
		total_size += length + 4;
		slice += length + 4;
	}

	//here we assume that one encoded video packet size less than 10M, if total_size larger than 10M, return false to avoid crash
	if (total_size > 10485760)
		return false;

	sb_buffer->SampleData_len = total_size;
	if (total_size) {
		sb_buffer->SampleData = (uint8_t *)calloc(1, total_size);
		uint8_t *ptr = sb_buffer->SampleData;
		slice = data;
		size_t copy_index = 0;
		while (slice < data + data_len) {
			memcpy(ptr + copy_index, start_code, 4);
			copy_index += 4;
			size_t length = byteutils_get_int_be(slice, 0);
			total_size += length + 4;
			memcpy(ptr + copy_index, slice + 4, length);
			copy_index += length;
			slice += length + 4;
		}
	}

	return true;
}

list_t * parseStia(uint8_t **data, size_t data_len)
{
	uint8_t *data_p = *data;

	uint8_t *next_data_pointer;
	size_t stiaLength;
	if (ParseLengthAndMagic(data_p, data_len, stia, &next_data_pointer, &stiaLength) < 0)
		return NULL;

	stiaLength -= 8;

	size_t numEntries = stiaLength / cmSampleTimingInfoLength;
	size_t modulus = stiaLength % cmSampleTimingInfoLength;
	if (modulus != 0)
	{
		usbmuxd_log(LL_ERROR, "error parsing stia, too many bytes: %d", modulus);
		return NULL;
	}

	data_p += 8;
	list_t *res = list_new();
	bool has_error = false;
	for (int i = 0; i < numEntries; i++)
	{
		int index = i * cmSampleTimingInfoLength;
		struct CMTime duration;
		if (NewCMTimeFromBytes(&duration, data_p + index) < 0)
		{
			has_error = true;
			break;
		}

		struct CMTime presentationTimeStamp;
		if (NewCMTimeFromBytes(&presentationTimeStamp, data_p + index + CMTimeLengthInBytes) < 0)
		{
			has_error = true;
			break;
		}
		struct CMTime decodeTimeStamp;
		if (NewCMTimeFromBytes(&decodeTimeStamp, data_p + index + CMTimeLengthInBytes * 2) < 0)
		{
			has_error = true;
			break;
		}
		struct CMSampleTimingInfo *sampleTimingInfo = calloc(1, sizeof(struct CMSampleTimingInfo));
		sampleTimingInfo->Duration = duration;
		sampleTimingInfo->PresentationTimeStamp = presentationTimeStamp;
		sampleTimingInfo->DecodeTimeStamp = decodeTimeStamp;
		list_node_t *node = list_node_new_struct(sampleTimingInfo);
		list_rpush(res, node);
	}

	if (has_error)
	{
		list_destroy(res);
		return NULL;
	}
	*data = data_p + stiaLength;
	return res;
}

size_t * parseSampleSizeArray(uint8_t **data, size_t data_len)
{
	size_t ssizLength;
	uint8_t *next_data_pointer;
	if (ParseLengthAndMagic(*data, data_len, ssiz, &next_data_pointer, &ssizLength) < 0)
		return NULL;

	ssizLength -= 8;
	size_t numEntries = ssizLength / 4;
	size_t modulus = ssizLength % 4;

	if (modulus != 0)
	{
		usbmuxd_log(LL_ERROR, "error parsing samplesizearray, too many bytes: %d", modulus);
		return NULL;
	}

	size_t *res = calloc(1, sizeof(size_t)*numEntries);
	uint8_t *data_p = *data + 8;
	for (int i = 0; i < numEntries; i++)
	{
		int index = 4 * i;
		res[i] = byteutils_get_int(data_p, index + i * 4);
	}

	*data = data_p + ssizLength;
	return res;
}

void clearCMSampleBuffer(struct CMSampleBuffer *buffer)
{
	if (!buffer)
		return;

	if (buffer->SampleTimingInfoArray)
		list_destroy(buffer->SampleTimingInfoArray);

	if (buffer->SampleData)
		free(buffer->SampleData);

	if (buffer->SampleSizes)
		free(buffer->SampleSizes);

	if (buffer->Attachments)
		list_destroy(buffer->Attachments);

	if (buffer->Sary)
		list_destroy(buffer->Sary);

	clearFormatDescriptor(&buffer->FormatDescription);
}

