#include <util/threading.h>
#include <obs-module.h>
#include <obs.h>
#include <stdio.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/circlebuf.h>
#include <obs-avc.h>
#include <srt/srt.h>
#include <srt/udt.h>
#include <map>

#include "libmpeg/include/mpeg-ps.h"
#include "libmpeg/include/mpeg-ts.h"

#define doLog(level, format, ...) \
	blog(level, "[srt output]:" format, ##__VA_ARGS__)

#define FFMIN(a, b) ((a) > (b) ? (b) : (a))

#define MILLISECOND_DEN 1000
static int32_t get_ms_time(struct encoder_packet *packet, int64_t val)
{
	return (int32_t)(val * MILLISECOND_DEN / packet->timebase_den);
}

enum AACObjectType {
	ObjectTypeReserved = 0,

	// Table 1.1 - Audio Object Type definition
	// @see @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23
	ObjectTypeAacMain = 1,
	ObjectTypeAacLC = 2,
	ObjectTypeAacSSR = 3,

	// AAC HE = LC+SBR
	ObjectTypeAacHE = 5,
	// AAC HEv2 = LC+SBR+PS
	ObjectTypeAacHEV2 = 29,
};

enum AACProfile {
	ProfileReserved = 3,

	// @see 7.1 Profiles, aac-iso-13818-7.pdf, page 40
	ProfileMain = 0,
	ProfileLC = 1,
	ProfileSSR = 2,
};

AACProfile srs_codec_aac_rtmp2ts(AACObjectType object_type)
{
	switch (object_type) {
	case ObjectTypeAacMain:
		return ProfileMain;
	case ObjectTypeAacHE:
	case ObjectTypeAacHEV2:
	case ObjectTypeAacLC:
		return ProfileLC;
	case ObjectTypeAacSSR:
		return ProfileSSR;
	default:
		return ProfileReserved;
	}
}

enum SRTMode {
	SRT_MODE_CALLER = 0,
	SRT_MODE_LISTENER = 1,
	SRT_MODE_RENDEZVOUS = 2
};

typedef struct SRTContext {
	SRTSOCKET fd;
	int eid;

	int64_t maxbw; //[-1, INT64_MAX]
	int payload_size;        //[-1, SRT_LIVE_MAX_PAYLOAD_SIZE]
	enum SRTMode mode;
	int sndbuf;     //[-1, INT_MAX]
	char *streamid;
	SRT_TRANSTYPE transtype;
} SRTContext;

struct srt_output {
	obs_output_t *output;
	pthread_mutex_t mutex;
	os_event_t *stop_event;
	SRTContext srtContext;
	void *ts;

	volatile bool active;
	volatile bool connecting;
	volatile bool disconnected;
	volatile bool encode_error;
	pthread_t connect_thread;
	pthread_t send_thread;

	struct dstr path;
	os_sem_t *send_sem;
	uint64_t stop_ts;
	uint64_t shutdown_timeout_ts;
	bool got_first_video;
	int32_t start_dts_offset;

	int max_shutdown_time_sec;

	struct circlebuf packets;
};

size_t srt_strlcpy(char *dst, const char *src, size_t size)
{
	size_t len = 0;
	while (++len < size && *src)
		*dst++ = *src++;
	if (len <= size)
		*dst = 0;
	return len + strlen(src) - 1;
}

void srt_url_split(char *proto, int proto_size, char *authorization,
		   int authorization_size, char *hostname, int hostname_size,
		   int *port_ptr, char *path, int path_size, const char *url)
{
	const char *p, *ls, *ls2, *at, *at2, *col, *brk;

	if (port_ptr)
		*port_ptr = -1;
	if (proto_size > 0)
		proto[0] = 0;
	if (authorization_size > 0)
		authorization[0] = 0;
	if (hostname_size > 0)
		hostname[0] = 0;
	if (path_size > 0)
		path[0] = 0;

	/* parse protocol */
	if ((p = strchr(url, ':'))) {
		srt_strlcpy(proto, url, FFMIN(proto_size, p + 1 - url));
		p++; /* skip ':' */
		if (*p == '/')
			p++;
		if (*p == '/')
			p++;
	} else {
		/* no protocol means plain filename */
		srt_strlcpy(path, url, path_size);
		return;
	}

	/* separate path from hostname */
	ls = strchr(p, '/');
	ls2 = strchr(p, '?');
	if (!ls)
		ls = ls2;
	else if (ls && ls2)
		ls = FFMIN(ls, ls2);
	if (ls)
		srt_strlcpy(path, ls, path_size);
	else
		ls = &p[strlen(p)]; // XXX

	/* the rest is hostname, use that to parse auth/port */
	if (ls != p) {
		/* authorization (user[:pass]@hostname) */
		at2 = p;
		while ((at = strchr(p, '@')) && at < ls) {
			srt_strlcpy(authorization, at2,
				    FFMIN(authorization_size, at + 1 - at2));
			p = at + 1; /* skip '@' */
		}

		if (*p == '[' && (brk = strchr(p, ']')) && brk < ls) {
			/* [host]:port */
			srt_strlcpy(hostname, p + 1,
				    FFMIN(hostname_size, brk - p));
			if (brk[1] == ':' && port_ptr)
				*port_ptr = atoi(brk + 2);
		} else if ((col = strchr(p, ':')) && col < ls) {
			srt_strlcpy(hostname, p,
				    FFMIN(col + 1 - p, hostname_size));
			if (port_ptr)
				*port_ptr = atoi(col + 1);
		} else
			srt_strlcpy(hostname, p,
				    FFMIN(ls + 1 - p, hostname_size));
	}
}

static int find_info_tag(char *arg, int arg_size, const char *tag1,
			 const char *info)
{
	const char *p;
	char tag[128], *q;

	p = info;
	if (*p == '?')
		p++;
	for (;;) {
		q = tag;
		while (*p != '\0' && *p != '=' && *p != '&') {
			if ((q - tag) < sizeof(tag) - 1)
				*q++ = *p;
			p++;
		}
		*q = '\0';
		q = arg;
		if (*p == '=') {
			p++;
			while (*p != '&' && *p != '\0') {
				if ((q - arg) < arg_size - 1) {
					if (*p == '+')
						*q++ = ' ';
					else
						*q++ = *p;
				}
				p++;
			}
		}
		*q = '\0';
		if (!strcmp(tag, tag1))
			return 1;
		if (*p != '&')
			break;
		p++;
	}
	return 0;
}

void logHandler(void *opaque, int level, const char *file, int line,
		    const char *area, const char *message)
{
	doLog(LOG_INFO, "{libsrt log_level %d} %s", level, message);
}

static inline size_t num_buffered_packets(struct srt_output *stream)
{
	return stream->packets.size / sizeof(struct encoder_packet);
}

static inline void free_packets(struct srt_output *stream)
{
	size_t num_packets;

	pthread_mutex_lock(&stream->mutex);

	num_packets = num_buffered_packets(stream);
	if (num_packets)
		doLog(LOG_INFO, "Freeing %d remaining packets",
		      (int)num_packets);

	while (stream->packets.size) {
		struct encoder_packet packet;
		circlebuf_pop_front(&stream->packets, &packet, sizeof(packet));
		obs_encoder_packet_release(&packet);
	}
	pthread_mutex_unlock(&stream->mutex);
}

static inline bool active(struct srt_output *stream)
{
	return os_atomic_load_bool(&stream->active);
}

static inline bool connecting(struct srt_output *stream)
{
	return os_atomic_load_bool(&stream->connecting);
}

static inline bool disconnected(struct srt_output *stream)
{
	return os_atomic_load_bool(&stream->disconnected);
}

static inline bool stopping(struct srt_output *stream)
{
	return os_event_try(stream->stop_event) != EAGAIN;
}

static const char *srt_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Srt Encoding Output";
}

