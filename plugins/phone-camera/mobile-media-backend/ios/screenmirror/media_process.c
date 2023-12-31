#include "media_process.h"
#include "usb.h"
#include "log.h"
#include "packet.h"
#include <math.h>

struct mirror_info *create_mirror_info(struct usb_device *dev)
{
	struct mirror_info *info = malloc(sizeof(struct mirror_info));
	memset(info, 0, sizeof(struct mirror_info));

	info->dev = dev;
	circlebuf_init(&info->m_audioDataCacheBuf);
	info->m_audioPopBuffer = malloc(4096 * sizeof(uint8_t));
	circlebuf_init(&info->m_mediaCache);

	return info;
}

void destory_mirror_info(struct mirror_info *info)
{
	if (!info)
		return;

	if (info->needMessage)
		free(info->needMessage);

	if (info->m_audioPopBuffer)
		free(info->m_audioPopBuffer);

	circlebuf_free(&info->m_audioDataCacheBuf);
	circlebuf_free(&info->m_mediaCache);
}

//type 0->video 1->audio 2->state
static void send_media_data(struct usb_device *dev, uint8_t type, int64_t timestamp, uint8_t *payload, size_t payload_size)
{
	struct media_header header = {0};
	uint32_t size = sizeof(struct media_header) + payload_size;
	char *send_buffer = malloc(size);
	header.type = type;
	header.payload_size = payload_size;
	header.timestamp = timestamp;
	memcpy(send_buffer, &header, sizeof(struct media_header));
	memcpy(send_buffer + sizeof(struct media_header), payload, payload_size);
	usb_send_media_data(dev, send_buffer, size);
	free(send_buffer);
}

// state 0->start 1->stop
void send_state(struct usb_device *dev, int state)
{
	struct media_header header = {0};
	uint32_t size = sizeof(struct media_header) + sizeof(int);
	char *send_buffer = malloc(size);
	header.type = 2;
	header.payload_size = sizeof(int);
	header.timestamp = 0;
	memcpy(send_buffer, &header, sizeof(struct media_header));
	memcpy(send_buffer + sizeof(struct media_header), &state, sizeof(int));
	usb_send_media_data(dev, send_buffer, size);
	free(send_buffer);
}

void sendAudioInfo(struct mirror_info *info, uint32_t sampleRate, enum speaker_layout layout)
{
	uint32_t buf[3] = {0};
	buf[0] = AUDIO_FORMAT_16BIT;
	buf[1] = sampleRate;
	buf[2] = layout;
	send_media_data(info->dev, 1, 0x8000000000000000, buf, sizeof(buf));
	info->audio_frame_duration = audio_frames_to_ns(sampleRate, get_audio_frames(AUDIO_FORMAT_16BIT, layout, 4096));
}

void sendData(struct mirror_info *info, struct CMSampleBuffer *buf)
{
	if (buf->HasFormatDescription) {
		send_media_data(info->dev, 0, 0x8000000000000000, buf->FormatDescription.PPS, buf->FormatDescription.PPS_len);
	}

	if (buf->SampleData_len <= 0)
		return;

	bool isAudio = buf->MediaType == MediaTypeSound;
	if (isAudio) {
		circlebuf_push_back(&info->m_audioDataCacheBuf, buf->SampleData, buf->SampleData_len);
		while (true) {
			if (info->m_audioDataCacheBuf.size >= 4096) {
				circlebuf_pop_front(&info->m_audioDataCacheBuf, info->m_audioPopBuffer, 4096);
				if (info->audio_frame_duration) { // must send audio info first, then audio data
					if (!info->audio_start_timestamp)
						info->audio_start_timestamp = os_gettime_ns();
					send_media_data(info->dev, 1, info->audio_start_timestamp + info->audio_frame_duration * (info->audio_frames_sent++),
							info->m_audioPopBuffer, 4096);
				}
			} else
				break;
		}
	} else {
		if (buf->OutputPresentationTimestamp.CMTimeValue > 17446044073700192000)
			buf->OutputPresentationTimestamp.CMTimeValue = 0;

		send_media_data(info->dev, 0, os_gettime_ns(), buf->SampleData, buf->SampleData_len);
	}
}

