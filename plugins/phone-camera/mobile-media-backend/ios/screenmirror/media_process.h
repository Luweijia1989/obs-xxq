#pragma once

//#include "qt_configuration.h"
#include "byteutils.h"
#include "parse_header.h"
#include "dict.h"
#include "cmclock.h"
#include "asyn_feed.h"
#include <util/circlebuf.h>
#include <media-io/audio-io.h>

struct mirror_info {
	struct CMClock clock;
	struct CMClock localAudioClock;
	CFTypeID deviceAudioClockRef;
	CFTypeID needClockRef;
	uint8_t *needMessage;
	size_t needMessageLen;
	size_t audioSamplesReceived;
	size_t videoSamplesReceived;
	bool firstAudioTimeTaken;
	double sampleRate;
	bool firstPingPacket;

	struct CMTime startTimeDeviceAudioClock;
	struct CMTime startTimeLocalAudioClock;
	struct CMTime lastEatFrameReceivedDeviceAudioClockTime;
	struct CMTime lastEatFrameReceivedLocalAudioClockTime;

	struct circlebuf m_audioDataCacheBuf;
	char *m_audioPopBuffer;
	struct circlebuf m_mediaCache;

	struct usb_device *dev;
};

struct mirror_info *create_mirror_info(struct usb_device *dev);
void destory_mirror_info(struct mirror_info *info);

void sendAudioInfo(struct mirror_info *info, uint32_t sampleRate, enum speaker_layout layout);
void sendData(struct mirror_info *info, struct CMSampleBuffer *buf);

void handleSyncPacket(void *ctx, uint8_t *buf, int length);
void handleAsyncPacket(void *ctx, uint8_t *buf, int length);
void onMirrorData(void *ctx, uint8_t *data, uint32_t size);

void mirror_end(void *ctx);