static int libsrt_neterrno()
{
	int err = srt_getlasterror(NULL);
	doLog(LOG_ERROR, "error %s", srt_getlasterror_str());
	return err;
}

static bool libsrt_write(void *stream, uint8_t *buf, size_t size)
{
	SRTContext *s = &((struct srt_output *)stream)->srtContext;
	int ret = srt_sendmsg(s->fd, (const char *)buf, (int)size, -1, 0);
	bool success = (ret != SRT_ERROR);
	if (!success) {
		libsrt_neterrno();
	}
	return success;
}

static int libsrt_setsockopt(int fd, SRT_SOCKOPT optname,
			     const char *optnamestr, const void *optval,
			     int optlen)
{
	if (srt_setsockopt(fd, 0, optname, optval, optlen) < 0) {
		doLog(LOG_ERROR, "failed to set option %s on socket: %s\n",
		      optnamestr, srt_getlasterror_str());
		return -1;
	}
	return 0;
}

static int libsrt_set_options_pre(struct srt_output *stream, int fd)
{
	SRTContext *s = &stream->srtContext;
	int yes = 1;

	if ((s->mode == SRT_MODE_RENDEZVOUS &&
	     libsrt_setsockopt(fd, SRTO_RENDEZVOUS, "SRTO_RENDEZVOUS", &yes,
			       sizeof(yes)) < 0) ||
	    (s->transtype != SRTT_INVALID &&
	     libsrt_setsockopt(fd, SRTO_TRANSTYPE, "SRTO_TRANSTYPE",
			       &s->transtype, sizeof(s->transtype)) < 0) ||
	    (s->maxbw >= 0 &&
	     libsrt_setsockopt(fd, SRTO_MAXBW, "SRTO_MAXBW", &s->maxbw,
			       sizeof(s->maxbw)) < 0) ||
	    (s->sndbuf >= 0 &&
	     libsrt_setsockopt(fd, SRTO_SNDBUF, "SRTO_SNDBUF", &s->sndbuf,
			       sizeof(s->sndbuf)) < 0) ||
	    (s->streamid &&
	     libsrt_setsockopt(fd, SRTO_STREAMID, "SRTO_STREAMID", s->streamid,
			       strlen(s->streamid)) < 0) ||
	    (s->payload_size >= 0 &&
	     libsrt_setsockopt(fd, SRTO_PAYLOADSIZE, "SRTO_PAYLOADSIZE",
			       &s->payload_size,
			       sizeof(s->payload_size)) < 0) ||
	    (libsrt_setsockopt(fd, SRTO_SENDER, "SRTO_SENDER", &yes,
			       sizeof(yes)) < 0)) {
		return -1;
	}
	return 0;
}