void handleSyncPacket(void *ctx, uint8_t *buf, int length)
{
	struct mirror_info *mi = ctx;

	uint32_t type = byteutils_get_int(buf, 12);
	switch (type) {
	case CWPA: {
		struct SyncCwpaPacket cwpaPacket;
		newSyncCwpaPacketFromBytes(buf, &cwpaPacket);
		CFTypeID clockRef = cwpaPacket.DeviceClockRef + 1000;

		mi->localAudioClock = NewCMClockWithHostTime(clockRef);
		mi->deviceAudioClockRef = cwpaPacket.DeviceClockRef;
		uint8_t *device_info;
		size_t device_info_len;
		list_t *hpd1_dict = CreateHpd1DeviceInfoDict();
		NewAsynHpd1Packet(hpd1_dict, &device_info, &device_info_len);
		list_destroy(hpd1_dict);
		usbmuxd_log(LL_DEBUG, "Sending ASYN HPD1");
		usb_send(mi->dev, device_info, device_info_len, 1);

		uint8_t *cwpa_reply;
		size_t cwpa_reply_len;
		CwpaPacketNewReply(&cwpaPacket, clockRef, &cwpa_reply, &cwpa_reply_len);
		usbmuxd_log(LL_DEBUG, "Send CWPA-RPLY {correlation:%p, clockRef:%p}", cwpaPacket.CorrelationID, clockRef);
		usb_send(mi->dev, cwpa_reply, cwpa_reply_len, 1);

		uint8_t *hpa1;
		size_t hpa1_len;
		list_t *hpa1_dict = CreateHpa1DeviceInfoDict();
		NewAsynHpa1Packet(hpa1_dict, cwpaPacket.DeviceClockRef, &hpa1, &hpa1_len);
		list_destroy(hpa1_dict);
		usbmuxd_log(LL_DEBUG, "Sending ASYN HPA1");
		usb_send(mi->dev, hpa1, hpa1_len, 1);
	} break;
	case AFMT: {
		usbmuxd_log(LL_DEBUG, "AFMT");
		struct SyncAfmtPacket afmtPacket = {0};
		if (NewSyncAfmtPacketFromBytes(buf, length, &afmtPacket) == 0) { //处理音频格式
			sendAudioInfo(mi, (uint32_t)afmtPacket.AudioStreamInfo.SampleRate, (enum speaker_layout)afmtPacket.AudioStreamInfo.ChannelsPerFrame);
		}

		uint8_t *afmt;
		size_t afmt_len;
		AfmtPacketNewReply(&afmtPacket, &afmt, &afmt_len);
		usbmuxd_log(LL_DEBUG, "Send AFMT-REPLY {correlation:%p}", afmtPacket.CorrelationID);
		usb_send(mi->dev, afmt, afmt_len, 1);
	} break;
	case CVRP: {
		usbmuxd_log(LL_DEBUG, "CVRP");
		struct SyncCvrpPacket cvrp_packet = {0};
		NewSyncCvrpPacketFromBytes(buf, length, &cvrp_packet);

		if (cvrp_packet.Payload) {
			list_node_t *node;
			list_iterator_t *it = list_iterator_new(cvrp_packet.Payload, LIST_HEAD);
			while ((node = list_iterator_next(it))) {
				struct StringKeyEntry *entry = (struct StringKeyEntry *)node->val;
				if (entry->typeMagic == FormatDescriptorMagic) {
					struct FormatDescriptor *fd = (struct FormatDescriptor *)entry->children;
					mi->sampleRate = fd->AudioDescription.SampleRate;
					break;
				}
			}
			list_iterator_destroy(it);
		}

		const double EPS = 1e-6;
		if (fabs(mi->sampleRate - 0.f) > EPS)
			mi->sampleRate = 48000.0f;

		mi->needClockRef = cvrp_packet.DeviceClockRef;
		AsynNeedPacketBytes(mi->needClockRef, &mi->needMessage, &mi->needMessageLen);
		uint8_t *copy = malloc(mi->needMessageLen);
		memcpy(copy, mi->needMessage, mi->needMessageLen);
		usb_send(mi->dev, copy, mi->needMessageLen, 1);

		CFTypeID clockRef2 = cvrp_packet.DeviceClockRef + 0x1000AF;
		uint8_t *send_data;
		size_t send_data_len;
		SyncCvrpPacketNewReply(&cvrp_packet, clockRef2, &send_data, &send_data_len);
		usb_send(mi->dev, send_data, send_data_len, 1);
		clearSyncCvrpPacket(&cvrp_packet);
	} break;
	case OG: {
		struct SyncOgPacket og_packet = {0};
		NewSyncOgPacketFromBytes(buf, length, &og_packet);
		uint8_t *send_data;
		size_t send_data_len;
		SyncOgPacketNewReply(&og_packet, &send_data, &send_data_len);
		usb_send(mi->dev, send_data, send_data_len, 1);
	} break;
	case CLOK: {
		usbmuxd_log(LL_DEBUG, "CLOCK");
		struct SyncClokPacket clock_packet = {0};
		NewSyncClokPacketFromBytes(buf, length, &clock_packet);
		CFTypeID clockRef = clock_packet.ClockRef + 0x10000;
		mi->clock = NewCMClockWithHostTime(clockRef);

		uint8_t *send_data;
		size_t send_data_len;
		SyncClokPacketNewReply(&clock_packet, clockRef, &send_data, &send_data_len);
		usb_send(mi->dev, send_data, send_data_len, 1);
	} break;
	case TIME: {
		usbmuxd_log(LL_DEBUG, "TIME");
		struct SyncTimePacket time_packet = {0};
		NewSyncTimePacketFromBytes(buf, length, &time_packet);
		struct CMTime time_to_send = GetTime(&mi->clock);

		uint8_t *send_data;
		size_t send_data_len;
		SyncTimePacketNewReply(&time_packet, &time_to_send, &send_data, &send_data_len);
		usb_send(mi->dev, send_data, send_data_len, 1);
	} break;
	case SKEW: {
		struct SyncSkewPacket skew_packet = {0};
		int res = NewSyncSkewPacketFromBytes(buf, length, &skew_packet);
		if (res < 0)
			usbmuxd_log(LL_DEBUG, "Error parsing SYNC SKEW packet");

		CalculateSkew(&mi->startTimeLocalAudioClock, &mi->lastEatFrameReceivedLocalAudioClockTime, &mi->startTimeDeviceAudioClock,
			      &mi->lastEatFrameReceivedDeviceAudioClockTime);

		uint8_t *send_data;
		size_t send_data_len;
		SyncSkewPacketNewReply(&skew_packet, mi->sampleRate, &send_data, &send_data_len);
		usb_send(mi->dev, send_data, send_data_len, 1);
	} break;
	case STOP: {
		struct SyncStopPacket stop_packet = {0};
		int res = NewSyncStopPacketFromBytes(buf, length, &stop_packet);
		if (res < 0)
			usbmuxd_log(LL_DEBUG, "Error parsing SYNC STOP packet");

		uint8_t *send_data;
		size_t send_data_len;
		SyncStopPacketNewReply(&stop_packet, &send_data, &send_data_len);
		usb_send(mi->dev, send_data, send_data_len, 1);
	} break;
	default:
		usbmuxd_log(LL_DEBUG, "received unknown sync packet type"); //stop
		break;
	}
}

