#include <lusb0_usb.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include "qt_configuration.h"
#include "log.h"
#include "preflight.h"
#include "device.h"
#include "socket.h"
#include "utils.h"
#include "ringbuf.h"
#include "parse_header.h"
#include "cmclock.h"
#include "nsnumber.h"
#include "dict.h"
#include "byteutils.h"
#include "async_packet.h"
#include "cmsamplebuf.h"
#include "asyn_feed.h"
#include "consumer.h"
#include "ipc.h"
#include "../common-define.h"
#include <fcntl.h>
#include <io.h>

//#define STANDALONE

struct MessageProcessor {
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

	struct CMTime startTimeDeviceAudioClock;
	struct CMTime startTimeLocalAudioClock;
	struct CMTime lastEatFrameReceivedDeviceAudioClockTime;
	struct CMTime lastEatFrameReceivedLocalAudioClockTime;

	struct Consumer consumer;
};

struct usb_device_info {
	usb_dev_handle *device_handle;
	struct usb_device *device;
	char serial[256];
	uint8_t interface, ep_in, ep_out;
	uint8_t interface_fa, ep_in_fa, ep_out_fa;
	int wMaxPacketSize;
	int wMaxPacketSize_fa;
	int version;
	uint16_t rx_seq;
	uint16_t tx_seq;
	pthread_t thread_handle;
	void *bulk_read_context;
	bool read_exit;
	bool lock_down_read_exit;

	ringbuf_t usb_data_buffer;
	struct MessageProcessor mp;
	bool first_ping_packet;
	struct media_info m_info;
	bool has_video_received;
	uint64_t audio_offset;
	uint64_t last_audio_ts;

	HANDLE stop_signal;
};

struct usb_device_info app_device = {0};
bool should_exit = false;
bool in_progress = false;
struct IPCClient *ipc_client = NULL;
pthread_mutex_t exit_mutex;
HANDLE lock_down_event = INVALID_HANDLE_VALUE;

//////////////////////////////////////////////////////////////////////////////
usb_dev_handle *open_dev(struct usb_device **device)
{
	struct usb_bus *bus;
	struct usb_device *dev;

	for (bus = usb_get_busses(); bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor != VID_APPLE ||
			    dev->descriptor.idProduct < PID_RANGE_LOW ||
			    dev->descriptor.idProduct > PID_RANGE_MAX) {
				continue;
			}
			*device = dev;
			return usb_open(dev);
		}
	}
	return NULL;
}