static int libsrt_socket_nonblock(int socket, int enable)
{
	int ret =
		srt_setsockopt(socket, 0, SRTO_SNDSYN, &enable, sizeof(enable));
	if (ret < 0)
		return ret;
	return srt_setsockopt(socket, 0, SRTO_RCVSYN, &enable, sizeof(enable));
}

static int libsrt_network_wait_fd(int eid, int fd, int write)
{
	int ret, len = 1;
	int modes = write ? SRT_EPOLL_OUT : SRT_EPOLL_IN;
	SRTSOCKET ready[1];

	if (srt_epoll_add_usock(eid, fd, &modes) < 0)
		return libsrt_neterrno();
	if (write) {
		ret = srt_epoll_wait(eid, 0, 0, ready, &len, 100, 0, 0, 0, 0);
	} else {
		ret = srt_epoll_wait(eid, ready, &len, 0, 0, 100, 0, 0, 0, 0);
	}
	if (ret < 0) {
		ret = libsrt_neterrno();
	} else {
		ret = 0;
	}
	if (srt_epoll_remove_usock(eid, fd) < 0)
		return libsrt_neterrno();
	return ret;
}

static int libsrt_network_wait_fd_timeout(int eid, int fd, int write,
					  int64_t timeout)
{
	int ret;
	int64_t wait_start = 0;

	while (1) {
		ret = libsrt_network_wait_fd(eid, fd, write);
		if (ret != SRT_EASYNCRCV)
			return ret;
		if (timeout > 0) {
			if (!wait_start)
				wait_start = os_gettime_ns() / 1000;
			else if (os_gettime_ns() / 1000 - wait_start > timeout)
				return SRT_ETIMEOUT;
		}
	}
}