void handleAsyncPacket(void *ctx, uint8_t *buf, int length)
{
	struct mirror_info *mi = ctx;

	uint32_t type = byteutils_get_int(buf, 12);
	switch (type) {
	case EAT: {
		mi->audioSamplesReceived++;
		struct AsynCmSampleBufPacket eatPacket = {0};
		bool success = NewAsynCmSampleBufPacketFromBytes(buf, length, &eatPacket);

		if (!success) {
			usbmuxd_log(LL_DEBUG, "unknown eat");
		} else {
			if (!mi->firstAudioTimeTaken) {
				mi->startTimeDeviceAudioClock = eatPacket.CMSampleBuf.OutputPresentationTimestamp;
				mi->startTimeLocalAudioClock = GetTime(&mi->localAudioClock);
				mi->lastEatFrameReceivedDeviceAudioClockTime = eatPacket.CMSampleBuf.OutputPresentationTimestamp;
				mi->lastEatFrameReceivedLocalAudioClockTime = mi->startTimeLocalAudioClock;
				mi->firstAudioTimeTaken = true;
			} else {
				mi->lastEatFrameReceivedDeviceAudioClockTime = eatPacket.CMSampleBuf.OutputPresentationTimestamp;
				mi->lastEatFrameReceivedLocalAudioClockTime = GetTime(&mi->localAudioClock);
			}

			sendData(mi, &eatPacket.CMSampleBuf);

			if (mi->audioSamplesReceived % 100 == 0) {
				usbmuxd_log(LL_DEBUG, "RCV Audio Samples:%d", mi->audioSamplesReceived);
			}
			clearAsynCmSampleBufPacket(&eatPacket);
		}
	} break;
	case FEED: {
		uint8_t *copy = malloc(mi->needMessageLen);
		memcpy(copy, mi->needMessage, mi->needMessageLen);

		struct AsynCmSampleBufPacket acsbp = {0};
		bool success = NewAsynCmSampleBufPacketFromBytes(buf, length, &acsbp);
		if (!success) {
			usb_send(mi->dev, copy, mi->needMessageLen, 1);
		} else {
			mi->videoSamplesReceived++;

			sendData(mi, &acsbp.CMSampleBuf);

			if (mi->videoSamplesReceived % 500 == 0) {
				usbmuxd_log(LL_DEBUG, "RCV Video Samples:%d ", mi->videoSamplesReceived);
				mi->videoSamplesReceived = 0;
			}
			usb_send(mi->dev, copy, mi->needMessageLen, 1);
			clearAsynCmSampleBufPacket(&acsbp);
		}
	} break;
	case SPRP: {
		usbmuxd_log(LL_DEBUG, "SPRP");
	} break;
	case TJMP:
		usbmuxd_log(LL_DEBUG, "TJMP");
		break;
	case SRAT:
		usbmuxd_log(LL_DEBUG, "SART");
		break;
	case TBAS:
		usbmuxd_log(LL_DEBUG, "TBAS");
		break;
	case RELS:
		usbmuxd_log(LL_DEBUG, "RELS");
		break;
	default:
		usbmuxd_log(LL_DEBUG, "UNKNOWN");
		break;
	}
}

