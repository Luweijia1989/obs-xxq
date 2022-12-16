#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cmclock.h"
#include "formatdescriptor.h"
#include "list.h"

typedef uint32_t CMItemCount;

#define sbuf 0x73627566 
#define opts 0x6F707473 
#define stia 0x73746961 
#define sdat 0x73646174 
#define satt 0x73617474 
#define sary 0x73617279 
#define ssiz 0x7373697A 
#define nsmp 0x6E736D70 
#define cmSampleTimingInfoLength 72

#pragma pack(1)
struct CMSampleTimingInfo
{
	struct CMTime Duration;
	struct CMTime PresentationTimeStamp;
	struct CMTime DecodeTimeStamp;
};
#pragma pack()

#pragma pack(1)
struct CMSampleBuffer
{
	struct CMTime OutputPresentationTimestamp;
	struct FormatDescriptor FormatDescription;
	bool HasFormatDescription;
	CMItemCount NumSamples;                        
	list_t *SampleTimingInfoArray;
	uint8_t *SampleData;
	size_t SampleData_len;
	size_t *SampleSizes;
	list_t *Attachments;
	list_t *Sary;
	uint32_t MediaType;
};
#pragma pack()

bool NewCMSampleBufferFromBytesAudio(uint8_t *data, size_t data_len, struct CMSampleBuffer *sb_buffer);

bool NewCMSampleBufferFromBytesVideo(uint8_t *data, size_t data_len, struct CMSampleBuffer *sb_buffer);

bool NewCMSampleBufferFromBytes(uint8_t *data, size_t data_len, uint32_t media_type, struct CMSampleBuffer *sb_buffer);

bool FillCMSampleBufferData(uint8_t *data, size_t data_len, struct CMSampleBuffer *sb_buffer);

list_t *parseStia(uint8_t **data, size_t data_len);

size_t *parseSampleSizeArray(uint8_t **data, size_t data_len);

void clearCMSampleBuffer(struct CMSampleBuffer *buffer);