static int libsrt_listen_connect(int eid, int fd, const struct sockaddr *addr,
				 socklen_t addrlen, int timeout,
				 const char *uri, int will_try_next)
{
	int ret = OBS_OUTPUT_SUCCESS;

	if (libsrt_socket_nonblock(fd, 1) < 0)
		doLog(LOG_ERROR, "libsrt socket nonblock failed\n");

	if ((ret = srt_connect(fd, addr, addrlen))) {
		ret = libsrt_neterrno();
		switch (ret) {
		case SRT_EASYNCRCV: {
			ret = libsrt_network_wait_fd_timeout(eid, fd, 1,
							     timeout);
			if (ret < 0)
				return OBS_OUTPUT_CONNECT_FAILED;
			ret = srt_getlasterror(NULL);
			const char *buf = srt_getlasterror_str();
			if (ret != 0) {
				if (will_try_next)
					doLog(LOG_WARNING,
					      "Connection to %s failed (%s), trying next address\n",
					      uri, buf);
				else
					doLog(LOG_ERROR,
					      "Connection to %s failed: %s\n",
					      uri, buf);
			}
			srt_clearlasterror();
		}
		default:
			return OBS_OUTPUT_INVALID_STREAM;
		}
	}

	return ret;
}

static int libsrt_connect(struct srt_output *stream)
{
	if (dstr_is_empty(&stream->path)) {
		doLog(LOG_ERROR, "srt URL is empty");
		return OBS_OUTPUT_BAD_PATH;
	}

	doLog(LOG_INFO, "Connecting to srt URL %s...", stream->path.array);

	SRTContext *s = &stream->srtContext;
	const char *uri = stream->path.array;
	const char *p;
	char buf[256];

	/* SRT options (srt/srt.h) */
	p = strchr(uri, '?');
	if (p && find_info_tag(buf, sizeof(buf), "streamid", p)) {
		bfree(s->streamid);
		s->streamid = bstrdup(buf);
	}

	struct addrinfo hints = {0}, *ai, *cur_ai;
	int port, fd = -1;
	int ret;
	char hostname[1024], proto[1024], path[1024];
	char portstr[10];
	int open_timeout = 5000;
	int eid;

	eid = srt_epoll_create();
	if (eid < 0) {
		libsrt_neterrno();
		return OBS_OUTPUT_CONNECT_FAILED;
	}
	s->eid = eid;

	srt_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
		      &port, path, sizeof(path), uri);

	if (port <= 0 || port >= 65536) {
		doLog(LOG_ERROR, "Port missing in uri\n");
		return OBS_OUTPUT_CONNECT_FAILED;
	}

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	snprintf(portstr, sizeof(portstr), "%d", port);
	if (s->mode == SRT_MODE_LISTENER)
		hints.ai_flags |= AI_PASSIVE;
	ret = getaddrinfo(hostname[0] ? hostname : NULL, portstr, &hints, &ai);
	if (ret) {
		doLog(LOG_ERROR, "Failed to resolve hostname %s", hostname);
		return OBS_OUTPUT_CONNECT_FAILED;
	}

	cur_ai = ai;

restart:

	fd = srt_socket(cur_ai->ai_family, cur_ai->ai_socktype, 0);
	if (fd < 0) {
		ret = libsrt_neterrno();
		doLog(LOG_WARNING, "srt_socket < 0\n");
		goto fail;
	}

	if ((ret = libsrt_set_options_pre(stream, fd)) < 0) {
		doLog(LOG_WARNING, "libsrt_set_options_pre failed\n");
		goto fail;
	}

	if ((ret = libsrt_listen_connect(s->eid, fd, cur_ai->ai_addr,
					 cur_ai->ai_addrlen, open_timeout, uri,
					 !!cur_ai->ai_next)) < 0) {
		doLog(LOG_WARNING, "libsrt_listen_connect failed\n");
		goto fail;
	}

	doLog(LOG_INFO, "Connection to %s successful", stream->path.array);

	s->fd = fd;

	freeaddrinfo(ai);
	return OBS_OUTPUT_SUCCESS;

fail:
	srt_epoll_release(eid);
	if (cur_ai->ai_next) {
		/* Retry with the next sockaddr */
		cur_ai = cur_ai->ai_next;
		if (fd >= 0)
			srt_close(fd);
		ret = 0;
		goto restart;
	}

	if (fd >= 0)
		srt_close(fd);
	freeaddrinfo(ai);

	return OBS_OUTPUT_CONNECT_FAILED;
}