static void frameReceived(void *ctx, uint8_t *data, uint32_t size)
{
	struct mirror_info *mi = ctx;

	uint32_t type = byteutils_get_int(data, 0);
	switch (type) {
	case PingPacketMagic:
		if (!mi->firstPingPacket) {
			uint8_t *cmd = malloc(16);
			cmd[0] = 0x10;
			cmd[1] = 0x00;
			cmd[2] = 0x00;
			cmd[3] = 0x00;
			cmd[4] = 0x67;
			cmd[5] = 0x6e;
			cmd[6] = 0x69;
			cmd[7] = 0x70;
			cmd[8] = 0x01;
			cmd[9] = 0x00;
			cmd[10] = 0x00;
			cmd[11] = 0x00;
			cmd[12] = 0x01;
			cmd[13] = 0x00;
			cmd[14] = 0x00;
			cmd[15] = 0x00;
			usb_send(mi->dev, cmd, 16, 1);
			mi->firstPingPacket = true;
		}
		break;
	case SyncPacketMagic:
		handleSyncPacket(ctx, data, size);
		break;
	case AsynPacketMagic:
		handleAsyncPacket(ctx, data, size);
		break;
	default:
		break;
	}
}

void onMirrorData(void *ctx, uint8_t *data, uint32_t size)
{
	struct mirror_info *mi = ctx;

	circlebuf_push_back(&mi->m_mediaCache, data, size);
	while (true) {
		size_t byte_remain = mi->m_mediaCache.size;
		if (byte_remain < 4)
			break;

		unsigned char buff_len[4] = {0};
		circlebuf_peek_front(&mi->m_mediaCache, buff_len, 4);
		uint32_t len = byteutils_get_int(buff_len, 0);
		if (byte_remain >= len) {
			//有一帧
			unsigned char *temp_buf = (unsigned char *)malloc(len);
			circlebuf_pop_front(&mi->m_mediaCache, temp_buf, len);
			frameReceived(ctx, temp_buf + 4, len - 4);
			free(temp_buf);
		} else
			break;
	}
}