static void handle_sync_packet(unsigned char *buf, uint32_t length)
{
	uint32_t type = byteutils_get_int(buf, 12);
	switch (type) {
	case CWPA: {
		usbmuxd_log(LL_INFO, "CWPA");
		struct SyncCwpaPacket cwpaPacket;
		int res = newSyncCwpaPacketFromBytes(buf, &cwpaPacket);
		CFTypeID clockRef = cwpaPacket.DeviceClockRef + 1000;

		app_device.mp.localAudioClock =
			NewCMClockWithHostTime(clockRef);
		app_device.mp.deviceAudioClockRef = cwpaPacket.DeviceClockRef;
		uint8_t *device_info;
		size_t device_info_len;
		list_t *hpd1_dict = CreateHpd1DeviceInfoDict();
		NewAsynHpd1Packet(hpd1_dict, &device_info, &device_info_len);
		list_destroy(hpd1_dict);
		usbmuxd_log(LL_NOTICE, "Sending ASYN HPD1");
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       device_info, device_info_len, 1000);

		uint8_t *cwpa_reply;
		size_t cwpa_reply_len;
		CwpaPacketNewReply(&cwpaPacket, clockRef, &cwpa_reply,
				   &cwpa_reply_len);
		usbmuxd_log(LL_NOTICE,
			    "Send CWPA-RPLY {correlation:%p, clockRef:%p}",
			    cwpaPacket.CorrelationID, clockRef);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       cwpa_reply, cwpa_reply_len, 1000);
		free(cwpa_reply);

		usbmuxd_log(LL_NOTICE, "Sending ASYN HPD1");
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       device_info, device_info_len, 1000);
		free(device_info);

		uint8_t *hpa1;
		size_t hpa1_len;
		list_t *hpa1_dict = CreateHpa1DeviceInfoDict();
		NewAsynHpa1Packet(hpa1_dict, cwpaPacket.DeviceClockRef, &hpa1,
				  &hpa1_len);
		list_destroy(hpa1_dict);
		usbmuxd_log(LL_NOTICE, "Sending ASYN HPA1");
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       hpa1, hpa1_len, 1000);
		free(hpa1);
	} break;
	case AFMT: {
		usbmuxd_log(LL_INFO, "AFMT");
		struct SyncAfmtPacket afmtPacket = {0};
		if (NewSyncAfmtPacketFromBytes(buf, length, &afmtPacket) == 0) {
			app_device.m_info.samples_per_sec =
				(uint32_t)afmtPacket.AudioStreamInfo.SampleRate;
			app_device.m_info.format = AUDIO_FORMAT_16BIT;
			app_device.m_info.speakers =
				afmtPacket.AudioStreamInfo.ChannelsPerFrame;
			app_device.m_info.bytes_per_frame =
				afmtPacket.AudioStreamInfo.BytesPerFrame;
		}

		uint8_t *afmt;
		size_t afmt_len;
		AfmtPacketNewReply(&afmtPacket, &afmt, &afmt_len);
		usbmuxd_log(LL_NOTICE, "Send AFMT-REPLY {correlation:%p}",
			    afmtPacket.CorrelationID);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       afmt, afmt_len, 1000);
		free(afmt);
	} break;
	case CVRP: {
		usbmuxd_log(LL_INFO, "CVRP");
		struct SyncCvrpPacket cvrp_packet = {0};
		NewSyncCvrpPacketFromBytes(buf, length, &cvrp_packet);

		if (cvrp_packet.Payload) {
			list_node_t *node;
			list_iterator_t *it = list_iterator_new(
				cvrp_packet.Payload, LIST_HEAD);
			while ((node = list_iterator_next(it))) {
				struct StringKeyEntry *entry = node->val;
				if (entry->typeMagic == FormatDescriptorMagic) {
					struct FormatDescriptor *fd =
						entry->children;
					app_device.mp.sampleRate =
						fd->AudioDescription.SampleRate;
					break;
				}
			}
			list_iterator_destroy(it);
		}

		const double EPS = 1e-6;
		if (fabs(app_device.mp.sampleRate - 0.f) > EPS)
			app_device.mp.sampleRate = 48000.0f;

		app_device.mp.needClockRef = cvrp_packet.DeviceClockRef;
		AsynNeedPacketBytes(app_device.mp.needClockRef,
				    &app_device.mp.needMessage,
				    &app_device.mp.needMessageLen);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       app_device.mp.needMessage,
			       app_device.mp.needMessageLen, 1000);

		CFTypeID clockRef2 = cvrp_packet.DeviceClockRef + 0x1000AF;
		uint8_t *send_data;
		size_t send_data_len;
		SyncCvrpPacketNewReply(&cvrp_packet, clockRef2, &send_data,
				       &send_data_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       send_data, send_data_len, 1000);
		free(send_data);
		clearSyncCvrpPacket(&cvrp_packet);
	} break;
	case OG: {
		struct SyncOgPacket og_packet = {0};
		NewSyncOgPacketFromBytes(buf, length, &og_packet);
		uint8_t *send_data;
		size_t send_data_len;
		SyncOgPacketNewReply(&og_packet, &send_data, &send_data_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case CLOK: {
		usbmuxd_log(LL_INFO, "CLOCK");
		struct SyncClokPacket clock_packet = {0};
		NewSyncClokPacketFromBytes(buf, length, &clock_packet);
		CFTypeID clockRef = clock_packet.ClockRef + 0x10000;
		app_device.mp.clock = NewCMClockWithHostTime(clockRef);

		uint8_t *send_data;
		size_t send_data_len;
		SyncClokPacketNewReply(&clock_packet, clockRef, &send_data,
				       &send_data_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case TIME: {
		usbmuxd_log(LL_INFO, "TIME");
		struct SyncTimePacket time_packet = {0};
		NewSyncTimePacketFromBytes(buf, length, &time_packet);
		struct CMTime time_to_send = GetTime(&app_device.mp.clock);

		uint8_t *send_data;
		size_t send_data_len;
		SyncTimePacketNewReply(&time_packet, &time_to_send, &send_data,
				       &send_data_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case SKEW: {
		struct SyncSkewPacket skew_packet = {0};
		int res = NewSyncSkewPacketFromBytes(buf, length, &skew_packet);
		if (res < 0)
			usbmuxd_log(LL_ERROR, "Error parsing SYNC SKEW packet");

		double skewValue = CalculateSkew(
			&app_device.mp.startTimeLocalAudioClock,
			&app_device.mp.lastEatFrameReceivedLocalAudioClockTime,
			&app_device.mp.startTimeDeviceAudioClock,
			&app_device.mp.lastEatFrameReceivedDeviceAudioClockTime);

		uint8_t *send_data;
		size_t send_data_len;
		SyncSkewPacketNewReply(&skew_packet, app_device.mp.sampleRate,
				       &send_data, &send_data_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case STOP: {
		struct SyncStopPacket stop_packet = {0};
		int res = NewSyncStopPacketFromBytes(buf, length, &stop_packet);
		if (res < 0)
			usbmuxd_log(LL_ERROR, "Error parsing SYNC STOP packet");

		uint8_t *send_data;
		size_t send_data_len;
		SyncStopPacketNewReply(&stop_packet, &send_data,
				       &send_data_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       send_data, send_data_len, 1000);
		free(send_data);
	} break;
	default:
		usbmuxd_log(LL_INFO,
			    "received unknown sync packet type"); //stop
		break;
	}
}

static void handle_async_packet(unsigned char *buf, uint32_t length)
{
	uint32_t type = byteutils_get_int(buf, 12);
	int dd = 0;
	switch (type) {
	case EAT: {
		usbmuxd_log(LL_INFO, "EAT");

		app_device.mp.audioSamplesReceived++;
		struct AsynCmSampleBufPacket eatPacket = {0};
		bool success = NewAsynCmSampleBufPacketFromBytes(buf, length,
								 &eatPacket);

		if (!success) {
			usbmuxd_log(LL_ERROR, "unknown eat");
		} else {
			if (!app_device.mp.firstAudioTimeTaken) {
				app_device.mp.startTimeDeviceAudioClock =
					eatPacket.CMSampleBuf
						.OutputPresentationTimestamp;
				app_device.mp.startTimeLocalAudioClock =
					GetTime(&app_device.mp.localAudioClock);
				app_device.mp
					.lastEatFrameReceivedDeviceAudioClockTime =
					eatPacket.CMSampleBuf
						.OutputPresentationTimestamp;
				app_device.mp
					.lastEatFrameReceivedLocalAudioClockTime =
					app_device.mp.startTimeLocalAudioClock;
				app_device.mp.firstAudioTimeTaken = true;
			} else {
				app_device.mp
					.lastEatFrameReceivedDeviceAudioClockTime =
					eatPacket.CMSampleBuf
						.OutputPresentationTimestamp;
				app_device.mp
					.lastEatFrameReceivedLocalAudioClockTime =
					GetTime(&app_device.mp.localAudioClock);
			}

			app_device.mp.consumer.consume(
				&eatPacket.CMSampleBuf,
				app_device.mp.consumer.consumer_ctx);

			if (app_device.mp.audioSamplesReceived % 100 == 0) {
				usbmuxd_log(LL_INFO, "RCV Audio Samples:%d",
					    app_device.mp.audioSamplesReceived);
			}
			clearAsynCmSampleBufPacket(&eatPacket);
		}
	} break;
	case FEED: {
		usbmuxd_log(LL_INFO, "FEED");

		struct AsynCmSampleBufPacket acsbp = {0};
		bool success =
			NewAsynCmSampleBufPacketFromBytes(buf, length, &acsbp);
		if (!success) {
			usb_bulk_write(app_device.device_handle,
				       app_device.ep_out_fa,
				       app_device.mp.needMessage,
				       app_device.mp.needMessageLen, 1000);
		} else {
			app_device.mp.videoSamplesReceived++;

			app_device.mp.consumer.consume(
				&acsbp.CMSampleBuf,
				app_device.mp.consumer.consumer_ctx);

			if (app_device.mp.videoSamplesReceived % 500 == 0) {
				usbmuxd_log(LL_INFO, "Rcv'd(%d) ",
					    app_device.mp.videoSamplesReceived);
				app_device.mp.videoSamplesReceived = 0;
			}
			usb_bulk_write(app_device.device_handle,
				       app_device.ep_out_fa,
				       app_device.mp.needMessage,
				       app_device.mp.needMessageLen, 1000);
			clearAsynCmSampleBufPacket(&acsbp);
		}
	} break;
	case SPRP: {
		usbmuxd_log(LL_INFO, "SPRP");
		//struct AsynSprpPacket as_packet = { 0 };
		//NewAsynSprpPacketFromBytes(buf, length, &as_packet); // 内存释放
	} break;
	case TJMP:
		usbmuxd_log(LL_INFO, "TJMP");
		dd = 1;
		break;
	case SRAT:
		usbmuxd_log(LL_INFO, "SART");
		//usb_bulk_write(app_device.device_handle, app_device.ep_out_fa, app_device.mp.needMessage, app_device.mp.needMessageLen, 1000);
		dd = 1;
		break;
	case TBAS:
		usbmuxd_log(LL_INFO, "TBAS");
		dd = 1;
		break;
	case RELS:
		usbmuxd_log(LL_INFO, "RELS");
		SetEvent(app_device.stop_signal);
		break;
	default:
		usbmuxd_log(LL_INFO, "UNKNOWN");
		break;
	}
}

static void frame_received(unsigned char *buf, uint32_t length)
{
	uint32_t type = byteutils_get_int(buf, 0);
	switch (type) {
	case PingPacketMagic:
		if (!app_device.first_ping_packet) {
			unsigned char cmd[] = {0x10, 0x00, 0x00, 0x00,
					       0x67, 0x6e, 0x69, 0x70,
					       0x01, 0x00, 0x00, 0x00,
					       0x01, 0x00, 0x00, 0x00};
			usb_bulk_write(app_device.device_handle,
				       app_device.ep_out_fa, cmd, 16, 1000);
			app_device.first_ping_packet = true;
		}
		break;
	case SyncPacketMagic:
		handle_sync_packet(buf, length);
		break;
	case AsynPacketMagic:
		handle_async_packet(buf, length);
		break;
	default:
		break;
	}
}

static void usb_extract_frame(unsigned char *buf, uint32_t length)
{
	if (!length)
		return;

	ringbuf_memcpy_into(app_device.usb_data_buffer, buf, length);
	while (true) {
		size_t byte_remain =
			ringbuf_bytes_used(app_device.usb_data_buffer);
		if (byte_remain < 4)
			break;

		unsigned char buff_len[4] = {0};
		ringbuf_memcpy_from_not_remove(buff_len,
					       app_device.usb_data_buffer, 4);
		size_t len = byteutils_get_int(buff_len, 0);
		if (byte_remain >= len) {
			//有一帧
			uint8_t *temp_buf = calloc(1, len);
			ringbuf_memcpy_from(temp_buf,
					    app_device.usb_data_buffer, len);
			frame_received(temp_buf + 4, len - 4);
			free(temp_buf);
		} else
			break;
	}
}

void *usb_read_thread(void *data)
{
	in_progress = true;
	usb_clear_halt(app_device.device_handle, app_device.ep_in_fa);
	usb_clear_halt(app_device.device_handle, app_device.ep_out_fa);

	int ret = usb_bulk_setup_async(app_device.device_handle,
				       &app_device.bulk_read_context,
				       app_device.ep_in_fa);

	uint8_t read_buffer[65536];
	while (!app_device.read_exit) {
		ret = usb_submit_async(app_device.bulk_read_context,
				       read_buffer, 65536);
		if (ret < 0)
			break;

		ret = usb_reap_async(app_device.bulk_read_context, INFINITE);
		if (ret < 0 && ret != -ETIMEDOUT)
			break;

		usb_extract_frame(read_buffer, ret);
	}
	usb_free_async(&app_device.bulk_read_context);
	app_device.bulk_read_context = NULL;
	in_progress = false;
	return NULL;
}

void *usb_read_lock_down_thread(void *data)
{
	uint8_t read_buffer[65536];
	int read_len = 0;
	while (!app_device.lock_down_read_exit) {
		read_len = usb_bulk_read(app_device.device_handle,
					 app_device.ep_in, read_buffer, 65536,
					 100);
		if (read_len < 0) {
			if (read_len != -116) {
				SetEvent(lock_down_event);
				break;
			}
			continue;
		}

		device_data_input(app_device.device_handle, read_buffer,
				  read_len);
	}
	return NULL;
}

void reset_app_device()
{
	if (!app_device.device_handle)
		return;

	CloseHandle(app_device.stop_signal);

	device_remove(app_device.device_handle);
	if (app_device.bulk_read_context)
		usb_cancel_async(app_device.bulk_read_context);
	app_device.read_exit = true;
	pthread_join(app_device.thread_handle, NULL);

	clearConsumer(&app_device.mp.consumer);

	usb_close(app_device.device_handle);

	free(app_device.mp.needMessage);
	ringbuf_free(&app_device.usb_data_buffer);
	memset(&app_device, 0, sizeof(struct usb_device_info));
	in_progress = false;
}

void closeSession()
{
	if (!app_device.device_handle)
		return;
	{
		uint8_t *d1;
		size_t d1_len;
		NewAsynHPA0(app_device.mp.deviceAudioClockRef, &d1, &d1_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       d1, d1_len, 1000);
		free(d1);
	}

	{
		uint8_t *d1;
		size_t d1_len;
		NewAsynHPD0(&d1, &d1_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       d1, d1_len, 1000);
		free(d1);
	}

	DWORD res = WaitForSingleObject(app_device.stop_signal, 3000);
	if (res == WAIT_TIMEOUT) {
		usbmuxd_log(LL_INFO, "Timed out waiting for device closing");
		return;
	}

	{
		uint8_t *d1;
		size_t d1_len;
		NewAsynHPD0(&d1, &d1_len);
		usb_bulk_write(app_device.device_handle, app_device.ep_out_fa,
			       d1, d1_len, 1000);
		free(d1);
		usbmuxd_log(LL_INFO, "OK. Ready to release USB Device.");
	}

	usb_release_interface(app_device.device_handle,
			      app_device.interface_fa);
	usb_release_interface(app_device.device_handle, app_device.interface);

	usb_control_msg(app_device.device_handle, 0x40, 0x52, 0x00, 0x00, NULL,
			0, 1000 /* LIBUSB_DEFAULT_TIMEOUT */);
	usb_set_configuration(app_device.device_handle, 4);
}

void exit_app()
{
	pthread_mutex_lock(&exit_mutex);
	should_exit = true;

	if (in_progress) {
		closeSession();
	}
	reset_app_device();

	pthread_mutex_unlock(&exit_mutex);
}

void pipeConsume(struct CMSampleBuffer *buf, void *c)
{
	if (buf->HasFormatDescription) {
		memcpy(app_device.m_info.pps, buf->FormatDescription.PPS,
		       buf->FormatDescription.PPS_len);
		app_device.m_info.pps_len = buf->FormatDescription.PPS_len;
		app_device.m_info.sps_len = buf->FormatDescription.SPS_len;
		struct av_packet_info pack_info = {0};
		pack_info.size = sizeof(struct media_info);
		pack_info.type = FFM_MEDIA_INFO;
#ifndef STANDALONE
		ipc_client_write(ipc_client, &pack_info,
				 sizeof(struct av_packet_info), INFINITE);
		ipc_client_write(ipc_client, &app_device.m_info,
				 sizeof(struct media_info), INFINITE);
#endif
	}

	if (buf->SampleData_len <= 0)
		return;

	struct av_packet_info pack_info = {0};
	pack_info.size = buf->SampleData_len;
	pack_info.type = buf->MediaType == MediaTypeSound ? FFM_PACKET_AUDIO
							  : FFM_PACKET_VIDEO;
	if (buf->OutputPresentationTimestamp.CMTimeValue > 17446044073700192000)
		buf->OutputPresentationTimestamp.CMTimeValue = 0;
	pack_info.pts = 0;
	if (pack_info.type == FFM_PACKET_AUDIO) {
		app_device.last_audio_ts =
			buf->OutputPresentationTimestamp.CMTimeValue * 1000.0 /
			buf->OutputPresentationTimestamp.CMTimeScale;
		/*pack_info.pts =
			app_device.last_audio_ts - app_device.audio_offset;*/
	} else {
		/*pack_info.pts = buf->OutputPresentationTimestamp.CMTimeValue *
				1000.0 /
				buf->OutputPresentationTimestamp.CMTimeScale;*/
		if (!app_device.has_video_received) {
			app_device.audio_offset =
				app_device.last_audio_ts - pack_info.pts;
			app_device.has_video_received = true;
		}
	}

	if (pack_info.type == FFM_PACKET_AUDIO &&
	    !app_device.has_video_received)
		return;

	pack_info.pts = pack_info.pts * 1000000;
#ifndef STANDALONE
	ipc_client_write(ipc_client, &pack_info, sizeof(struct av_packet_info),
			 INFINITE);
	ipc_client_write(ipc_client, buf->SampleData, buf->SampleData_len,
			 INFINITE);
#endif
}

struct Consumer PipeWriter()
{
	struct Consumer pipe_write = {
		.consume = pipeConsume, .stop = NULL, .consumer_ctx = NULL};
	return pipe_write;
}

int usb_device_discover()
{
	pthread_mutex_lock(&exit_mutex);
	if (should_exit) {
		pthread_mutex_unlock(&exit_mutex);
		return 0;
	}
	int res = 0;
	usb_find_busses();  /* find all busses */
	usb_find_devices(); /* find all connected devices */

	struct usb_device *device = NULL;
	usb_dev_handle *handle = open_dev(&device);

	if (!handle) {
		if (app_device.device_handle)
			reset_app_device(); // stop
	} else {
		if (device == app_device.device) {
			usb_close(handle);
		} else {
			if (app_device.device_handle)
				reset_app_device();
			app_device.stop_signal =
				CreateEvent(NULL, false, false, NULL);
			app_device.device = device;
			app_device.device_handle = handle;
			app_device.usb_data_buffer =
				ringbuf_new(1024 * 1024 * 2);
			app_device.mp.consumer = PipeWriter();
			res = 1;
		}
	}
	pthread_mutex_unlock(&exit_mutex);
	return res;
}

void *stdin_read_thread(void *data)
{
	uint8_t buf[1024] = {0};
	while (true) {
		int read_len =
			fread(buf, 1, 1024,
			      stdin); // read 0 means parent has been stopped
		if (read_len) {
			if (buf[0] == 1) {
				exit_app();
				break;
			}
		} else {
			exit_app();
			break;
		}
	}
	return NULL;
}

static void create_stdin_thread()
{
	pthread_t th;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_create(&th, &attr, stdin_read_thread, NULL);
}

int main(void)
{
	SetErrorMode(SEM_FAILCRITICALERRORS);
	_setmode(_fileno(stdin), O_BINARY);

	create_stdin_thread();

#ifndef STANDALONE
	freopen("NUL", "w", stderr);
	ipc_client_create(&ipc_client);
#endif
	lock_down_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	client_init();
	device_init();
	pthread_t socket_t = create_socket_thread();
	usb_init(); /* initialize the library */
	pthread_mutex_init(&exit_mutex, NULL);

	while (!should_exit) {
		Sleep(200);
		if (!usb_device_discover())
			continue;
		else {
			send_status(ipc_client, MIRROR_START);
			usb_get_string_simple(
				app_device.device_handle,
				app_device.device->descriptor.iSerialNumber,
				app_device.serial, sizeof(app_device.serial));

			int muxConfigIndex = -1;
			int qtConfigIndex = -1;

			findConfigurations(&app_device.device->descriptor,
					   app_device.device, &muxConfigIndex,
					   &qtConfigIndex);

			if (qtConfigIndex == -1) {
				int res = usb_control_msg(
					app_device.device_handle, 0x40, 0x52,
					0x00, 0x02, NULL, 0,
					5000 /* LIBUSB_DEFAULT_TIMEOUT */);
				Sleep(300);
				continue;
			}

			byte config = -1;
			int res = usb_control_msg(
				app_device.device_handle,
				USB_RECIP_DEVICE | USB_ENDPOINT_IN,
				USB_REQ_GET_CONFIGURATION, 0, 0, &config, 1,
				5000 /* LIBUSB_DEFAULT_TIMEOUT */);
			if (config != qtConfigIndex) {
				usb_set_configuration(app_device.device_handle,
						      qtConfigIndex);
			}

			for (int m = 0;
			     m <
			     app_device.device->descriptor.bNumConfigurations;
			     m++) {
				struct usb_config_descriptor config =
					app_device.device->config[m];
				for (int n = 0; n < config.bNumInterfaces;
				     n++) {
					struct usb_interface_descriptor intf =
						config.interface[n]
							.altsetting[0];
					if (intf.bInterfaceClass ==
						    INTERFACE_CLASS &&
					    intf.bInterfaceSubClass ==
						    INTERFACE_SUBCLASS &&
					    intf.bInterfaceProtocol ==
						    INTERFACE_PROTOCOL) {
						if (intf.bNumEndpoints != 2) {
							continue;
						}
						if ((intf.endpoint[0]
							     .bEndpointAddress &
						     0x80) ==
							    USB_ENDPOINT_OUT &&
						    (intf.endpoint[1]
							     .bEndpointAddress &
						     0x80) == USB_ENDPOINT_IN) {
							app_device.interface =
								intf.bInterfaceNumber;
							app_device.ep_out =
								intf.endpoint[0]
									.bEndpointAddress;
							app_device
								.wMaxPacketSize =
								intf.endpoint[0]
									.wMaxPacketSize;
							app_device.ep_in =
								intf.endpoint[1]
									.bEndpointAddress;
						} else if (
							(intf.endpoint[1]
								 .bEndpointAddress &
							 0x80) ==
								USB_ENDPOINT_OUT &&
							(intf.endpoint[0]
								 .bEndpointAddress &
							 0x80) ==
								USB_ENDPOINT_IN) {
							app_device.interface =
								intf.bInterfaceNumber;
							app_device.ep_out =
								intf.endpoint[1]
									.bEndpointAddress;
							app_device
								.wMaxPacketSize =
								intf.endpoint[1]
									.wMaxPacketSize;
							app_device.ep_in =
								intf.endpoint[0]
									.bEndpointAddress;
						} else {
						}
					}

					if (intf.bInterfaceClass ==
						    INTERFACE_CLASS &&
					    intf.bInterfaceSubClass ==
						    INTERFACE_QUICKTIMECLASS) {
						if (intf.bNumEndpoints != 2) {
							continue;
						}
						if ((intf.endpoint[0]
							     .bEndpointAddress &
						     0x80) ==
							    USB_ENDPOINT_OUT &&
						    (intf.endpoint[1]
							     .bEndpointAddress &
						     0x80) == USB_ENDPOINT_IN) {
							app_device.interface_fa =
								intf.bInterfaceNumber;
							app_device.ep_out_fa =
								intf.endpoint[0]
									.bEndpointAddress;
							app_device
								.wMaxPacketSize_fa =
								intf.endpoint[0]
									.wMaxPacketSize;
							app_device.ep_in_fa =
								intf.endpoint[1]
									.bEndpointAddress;
						} else if (
							(intf.endpoint[1]
								 .bEndpointAddress &
							 0x80) ==
								USB_ENDPOINT_OUT &&
							(intf.endpoint[0]
								 .bEndpointAddress &
							 0x80) ==
								USB_ENDPOINT_IN) {
							app_device.interface_fa =
								intf.bInterfaceNumber;
							app_device.ep_out_fa =
								intf.endpoint[1]
									.bEndpointAddress;
							app_device
								.wMaxPacketSize_fa =
								intf.endpoint[1]
									.wMaxPacketSize;
							app_device.ep_in_fa =
								intf.endpoint[0]
									.bEndpointAddress;
						} else {
						}
					}
				}
			}

			res = usb_claim_interface(app_device.device_handle,
						  app_device.interface);
			res = usb_claim_interface(app_device.device_handle,
						  app_device.interface_fa);

			device_add(app_device.device, app_device.device_handle,
				   app_device.ep_out, app_device.serial);

			pthread_t th;
			pthread_create(&th, NULL, usb_read_lock_down_thread,
				       NULL);

			WaitForSingleObject(lock_down_event, INFINITE);
			app_device.lock_down_read_exit = true;
			pthread_join(th, NULL);

			int perr = pthread_create(&app_device.thread_handle,
						  NULL, usb_read_thread, NULL);
			if (perr != 0) {
				usbmuxd_log(
					LL_ERROR,
					"ERROR: failed to start usb read thread for device %s: %s (%d)",
					app_device.serial, strerror(perr),
					perr);
			}
		}
	}
	send_status(ipc_client, MIRROR_STOP);
	pthread_mutex_destroy(&exit_mutex);
	pthread_join(socket_t, NULL);

	usbmuxd_log(LL_NOTICE, "usbmuxd shutting down");
	device_kill_connections();
	device_shutdown();
	client_shutdown();
	CloseHandle(lock_down_event);
	ipc_client_destroy(&ipc_client);
	usbmuxd_log(LL_NOTICE, "Shutdown complete");

	return 0;
}