static int libsrt_open(struct srt_output *stream)
{
	if (stopping(stream)) {
		pthread_join(stream->send_thread, NULL);
	}

	free_packets(stream);

	obs_service_t *service = obs_output_get_service(stream->output);
	if (!service)
		return false;

	os_atomic_set_bool(&stream->disconnected, false);
	os_atomic_set_bool(&stream->encode_error, false);
	stream->got_first_video = false;

	dstr_copy(&stream->path, obs_service_get_url(service));
	dstr_depad(&stream->path);

	return true;
}

static inline bool get_next_packet(struct srt_output *stream,
				   struct encoder_packet *packet)
{
	bool new_packet = false;

	pthread_mutex_lock(&stream->mutex);
	if (stream->packets.size) {
		circlebuf_pop_front(&stream->packets, packet,
				    sizeof(struct encoder_packet));
		new_packet = true;
	}
	pthread_mutex_unlock(&stream->mutex);

	return new_packet;
}

static inline bool can_shutdown_stream(struct srt_output *stream,
				       struct encoder_packet *packet)
{
	uint64_t cur_time = os_gettime_ns();
	bool timeout = cur_time >= stream->shutdown_timeout_ts;

	if (timeout)
		doLog(LOG_INFO,
		      "Stream shutdown timeout reached (%d second(s))",
		      stream->max_shutdown_time_sec);

	return timeout || packet->sys_dts_usec >= (int64_t)stream->stop_ts;
}

static void *ts_alloc(void * /*param*/, size_t bytes)
{
	static char s_buffer[188];
	assert(bytes <= sizeof(s_buffer));
	return s_buffer;
}

static void ts_free(void * /*param*/, void * /*packet*/)
{
	return;
}

static int ts_write(void *param, const void *packet, size_t bytes)
{
	bool ret = libsrt_write(param, (uint8_t *)packet, bytes);
	return ret ? 0 : -1;
}

static int ts_stream(void *ts, int codecid, const void *data, size_t bytes)
{
	static std::map<int, int> streams;
	std::map<int, int>::const_iterator it = streams.find(codecid);
	if (streams.end() != it)
		return it->second;

	int i = mpeg_ts_add_stream(ts, codecid, data, bytes);
	streams[codecid] = i;
	return i;
}

static int onAVData(void *ts, int codec, const void *data, size_t bytes,
		    unsigned int pts, unsigned int dts, int flags)
{
	static char s_pts[64], s_dts[64];
	static uint32_t v_pts = 0, v_dts = 0;
	static uint32_t a_pts = 0, a_dts = 0;
	int ret = 0;
	if (STREAM_AUDIO_AAC == codec) {
		pts = (a_pts && pts < a_pts) ? a_pts : pts;
		dts = (a_dts && dts < a_dts) ? a_dts : dts;
		ret = mpeg_ts_write(ts, ts_stream(ts, codec, NULL, 0), 0, pts * 90,
			      dts * 90, data, bytes);

		a_pts = pts;
		a_dts = dts;
	} else if (STREAM_VIDEO_H264 == codec) {
		dts = (a_dts && dts < v_dts) ? v_dts : dts;
		ret = mpeg_ts_write(ts, ts_stream(ts, codec, NULL, 0),
			      0x01 & flags ? 1 : 0, pts * 90, dts * 90, data,
			      bytes);

		v_pts = pts;
		v_dts = dts;
	}

	return ret;
}