struct android_mirror_info *create_android_mirror_info(struct usb_device *dev)
{
	struct android_mirror_info *info = malloc(sizeof(struct android_mirror_info));
	memset(info, 0, sizeof(struct android_mirror_info));

	info->dev = dev;
	circlebuf_init(&info->media_cache);

	return info;
}

void destroy_android_mirror_info(struct android_mirror_info *info)
{
	if (!info)
		return;

	circlebuf_free(&info->media_cache);
	if (info->cache_buf)
		free(info->cache_buf);

	free(info);
}

static bool handle_android_media_data(void *ctx)
{
	struct android_mirror_info *mi = ctx;

	size_t headerSize = 4 + 8 + 1;

	if (mi->media_cache.size < headerSize)
		return false;

	size_t pktSize = 0;
	circlebuf_peek_front(&mi->media_cache, &pktSize, 4);

	size_t totalSize = pktSize + headerSize;
	if (mi->media_cache.size < totalSize)
		return false;

	if (mi->cache_buf_size < headerSize + pktSize) {
		mi->cache_buf_size = headerSize + pktSize;
		mi->cache_buf = realloc(mi->cache_buf, mi->cache_buf_size);
	}

	circlebuf_pop_front(&mi->media_cache, mi->cache_buf, headerSize);
	int type = (mi->cache_buf)[4];
	int64_t pts = 0;
	memcpy(&pts, mi->cache_buf + 5, 8);
	circlebuf_pop_front(&mi->media_cache, mi->cache_buf, pktSize);

	if (type == 1) { //video
		if (pts == ((int64_t)UINT64_C(0x8000000000000000)) && !mi->audio_info_sent) {
			uint32_t buf[3] = {0};
			buf[0] = AUDIO_FORMAT_16BIT;
			buf[1] = 48000;
			buf[2] = SPEAKERS_STEREO;
			send_media_data(mi->dev, 1, 0x8000000000000000, (uint8_t *)buf, sizeof(buf));
			mi->audio_info_sent = true;
		}

		send_media_data(mi->dev, 0, pts, mi->cache_buf, pktSize);
	} else {
		if (mi->audio_info_sent)
			send_media_data(mi->dev, 1, pts, mi->cache_buf, pktSize);
	}
	return true;
}

void on_android_mirror_data(void *ctx, uint8_t *data, uint32_t size)
{
	struct android_mirror_info *mi = ctx;
	circlebuf_push_back(&mi->media_cache, data, size);

	while (1) {
		bool b = handle_android_media_data(ctx);

		if (b)
			continue;
		else
			break;
	}
}
