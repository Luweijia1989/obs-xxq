#include "stream.h"

#include <assert.h>
#include "config.h"
#include "util/buffer_util.h"
#include "util/log.h"
#include "../common-define.h"

#define BUFSIZE 0x10000

#define HEADER_SIZE 12
#define NO_PTS UINT64_C(-1)

static bool stream_recv_packet(struct stream *stream)
{
	// The video stream contains raw packets, without time information. When we
	// record, we retrieve the timestamps separately, from a "meta" header
	// added by the server before each raw packet.
	//
	// The "meta" header length is 12 bytes:
	// [. . . . . . . .|. . . .]. . . . . . . . . . . . . . . ...
	//  <-------------> <-----> <-----------------------------...
	//        PTS        packet        raw packet
	//                    size
	//
	// It is followed by <packet_size> bytes containing the packet/frame.

	uint8_t header[HEADER_SIZE];
	long long r = net_recv_all(stream->socket, header, HEADER_SIZE);
	if (r < HEADER_SIZE) {
		return false;
	}

	uint64_t pts = buffer_read64be(header);
	uint32_t len = buffer_read32be(&header[8]);
	assert(pts == NO_PTS || (pts & 0x8000000000000000) == 0);
	assert(len);

	uint8_t *buffer = calloc(1, len);
	r = net_recv_all(stream->socket, buffer, len);
	if (r < 0 || ((uint32_t)r) < len) {
		free(buffer);
		return false;
	}

	uint64_t packet_pts = pts != NO_PTS
				      ? (int64_t)pts
				      : ((int64_t)UINT64_C(0x8000000000000000));
	bool is_pps = packet_pts == ((int64_t)UINT64_C(0x8000000000000000));

	if (is_pps) {
		struct av_packet_info pack_info = {0};
		pack_info.size = sizeof(struct media_info);
		pack_info.type = FFM_MEDIA_INFO;

		struct media_info info = {0};
		info.pps_len = len;
		memcpy(info.pps, buffer, len);

		ipc_client_write_2(stream->ipc_client, &pack_info, sizeof(struct av_packet_info), &info, sizeof(struct media_info), INFINITE);
	} else {
		struct av_packet_info pack_info = {0};
		pack_info.size = len;
		pack_info.type = FFM_PACKET_VIDEO;
		pack_info.pts = 0;
		ipc_client_write_2(stream->ipc_client, &pack_info, sizeof(struct av_packet_info), buffer, len, INFINITE);
	}

	free(buffer);
	return true;
}

static void *run_stream(void *data)
{
	struct stream *stream = data;
	while (!stream->stop) {
		bool ok = stream_recv_packet(stream);
		if (!ok) {
			// end of stream
			break;
		}
	}

	LOGD("End of frames");
	return NULL;
}

void stream_init(struct stream *stream, socket_t socket,
		 struct IPCClient *client)
{
	stream->socket = socket;
	stream->ipc_client = client;
}

bool stream_start(struct stream *stream)
{
	LOGD("Starting stream thread");

	int perr = pthread_create(&stream->thread, NULL, run_stream, stream);
	if (perr != 0) {
		LOGC("Could not start stream thread");
		return false;
	}
	return true;
}

void stream_stop(struct stream *stream)
{
	stream->stop = true;
	stream_join(stream);
	memset(stream, 0, sizeof(struct stream));
}

void stream_join(struct stream *stream)
{
	pthread_join(stream->thread, NULL);
}