static bool send_packet(struct srt_output *stream,
			struct encoder_packet *packet, bool is_header,
			size_t idx)
{
	int ret = 0;
	int32_t dts_offset = is_header ? 0 : stream->start_dts_offset;

	if (packet->type == OBS_ENCODER_VIDEO) {
		bool hasSps = false, hasKeyFrame = false;

		const uint8_t *nal_start, *nal_end;
		const uint8_t *end = packet->data + packet->size;
		int type;

		nal_start = obs_avc_find_startcode(packet->data, end);
		while (true) {
			while (nal_start < end && !*(nal_start++))
				;

			if (nal_start == end)
				break;

			type = nal_start[0] & 0x1F;

			if (type == OBS_NAL_SLICE_IDR)
				hasKeyFrame = true;

			if (type == OBS_NAL_SPS || type == OBS_NAL_PPS)
				hasSps = true;

			nal_end = obs_avc_find_startcode(nal_start, end);
			nal_start = nal_end;
		}

		int64_t offset = packet->pts - packet->dts;
		int32_t pts = get_ms_time(packet, packet->dts) - dts_offset;
		int32_t dts = pts + get_ms_time(packet, offset);

		std::vector<uint8_t> payload;
		if (hasKeyFrame && !hasSps) {
			uint8_t *header;
			size_t size;

			obs_output_t *context = stream->output;
			obs_encoder_t *vencoder =
				obs_output_get_video_encoder(context);
			obs_encoder_get_extra_data(vencoder, &header, &size);

			payload.insert(payload.end(), header, header + size);
		}

		payload.insert(payload.end(), packet->data,
			       packet->data + packet->size);

		ret = onAVData(stream->ts, STREAM_VIDEO_H264, payload.data(),
			 payload.size(), pts, dts, hasKeyFrame ? 1 : 0);
	} else if (packet->type == OBS_ENCODER_AUDIO) {
		uint8_t *header;
		size_t size;

		obs_output_t *context = stream->output;
		obs_encoder_t *aencoder =
			obs_output_get_audio_encoder(context, idx);
		obs_encoder_get_extra_data(aencoder, &header, &size);

		if (size <= 0)
			return false;

		uint8_t profile_ObjectType = header[0];
		uint8_t samplingFrequencyIndex = header[1];

		uint8_t aac_channels = (samplingFrequencyIndex >> 3) & 0x0f;
		samplingFrequencyIndex = ((profile_ObjectType << 1) & 0x0e) |
					 ((samplingFrequencyIndex >> 7) & 0x01);
		profile_ObjectType = (profile_ObjectType >> 3) & 0x1f;

		// set the aac sample rate.
		uint8_t aac_sample_rate = samplingFrequencyIndex;

		// convert the object type in sequence header to aac profile of ADTS.
		AACObjectType aac_object = (AACObjectType)profile_ObjectType;
		if (aac_object == ObjectTypeReserved) {
			return false;
		}

		int32_t pts = get_ms_time(packet, packet->dts) - dts_offset;
		int32_t dts = pts;
		int32_t frame_length = packet->size + 7;

		uint8_t adts_header[7] = {0xff, 0xf9, 0x00, 0x00,
					  0x00, 0x0f, 0xfc};
		// profile, 2bits
		AACProfile aac_profile = srs_codec_aac_rtmp2ts(aac_object);
		adts_header[2] = (aac_profile << 6) & 0xc0;
		// sampling_frequency_index 4bits
		adts_header[2] |= (aac_sample_rate << 2) & 0x3c;
		// channel_configuration 3bits
		adts_header[2] |= (aac_channels >> 2) & 0x01;
		adts_header[3] = (aac_channels << 6) & 0xc0;
		// frame_length 13bits
		adts_header[3] |= (frame_length >> 11) & 0x03;
		adts_header[4] = (frame_length >> 3) & 0xff;
		adts_header[5] = ((frame_length << 5) & 0xe0);
		// adts_buffer_fullness; //11bits
		adts_header[5] |= 0x1f;

		std::vector<uint8_t> payload;
		payload.insert(payload.end(), adts_header, adts_header + 7);
		payload.insert(payload.end(), packet->data,
			       packet->data + packet->size);

		ret = onAVData(stream->ts, STREAM_AUDIO_AAC, payload.data(),
			 payload.size(), pts, dts, 0);
	}

	if (is_header)
		bfree(packet->data);
	else
		obs_encoder_packet_release(packet);

	return ret == 0;
}

static void *send_thread(void *data)
{
	struct srt_output *stream = (struct srt_output *)data;

	os_set_thread_name("srt-stream: send_thread");

	while (os_sem_wait(stream->send_sem) == 0) {
		struct encoder_packet packet;

		if (stopping(stream) && stream->stop_ts == 0) {
			break;
		}

		if (!get_next_packet(stream, &packet))
			continue;

		if (stopping(stream)) {
			if (can_shutdown_stream(stream, &packet)) {
				obs_encoder_packet_release(&packet);
				break;
			}
		}

		if (!send_packet(stream, &packet, false, packet.track_idx)) {
			os_atomic_set_bool(&stream->disconnected, true);
			break;
		}
	}

	bool encode_error = os_atomic_load_bool(&stream->encode_error);

	if (disconnected(stream)) {
		doLog(LOG_INFO, "Disconnected from %s", stream->path.array);
	} else if (encode_error) {
		doLog(LOG_INFO, "Encoder error, disconnecting");
	} else {
		doLog(LOG_INFO, "User stopped the stream");
	}

	SRTContext *s = &stream->srtContext;
	srt_close(s->fd);
	srt_epoll_release(s->eid);

	if (!stopping(stream)) {
		pthread_detach(stream->send_thread);
		obs_output_signal_stop(stream->output, OBS_OUTPUT_DISCONNECTED);
	} else if (encode_error) {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_ENCODE_ERROR);
	} else {
		obs_output_end_data_capture(stream->output);
	}

	free_packets(stream);
	os_event_reset(stream->stop_event);
	os_atomic_set_bool(&stream->active, false);

	return NULL;
}

static int init_send(struct srt_output *stream)
{
	SRTContext *s = &stream->srtContext;
	obs_output_t *context = stream->output;

	os_sem_destroy(stream->send_sem);
	os_sem_init(&stream->send_sem, 0);

	int ret =
		pthread_create(&stream->send_thread, NULL, send_thread, stream);
	if (ret != 0) {
		srt_close(s->fd);
		srt_epoll_release(s->eid);
		doLog(LOG_ERROR, "Failed to create send thread");
		return OBS_OUTPUT_ERROR;
	}

	os_atomic_set_bool(&stream->active, true);

	obs_output_begin_data_capture(stream->output, 0);

	return OBS_OUTPUT_SUCCESS;
}

static void *connect_thread(void *data)
{
	struct srt_output *stream = (struct srt_output *)data;
	int ret;

	os_set_thread_name("srt-stream: connect_thread");

	if (!libsrt_open(stream)) {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_BAD_PATH);
		return NULL;
	}

	if (libsrt_connect(stream) != OBS_OUTPUT_SUCCESS)
	{
		obs_output_signal_stop(stream->output,
				       OBS_OUTPUT_CONNECT_FAILED);
		return NULL;
	}

	ret = init_send(stream);

	if (ret != OBS_OUTPUT_SUCCESS) {
		obs_output_signal_stop(stream->output, ret);
		doLog(LOG_INFO, "srt Connection to %s failed: %d",
		      stream->path.array, ret);
	}

	if (!stopping(stream))
		pthread_detach(stream->connect_thread);

	os_atomic_set_bool(&stream->connecting, false);
	return NULL;
}

static void srt_output_destroy(void *data)
{
	struct srt_output *stream = (struct srt_output *)data;
	if (stopping(stream) && !connecting(stream)) {
		pthread_join(stream->send_thread, NULL);

	} else if (connecting(stream) || active(stream)) {
		if (stream->connecting)
			pthread_join(stream->connect_thread, NULL);

		stream->stop_ts = 0;
		os_event_signal(stream->stop_event);

		if (active(stream)) {
			os_sem_post(stream->send_sem);
			obs_output_end_data_capture(stream->output);
			pthread_join(stream->send_thread, NULL);
		}
	}

	srt_close(stream->srtContext.fd);
	srt_epoll_release(stream->srtContext.eid);
	srt_cleanup();

	bfree(stream->srtContext.streamid);
	free_packets(stream);
	dstr_free(&stream->path);
	os_event_destroy(stream->stop_event);
	os_sem_destroy(stream->send_sem);
	pthread_mutex_destroy(&stream->mutex);
	circlebuf_free(&stream->packets);

	mpeg_ts_destroy(stream->ts);

	bfree(stream);
}

static void *srt_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct srt_output *stream =
		(struct srt_output *)bzalloc(sizeof(struct srt_output));

	struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;

	stream->ts = mpeg_ts_create(&tshandler, stream);
	stream->output = output;
	stream->max_shutdown_time_sec = 30;
	pthread_mutex_init_value(&stream->mutex);

	if (pthread_mutex_init(&stream->mutex, NULL) != 0)
		goto fail;
	if (os_event_init(&stream->stop_event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;

	UNUSED_PARAMETER(settings);
	SRTContext *s = &stream->srtContext;
	s->maxbw = 1024 * 1024 * 2;
	s->payload_size = 1316;
	s->sndbuf = 1024 * 1024 * 10;
	s->mode = SRT_MODE_CALLER;
	s->transtype = SRTT_LIVE;

	int ret = srt_startup();
	if (ret < 0) {
		libsrt_neterrno();
		goto fail;
	}

	srt_setloglevel(LOG_DEBUG);
	srt_setlogflags(0 | SRT_LOGF_DISABLE_TIME | SRT_LOGF_DISABLE_SEVERITY |
			SRT_LOGF_DISABLE_THREADNAME | SRT_LOGF_DISABLE_EOL);
	srt_setloghandler("", logHandler);

	return stream;

fail:
	srt_output_destroy(stream);
	return NULL;
}

static bool srt_output_start(void *data)
{
	struct srt_output *stream = (struct srt_output *)data;

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	os_atomic_set_bool(&stream->connecting, true);
	return pthread_create(&stream->connect_thread, NULL, connect_thread,
			      stream) == 0;
}

static void srt_output_stop(void *data, uint64_t ts)
{
	struct srt_output *stream = (struct srt_output *)data;

	if (stopping(stream) && ts != 0)
		return;

	if (connecting(stream))
		pthread_join(stream->connect_thread, NULL);

	stream->stop_ts = ts / 1000ULL;

	if (ts)
		stream->shutdown_timeout_ts =
			ts +
			(uint64_t)stream->max_shutdown_time_sec * 1000000000ULL;

	if (active(stream)) {
		os_event_signal(stream->stop_event);
		if (stream->stop_ts == 0)
			os_sem_post(stream->send_sem);
	} else {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_SUCCESS);
	}
}

static inline bool add_packet(struct srt_output *stream,
			      struct encoder_packet *packet)
{
	circlebuf_push_back(&stream->packets, packet,
			    sizeof(struct encoder_packet));
	return true;
}

static void srt_output_data(void *data, struct encoder_packet *packet)
{
	struct srt_output *stream = (struct srt_output *)data;
	struct encoder_packet new_packet;
	bool added_packet = false;

	if (disconnected(stream) || !active(stream))
		return;

	/* encoder fail */
	if (!packet) {
		os_atomic_set_bool(&stream->encode_error, true);
		os_sem_post(stream->send_sem);
		return;
	}

	if (packet->type == OBS_ENCODER_VIDEO) {
		if (!stream->got_first_video) {
			stream->start_dts_offset =
				get_ms_time(packet, packet->dts);
			stream->got_first_video = true;
		}
	}

	obs_encoder_packet_ref(&new_packet, packet);

	pthread_mutex_lock(&stream->mutex);

	if (!disconnected(stream)) {
		added_packet = add_packet(stream, &new_packet);
	}

	pthread_mutex_unlock(&stream->mutex);

	if (added_packet)
		os_sem_post(stream->send_sem);
	else
		obs_encoder_packet_release(&new_packet);
}

static uint64_t srt_total_bytes_sent(void *data)
{
	struct srt_output *stream = (struct srt_output *)data;
	SRTContext *s = &stream->srtContext;

	SRT_TRACEBSTATS info;
	if (srt_bistats(s->fd, &info, 0, 1) == SRT_SUCCESS)
		return info.byteSentTotal;
	
	return 0;
}

static int srt_dropped_frames(void *data)
{
	struct srt_output *stream = (struct srt_output *)data;
	SRTContext *s = &stream->srtContext;

	SRT_TRACEBSTATS info;
	if (srt_bistats(s->fd, &info, 0, 1) == SRT_SUCCESS)
		return info.pktSndDropTotal;

	return 0;
}

struct obs_output_info srt_output_info = {
	"srt_output",
	OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK,
	srt_output_getname,
	srt_output_create,
	srt_output_destroy,
	srt_output_start,
	srt_output_stop,
	nullptr,
	nullptr,
	srt_output_data,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	srt_total_bytes_sent,
	srt_dropped_frames,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"h264",
	"aac",
	nullptr,
	nullptr,
};
