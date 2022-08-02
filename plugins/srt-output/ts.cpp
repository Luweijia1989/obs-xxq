#include "ts.h"
#include "autofree.h"
#include <algorithm>

#define ERROR_SUCCESS 0

// free the p and set to NULL.
// p must be a T*.
#define srs_freep(p)      \
	if (p) {          \
		delete p; \
		p = NULL; \
	}                 \
	(void)0

// please use the freepa(T[]) to free an array,
// or the behavior is undefined.
#define srs_freepa(pa) \
    if (pa) { \
        delete[] pa; \
        pa = NULL; \
    } \
    (void)0


// the mpegts header specifed the video/audio pid.
#define TS_PMT_NUMBER 1
#define TS_PMT_PID 0x1001
#define TS_VIDEO_AVC_PID 0x100
#define TS_AUDIO_AAC_PID 0x101
#define TS_AUDIO_MP3_PID 0x102

static const uint32_t crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

// @see http://www.stmc.edu.hk/~vincent/ffmpeg_0.4.9-pre1/libavformat/mpegtsenc.c
unsigned int mpegts_crc32(const uint8_t *data, int len)
{
    register int i;
    unsigned int crc = 0xffffffff;
    
    for (i=0; i<len; i++)
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];
    
    return crc;
}

uint32_t srs_crc32(const void* buf, int size)
{
    return mpegts_crc32((const uint8_t*)buf, size);
}

SrsTsChannel::SrsTsChannel()
{
    pid = 0;
    apply = SrsTsPidApplyReserved;
    stream = SrsTsStreamReserved;
    msg = NULL;
    continuity_counter = 0;
    context = NULL;
}

SrsTsChannel::~SrsTsChannel()
{
    srs_freep(msg);
}

SrsTsMessage::SrsTsMessage(SrsTsChannel* c, SrsTsPacket* p)
{
    channel = c;
    packet = p;

    dts = pts = 0;
    sid = (SrsTsPESStreamId)0x00;
    continuity_counter = 0;
    PES_packet_length = 0;
    payload = new SrsSimpleBuffer();
    is_discontinuity = false;

    start_pts = 0;
    write_pcr = false;
}

SrsTsMessage::~SrsTsMessage()
{
    srs_freep(payload);
}

int SrsTsMessage::dump(SrsStream* stream, int* pnb_bytes)
{
    int ret = ERROR_SUCCESS;

    if (stream->empty()) {
        return ret;
    }

    // xB
    int nb_bytes = stream->size() - stream->pos();
    if (PES_packet_length > 0) {
        nb_bytes = std::min(nb_bytes, PES_packet_length - payload->length());
    }

    if (nb_bytes > 0) {
        if (!stream->require(nb_bytes)) {
            return -1;
        }

        payload->append(stream->data() + stream->pos(), nb_bytes);
        stream->skip(nb_bytes);
    }

    *pnb_bytes = nb_bytes;

    return ret;
}

bool SrsTsMessage::completed(int8_t payload_unit_start_indicator)
{
    if (PES_packet_length == 0) {
        return payload_unit_start_indicator;
    }
    return payload->length() >= PES_packet_length;
}

bool SrsTsMessage::fresh()
{
    return payload->length() == 0;
}

bool SrsTsMessage::is_audio()
{
    return ((sid >> 5) & 0x07) == SrsTsPESStreamIdAudioChecker;
}

bool SrsTsMessage::is_video()
{
    return ((sid >> 4) & 0x0f) == SrsTsPESStreamIdVideoChecker;
}

int SrsTsMessage::stream_number()
{
    if (is_audio()) {
        return sid & 0x1f;
    } else if (is_video()) {
        return sid & 0x0f;
    }
    return -1;
}

SrsTsMessage* SrsTsMessage::detach()
{
    // @remark the packet cannot be used, but channel is ok.
    SrsTsMessage* cp = new SrsTsMessage(channel, NULL);
    cp->start_pts = start_pts;
    cp->write_pcr = write_pcr;
    cp->is_discontinuity = is_discontinuity;
    cp->dts = dts;
    cp->pts = pts;
    cp->sid = sid;
    cp->PES_packet_length = PES_packet_length;
    cp->continuity_counter = continuity_counter;
    cp->payload = payload;
    payload = NULL;
    return cp;
}

ISrsTsHandler::ISrsTsHandler() {}

ISrsTsHandler::~ISrsTsHandler() {}

SrsTsContext::SrsTsContext()
{
	pure_audio = false;
	vcodec = SrsCodecVideoReserved;
	acodec = SrsCodecAudioReserved1;

	//tsFile = fopen("E:\\hhhh.ts", "wb");
}

SrsTsContext::~SrsTsContext()
{
	std::map<int, SrsTsChannel *>::iterator it;
	for (it = pids.begin(); it != pids.end(); ++it) {
		SrsTsChannel *channel = it->second;
		srs_freep(channel);
	}
	pids.clear();

	//fclose(tsFile);
}

void SrsTsContext::setWriteCallback(tscb cb, void *data)
{
	_cb = cb;
	_cb_data = data;
}

bool SrsTsContext::is_pure_audio()
{
	return pure_audio;
}

void SrsTsContext::on_pmt_parsed()
{
	pure_audio = true;

	std::map<int, SrsTsChannel *>::iterator it;
	for (it = pids.begin(); it != pids.end(); ++it) {
		SrsTsChannel *channel = it->second;
		if (channel->apply == SrsTsPidApplyVideo) {
			pure_audio = false;
		}
	}
}

void SrsTsContext::reset()
{
	vcodec = SrsCodecVideoReserved;
	acodec = SrsCodecAudioReserved1;
}

SrsTsChannel *SrsTsContext::get(int pid)
{
	if (pids.find(pid) == pids.end()) {
		return NULL;
	}
	return pids[pid];
}

void SrsTsContext::set(int pid, SrsTsPidApply apply_pid, SrsTsStream stream)
{
	SrsTsChannel *channel = NULL;

	if (pids.find(pid) == pids.end()) {
		channel = new SrsTsChannel();
		channel->context = this;
		pids[pid] = channel;
	} else {
		channel = pids[pid];
	}

	channel->pid = pid;
	channel->apply = apply_pid;
	channel->stream = stream;
}

int SrsTsContext::encode(SrsTsMessage *msg,
			 SrsCodecVideo vc, SrsCodecAudio ac)
{
	int ret = ERROR_SUCCESS;

	SrsTsStream vs, as;
	int16_t video_pid = 0, audio_pid = 0;
	switch (vc) {
	case SrsCodecVideoAVC:
		vs = SrsTsStreamVideoH264;
		video_pid = TS_VIDEO_AVC_PID;
		break;
	case SrsCodecVideoDisabled:
		vs = SrsTsStreamReserved;
		break;
	case SrsCodecVideoReserved:
	case SrsCodecVideoReserved1:
	case SrsCodecVideoReserved2:
	case SrsCodecVideoSorensonH263:
	case SrsCodecVideoScreenVideo:
	case SrsCodecVideoOn2VP6:
	case SrsCodecVideoOn2VP6WithAlphaChannel:
	case SrsCodecVideoScreenVideoVersion2:
		vs = SrsTsStreamReserved;
		break;
	}
	switch (ac) {
	case SrsCodecAudioAAC:
		as = SrsTsStreamAudioAAC;
		audio_pid = TS_AUDIO_AAC_PID;
		break;
	case SrsCodecAudioMP3:
		as = SrsTsStreamAudioMp3;
		audio_pid = TS_AUDIO_MP3_PID;
		break;
	case SrsCodecAudioDisabled:
		as = SrsTsStreamReserved;
		break;
	case SrsCodecAudioReserved1:
	case SrsCodecAudioLinearPCMPlatformEndian:
	case SrsCodecAudioADPCM:
	case SrsCodecAudioLinearPCMLittleEndian:
	case SrsCodecAudioNellymoser16kHzMono:
	case SrsCodecAudioNellymoser8kHzMono:
	case SrsCodecAudioNellymoser:
	case SrsCodecAudioReservedG711AlawLogarithmicPCM:
	case SrsCodecAudioReservedG711MuLawLogarithmicPCM:
	case SrsCodecAudioReserved:
	case SrsCodecAudioSpeex:
	case SrsCodecAudioReservedMP3_8kHz:
	case SrsCodecAudioReservedDeviceSpecificSound:
		as = SrsTsStreamReserved;
		break;
	}

	if (as == SrsTsStreamReserved && vs == SrsTsStreamReserved) {
		return -1;
	}

	// when any codec changed, write PAT/PMT table.
	if (vcodec != vc || acodec != ac) {
		vcodec = vc;
		acodec = ac;
		if ((ret = encode_pat_pmt(video_pid, vs, audio_pid,
					  as)) != ERROR_SUCCESS) {
			return ret;
		}
	}

	// encode the media frame to PES packets over TS.
	if (msg->is_audio()) {
		return encode_pes(msg, audio_pid, as,
				  vs == SrsTsStreamReserved);
	} else {
		return encode_pes(msg, video_pid, vs,
				  vs == SrsTsStreamReserved);
	}
}

int SrsTsContext::encode_pat_pmt(int16_t vpid, SrsTsStream vs, int16_t apid, SrsTsStream as)
{
	int ret = ERROR_SUCCESS;

	if (vs != SrsTsStreamVideoH264 && as != SrsTsStreamAudioAAC &&
	    as != SrsTsStreamAudioMp3) {
		return -1;
	}

	int16_t pmt_number = TS_PMT_NUMBER;
	int16_t pmt_pid = TS_PMT_PID;
	if (true) {
		SrsTsPacket *pkt =
			SrsTsPacket::create_pat(this, pmt_number, pmt_pid);
		SrsAutoFree(SrsTsPacket, pkt);

		char *buf = new char[SRS_TS_PACKET_SIZE];
		SrsAutoFreeA(char, buf);

		// set the left bytes with 0xFF.
		int nb_buf = pkt->size();
		memset(buf + nb_buf, 0xFF, SRS_TS_PACKET_SIZE - nb_buf);

		SrsStream stream;
		if ((ret = stream.initialize(buf, nb_buf)) != ERROR_SUCCESS) {
			return ret;
		}
		if ((ret = pkt->encode(&stream)) != ERROR_SUCCESS) {
			return ret;
		}

		if (_cb && _cb_data)
			_cb(_cb_data, (uint8_t *)buf, SRS_TS_PACKET_SIZE);

		//fwrite(buf, 1, 188, tsFile);
		//to do write
		/*if ((ret = writer->write(buf, SRS_TS_PACKET_SIZE, NULL)) !=
		    ERROR_SUCCESS) {
			srs_error("ts write ts packet failed. ret=%d", ret);
			return ret;
		}*/
	}
	if (true) {
		SrsTsPacket *pkt = SrsTsPacket::create_pmt(
			this, pmt_number, pmt_pid, vpid, vs, apid, as);
		SrsAutoFree(SrsTsPacket, pkt);

		char *buf = new char[SRS_TS_PACKET_SIZE];
		SrsAutoFreeA(char, buf);

		// set the left bytes with 0xFF.
		int nb_buf = pkt->size();
		memset(buf + nb_buf, 0xFF, SRS_TS_PACKET_SIZE - nb_buf);

		SrsStream stream;
		if ((ret = stream.initialize(buf, nb_buf)) != ERROR_SUCCESS) {
			return ret;
		}
		if ((ret = pkt->encode(&stream)) != ERROR_SUCCESS) {
			return ret;
		}

		if (_cb && _cb_data)
			_cb(_cb_data, (uint8_t *)buf, SRS_TS_PACKET_SIZE);

		//fwrite(buf, 1, 188, tsFile);
		//to do write
		/*if ((ret = writer->write(buf, SRS_TS_PACKET_SIZE, NULL)) !=
		    ERROR_SUCCESS) {
			srs_error("ts write ts packet failed. ret=%d", ret);
			return ret;
		}*/
	}

	return ret;
}

int SrsTsContext::encode_pes(SrsTsMessage *msg,
			     int16_t pid, SrsTsStream sid, bool pure_audio)
{
	int ret = ERROR_SUCCESS;

	if (msg->payload->length() == 0) {
		return ret;
	}

	if (sid != SrsTsStreamVideoH264 && sid != SrsTsStreamAudioMp3 &&
	    sid != SrsTsStreamAudioAAC) {
		return ret;
	}

	SrsTsChannel *channel = get(pid);

	char *start = msg->payload->bytes();
	char *end = start + msg->payload->length();
	char *p = start;

	while (p < end) {
		SrsTsPacket *pkt = NULL;
		if (p == start) {
			// write pcr according to message.
			bool write_pcr = msg->write_pcr;

			// for pure audio, always write pcr.
			// TODO: FIXME: maybe only need to write at begin and end of ts.
			if (pure_audio && msg->is_audio()) {
				write_pcr = true;
			}

			// it's ok to set pcr equals to dts,
			// @see https://github.com/ossrs/srs/issues/311
			// Fig. 3.18. Program Clock Reference of Digital-Video-and-Audio-Broadcasting-Technology, page 65
			// In MPEG-2, these are the "Program Clock Refer- ence" (PCR) values which are
			// nothing else than an up-to-date copy of the STC counter fed into the transport
			// stream at a certain time. The data stream thus carries an accurate internal
			// "clock time". All coding and de- coding processes are controlled by this clock
			// time. To do this, the receiver, i.e. the MPEG decoder, must read out the
			// "clock time", namely the PCR values, and compare them with its own internal
			// system clock, that is to say its own 42 bit counter.
			int64_t pcr = write_pcr ? msg->dts : -1;

			// TODO: FIXME: finger it why use discontinuity of msg.
			pkt = SrsTsPacket::create_pes_first(
				this, pid, msg->sid,
				channel->continuity_counter++,
				msg->is_discontinuity, pcr, msg->dts, msg->pts,
				msg->payload->length());
		} else {
			pkt = SrsTsPacket::create_pes_continue(
				this, pid, msg->sid,
				channel->continuity_counter++);
		}
		SrsAutoFree(SrsTsPacket, pkt);

		char *buf = new char[SRS_TS_PACKET_SIZE];
		SrsAutoFreeA(char, buf);

		// set the left bytes with 0xFF.
		int nb_buf = pkt->size();

		int left = (int)std::min((int)(end - p), SRS_TS_PACKET_SIZE - nb_buf);
		int nb_stuffings = SRS_TS_PACKET_SIZE - nb_buf - left;
		if (nb_stuffings > 0) {
			// set all bytes to stuffings.
			memset(buf, 0xFF, SRS_TS_PACKET_SIZE);

			// padding with stuffings.
			pkt->padding(nb_stuffings);

			// size changed, recalc it.
			nb_buf = pkt->size();

			left = (int)std::min((int)(end - p),
					    SRS_TS_PACKET_SIZE - nb_buf);
			nb_stuffings = SRS_TS_PACKET_SIZE - nb_buf - left;
		}
		memcpy(buf + nb_buf, p, left);
		p += left;

		SrsStream stream;
		if ((ret = stream.initialize(buf, nb_buf)) != ERROR_SUCCESS) {
			return ret;
		}
		if ((ret = pkt->encode(&stream)) != ERROR_SUCCESS) {
			return ret;
		}

		if (_cb && _cb_data)
			_cb(_cb_data, (uint8_t *)buf, SRS_TS_PACKET_SIZE);

		//fwrite(buf, 1, 188, tsFile);
		//to do write
		/*if ((ret = writer->write(buf, SRS_TS_PACKET_SIZE, NULL)) !=
		    ERROR_SUCCESS) {
			srs_error("ts write ts packet failed. ret=%d", ret);
			return ret;
		}*/
	}

	return ret;
}

SrsTsPacket::SrsTsPacket(SrsTsContext *c)
{
	context = c;

	sync_byte = 0;
	transport_error_indicator = 0;
	payload_unit_start_indicator = 0;
	transport_priority = 0;
	pid = SrsTsPidPAT;
	transport_scrambling_control = SrsTsScrambledDisabled;
	adaption_field_control = SrsTsAdaptationFieldTypeReserved;
	continuity_counter = 0;
	adaptation_field = NULL;
	payload = NULL;
}

SrsTsPacket::~SrsTsPacket()
{
	srs_freep(adaptation_field);
	srs_freep(payload);
}

int SrsTsPacket::size()
{
	int sz = 4;

	sz += adaptation_field ? adaptation_field->size() : 0;
	sz += payload ? payload->size() : 0;

	return sz;
}

int SrsTsPacket::encode(SrsStream *stream)
{
	int ret = ERROR_SUCCESS;

	// 4B ts packet header.
	if (!stream->require(4)) {
		ret = -1;
		return ret;
	}

	stream->write_1bytes(sync_byte);

	int16_t pidv = pid & 0x1FFF;
	pidv |= (transport_priority << 13) & 0x2000;
	pidv |= (transport_error_indicator << 15) & 0x8000;
	pidv |= (payload_unit_start_indicator << 14) & 0x4000;
	stream->write_2bytes(pidv);

	int8_t ccv = continuity_counter & 0x0F;
	ccv |= (transport_scrambling_control << 6) & 0xC0;
	ccv |= (adaption_field_control << 4) & 0x30;
	stream->write_1bytes(ccv);

	// optional: adaptation field
	if (adaptation_field) {
		if ((ret = adaptation_field->encode(stream)) != ERROR_SUCCESS) {
			return ret;
		}
	}

	// optional: payload.
	if (payload) {
		if ((ret = payload->encode(stream)) != ERROR_SUCCESS) {
			return ret;
		}
	}

	return ret;
}

void SrsTsPacket::padding(int nb_stuffings)
{
	if (!adaptation_field) {
		SrsTsAdaptationField *af = new SrsTsAdaptationField(this);
		adaptation_field = af;

		af->adaption_field_length = 0; // calc in size.
		af->discontinuity_indicator = 0;
		af->random_access_indicator = 0;
		af->elementary_stream_priority_indicator = 0;
		af->PCR_flag = 0;
		af->OPCR_flag = 0;
		af->splicing_point_flag = 0;
		af->transport_private_data_flag = 0;
		af->adaptation_field_extension_flag = 0;

		// consume the af size if possible.
		nb_stuffings = std::max(0, nb_stuffings - af->size());
	}

	adaptation_field->nb_af_reserved = nb_stuffings;

	// set payload with af.
	if (adaption_field_control == SrsTsAdaptationFieldTypePayloadOnly) {
		adaption_field_control = SrsTsAdaptationFieldTypeBoth;
	}
}

SrsTsPacket *SrsTsPacket::create_pat(SrsTsContext *context, int16_t pmt_number,
				     int16_t pmt_pid)
{
	SrsTsPacket *pkt = new SrsTsPacket(context);
	pkt->sync_byte = 0x47;
	pkt->transport_error_indicator = 0;
	pkt->payload_unit_start_indicator = 1;
	pkt->transport_priority = 0;
	pkt->pid = SrsTsPidPAT;
	pkt->transport_scrambling_control = SrsTsScrambledDisabled;
	pkt->adaption_field_control = SrsTsAdaptationFieldTypePayloadOnly;
	pkt->continuity_counter = 0;
	pkt->adaptation_field = NULL;
	SrsTsPayloadPAT *pat = new SrsTsPayloadPAT(pkt);
	pkt->payload = pat;

	pat->pointer_field = 0;
	pat->table_id = SrsTsPsiIdPas;
	pat->section_syntax_indicator = 1;
	pat->section_length = 0; // calc in size.
	pat->transport_stream_id = 1;
	pat->version_number = 0;
	pat->current_next_indicator = 1;
	pat->section_number = 0;
	pat->last_section_number = 0;
	pat->programs.push_back(
		new SrsTsPayloadPATProgram(pmt_number, pmt_pid));
	pat->CRC_32 = 0; // calc in encode.
	return pkt;
}

SrsTsPacket *SrsTsPacket::create_pmt(SrsTsContext *context, int16_t pmt_number,
				     int16_t pmt_pid, int16_t vpid,
				     SrsTsStream vs, int16_t apid,
				     SrsTsStream as)
{
	SrsTsPacket *pkt = new SrsTsPacket(context);
	pkt->sync_byte = 0x47;
	pkt->transport_error_indicator = 0;
	pkt->payload_unit_start_indicator = 1;
	pkt->transport_priority = 0;
	pkt->pid = (SrsTsPid)pmt_pid;
	pkt->transport_scrambling_control = SrsTsScrambledDisabled;
	pkt->adaption_field_control = SrsTsAdaptationFieldTypePayloadOnly;
	// TODO: FIXME: maybe should continuous in channel.
	pkt->continuity_counter = 0;
	pkt->adaptation_field = NULL;
	SrsTsPayloadPMT *pmt = new SrsTsPayloadPMT(pkt);
	pkt->payload = pmt;

	pmt->pointer_field = 0;
	pmt->table_id = SrsTsPsiIdPms;
	pmt->section_syntax_indicator = 1;
	pmt->section_length = 0; // calc in size.
	pmt->program_number = pmt_number;
	pmt->version_number = 0;
	pmt->current_next_indicator = 1;
	pmt->section_number = 0;
	pmt->last_section_number = 0;
	pmt->program_info_length = 0;

	// if mp3 or aac specified, use audio to carry pcr.
	if (as == SrsTsStreamAudioAAC || as == SrsTsStreamAudioMp3) {
		// use audio to carray pcr by default.
		// for hls, there must be atleast one audio channel.
		pmt->PCR_PID = apid;
		pmt->infos.push_back(new SrsTsPayloadPMTESInfo(as, apid));
	}

	// if h.264 specified, use video to carry pcr.
	if (vs == SrsTsStreamVideoH264) {
		pmt->PCR_PID = vpid;
		pmt->infos.push_back(new SrsTsPayloadPMTESInfo(vs, vpid));
	}

	pmt->CRC_32 = 0; // calc in encode.
	return pkt;
}

SrsTsPacket *SrsTsPacket::create_pes_first(SrsTsContext *context, int16_t pid,
					   SrsTsPESStreamId sid,
					   uint8_t continuity_counter,
					   bool discontinuity, int64_t pcr,
					   int64_t dts, int64_t pts, int size)
{
	SrsTsPacket *pkt = new SrsTsPacket(context);
	pkt->sync_byte = 0x47;
	pkt->transport_error_indicator = 0;
	pkt->payload_unit_start_indicator = 1;
	pkt->transport_priority = 0;
	pkt->pid = (SrsTsPid)pid;
	pkt->transport_scrambling_control = SrsTsScrambledDisabled;
	pkt->adaption_field_control = SrsTsAdaptationFieldTypePayloadOnly;
	pkt->continuity_counter = continuity_counter;
	pkt->adaptation_field = NULL;
	SrsTsPayloadPES *pes = new SrsTsPayloadPES(pkt);
	pkt->payload = pes;

	if (pcr >= 0) {
		SrsTsAdaptationField *af = new SrsTsAdaptationField(pkt);
		pkt->adaptation_field = af;
		pkt->adaption_field_control = SrsTsAdaptationFieldTypeBoth;

		af->adaption_field_length = 0; // calc in size.
		af->discontinuity_indicator = discontinuity;
		af->random_access_indicator = 0;
		af->elementary_stream_priority_indicator = 0;
		af->PCR_flag = 1;
		af->OPCR_flag = 0;
		af->splicing_point_flag = 0;
		af->transport_private_data_flag = 0;
		af->adaptation_field_extension_flag = 0;
		af->program_clock_reference_base = pcr;
		af->program_clock_reference_extension = 0;
	}

	pes->packet_start_code_prefix = 0x01;
	pes->stream_id = (uint8_t)sid;
	pes->PES_packet_length = (size > 0xFFFF) ? 0 : size;
	pes->PES_scrambling_control = 0;
	pes->PES_priority = 0;
	pes->data_alignment_indicator = 0;
	pes->copyright = 0;
	pes->original_or_copy = 0;
	pes->PTS_DTS_flags = (dts == pts) ? 0x02 : 0x03;
	pes->ESCR_flag = 0;
	pes->ES_rate_flag = 0;
	pes->DSM_trick_mode_flag = 0;
	pes->additional_copy_info_flag = 0;
	pes->PES_CRC_flag = 0;
	pes->PES_extension_flag = 0;
	pes->PES_header_data_length = 0; // calc in size.
	pes->pts = pts;
	pes->dts = dts;
	return pkt;
}

SrsTsPacket *SrsTsPacket::create_pes_continue(SrsTsContext *context,
					      int16_t pid, SrsTsPESStreamId sid,
					      uint8_t continuity_counter)
{
	SrsTsPacket *pkt = new SrsTsPacket(context);
	pkt->sync_byte = 0x47;
	pkt->transport_error_indicator = 0;
	pkt->payload_unit_start_indicator = 0;
	pkt->transport_priority = 0;
	pkt->pid = (SrsTsPid)pid;
	pkt->transport_scrambling_control = SrsTsScrambledDisabled;
	pkt->adaption_field_control = SrsTsAdaptationFieldTypePayloadOnly;
	pkt->continuity_counter = continuity_counter;
	pkt->adaptation_field = NULL;
	pkt->payload = NULL;

	return pkt;
}

SrsTsPayload::SrsTsPayload(SrsTsPacket* p)
{
    packet = p;
}

SrsTsPayload::~SrsTsPayload()
{
}

SrsTsPayloadPES::SrsTsPayloadPES(SrsTsPacket* p) : SrsTsPayload(p)
{
    PES_private_data = NULL;
    pack_field = NULL;
    PES_extension_field = NULL;
    nb_stuffings = 0;
    nb_bytes = 0;
    nb_paddings = 0;
    const2bits = 0x02;
    const1_value0 = 0x07;
}

SrsTsPayloadPSI::SrsTsPayloadPSI(SrsTsPacket* p) : SrsTsPayload(p)
{
    pointer_field = 0;
    const0_value = 0;
    const1_value = 3;
    CRC_32 = 0;
}

SrsTsPayloadPES::~SrsTsPayloadPES()
{
    srs_freepa(PES_private_data);
    srs_freepa(pack_field);
    srs_freepa(PES_extension_field);
}

int SrsTsPayloadPES::size()
{
    int sz = 0;
    
    PES_header_data_length = 0;
    SrsTsPESStreamId sid = (SrsTsPESStreamId)stream_id;

    if (sid != SrsTsPESStreamIdProgramStreamMap
        && sid != SrsTsPESStreamIdPaddingStream
        && sid != SrsTsPESStreamIdPrivateStream2
        && sid != SrsTsPESStreamIdEcmStream
        && sid != SrsTsPESStreamIdEmmStream
        && sid != SrsTsPESStreamIdProgramStreamDirectory
        && sid != SrsTsPESStreamIdDsmccStream
        && sid != SrsTsPESStreamIdH2221TypeE
    ) {
        sz += 6;
        sz += 3;
        PES_header_data_length = sz;

        sz += (PTS_DTS_flags == 0x2)? 5:0;
        sz += (PTS_DTS_flags == 0x3)? 10:0;
        sz += ESCR_flag? 6:0;
        sz += ES_rate_flag? 3:0;
        sz += DSM_trick_mode_flag? 1:0;
        sz += additional_copy_info_flag? 1:0;
        sz += PES_CRC_flag? 2:0;
        sz += PES_extension_flag? 1:0;

        if (PES_extension_flag) {
            sz += PES_private_data_flag? 16:0;
            sz += pack_header_field_flag? 1 + pack_field_length:0; // 1+x bytes.
            sz += program_packet_sequence_counter_flag? 2:0;
            sz += P_STD_buffer_flag? 2:0;
            sz += PES_extension_flag_2? 1 + PES_extension_field_length:0; // 1+x bytes.
        }
        PES_header_data_length = sz - PES_header_data_length;

        sz += nb_stuffings;

        // packet bytes
    } else if (sid == SrsTsPESStreamIdProgramStreamMap
        || sid == SrsTsPESStreamIdPrivateStream2
        || sid == SrsTsPESStreamIdEcmStream
        || sid == SrsTsPESStreamIdEmmStream
        || sid == SrsTsPESStreamIdProgramStreamDirectory
        || sid == SrsTsPESStreamIdDsmccStream
        || sid == SrsTsPESStreamIdH2221TypeE
    ) {
        // packet bytes
    } else {
        // nb_drop
    }

    return sz;
}

int SrsTsPayloadPES::encode(SrsStream* stream)
{
    int ret = ERROR_SUCCESS;

    // 6B fixed header.
    if (!stream->require(6)) {
        return -1;
    }

    // 3B
    stream->write_3bytes(packet_start_code_prefix);
    // 1B
    stream->write_1bytes(stream_id);
    // 2B
    // the PES_packet_length is the actual bytes size, the pplv write to ts
    // is the actual bytes plus the header size.
    int32_t pplv = 0;
    if (PES_packet_length > 0) {
        pplv = PES_packet_length + 3 + PES_header_data_length;
        pplv = (pplv > 0xFFFF)? 0 : pplv;
    }
    stream->write_2bytes(pplv);

    // check the packet start prefix.
    packet_start_code_prefix &= 0xFFFFFF;
    if (packet_start_code_prefix != 0x01) {
        return -2;
    }

    // 3B flags.
    if (!stream->require(3)) {
        return -3;
    }
    // 1B
    int8_t oocv = original_or_copy & 0x01;
    oocv |= (const2bits << 6) & 0xC0;
    oocv |= (PES_scrambling_control << 4) & 0x30;
    oocv |= (PES_priority << 3) & 0x08;
    oocv |= (data_alignment_indicator << 2) & 0x04;
    oocv |= (copyright << 1) & 0x02;
    stream->write_1bytes(oocv);
    // 1B
    int8_t pefv = PES_extension_flag & 0x01;
    pefv |= (PTS_DTS_flags << 6) & 0xC0;
    pefv |= (ESCR_flag << 5) & 0x20;
    pefv |= (ES_rate_flag << 4) & 0x10;
    pefv |= (DSM_trick_mode_flag << 3) & 0x08;
    pefv |= (additional_copy_info_flag << 2) & 0x04;
    pefv |= (PES_CRC_flag << 1) & 0x02;
    stream->write_1bytes(pefv);
    // 1B
    stream->write_1bytes(PES_header_data_length);

    // check required together.
    int nb_required = 0;
    nb_required += (PTS_DTS_flags == 0x2)? 5:0;
    nb_required += (PTS_DTS_flags == 0x3)? 10:0;
    nb_required += ESCR_flag? 6:0;
    nb_required += ES_rate_flag? 3:0;
    nb_required += DSM_trick_mode_flag? 1:0;
    nb_required += additional_copy_info_flag? 1:0;
    nb_required += PES_CRC_flag? 2:0;
    nb_required += PES_extension_flag? 1:0;
    if (!stream->require(nb_required)) {
        return -4;
    }

    // 5B
    if (PTS_DTS_flags == 0x2) {
        if ((ret = encode_33bits_dts_pts(stream, 0x02, pts)) != ERROR_SUCCESS) {
            return ret;
        }
    }

    // 10B
    if (PTS_DTS_flags == 0x3) {
        if ((ret = encode_33bits_dts_pts(stream, 0x03, pts)) != ERROR_SUCCESS) {
            return ret;
        }
        if ((ret = encode_33bits_dts_pts(stream, 0x01, dts)) != ERROR_SUCCESS) {
            return ret;
        }

        // check sync, the diff of dts and pts should never greater than 1s.
        if (dts - pts > 90000 || pts - dts > 90000) {
            
        }
    }

    // 6B
    if (ESCR_flag) {
        stream->skip(6);
    }

    // 3B
    if (ES_rate_flag) {
        stream->skip(3);
    }

    // 1B
    if (DSM_trick_mode_flag) {
        stream->skip(1);
    }

    // 1B
    if (additional_copy_info_flag) {
        stream->skip(1);
    }

    // 2B
    if (PES_CRC_flag) {
        stream->skip(2);
    }

    // 1B
    if (PES_extension_flag) {
        int8_t efv = PES_extension_flag_2 & 0x01;
        efv |= (PES_private_data_flag << 7) & 0x80;
        efv |= (pack_header_field_flag << 6) & 0x40;
        efv |= (program_packet_sequence_counter_flag << 5) & 0x20;
        efv |= (P_STD_buffer_flag << 4) & 0x10;
        efv |= (const1_value0 << 1) & 0xE0;
        stream->write_1bytes(efv);

        nb_required = 0;
        nb_required += PES_private_data_flag? 16:0;
        nb_required += pack_header_field_flag? 1+pack_field_length:0; // 1+x bytes.
        nb_required += program_packet_sequence_counter_flag? 2:0;
        nb_required += P_STD_buffer_flag? 2:0;
        nb_required += PES_extension_flag_2? 1+PES_extension_field_length:0; // 1+x bytes.
        if (!stream->require(nb_required)) {
            return -5;
        }
        stream->skip(nb_required);
    }

    // stuffing_byte
    if (nb_stuffings) {
        stream->skip(nb_stuffings);
    }

    return ret;
}

int SrsTsPayloadPES::decode_33bits_dts_pts(SrsStream* stream, int64_t* pv)
{
    int ret = ERROR_SUCCESS;

    if (!stream->require(5)) {
        return -1;
    }

    // decode the 33bits schema.
    // ===========1B
    // 4bits const maybe '0001', '0010' or '0011'.
    // 3bits DTS/PTS [32..30]
    // 1bit const '1'
    int64_t dts_pts_30_32 = stream->read_1bytes();
    if ((dts_pts_30_32 & 0x01) != 0x01) {
        return -2;
    }
    // @remark, we donot check the high 4bits, maybe '0001', '0010' or '0011'.
    //      so we just ensure the high 4bits is not 0x00.
    if (((dts_pts_30_32 >> 4) & 0x0f) == 0x00) {
        return -3;
    }
    dts_pts_30_32 = (dts_pts_30_32 >> 1) & 0x07;

    // ===========2B
    // 15bits DTS/PTS [29..15]
    // 1bit const '1'
    int64_t dts_pts_15_29 = stream->read_2bytes();
    if ((dts_pts_15_29 & 0x01) != 0x01) {
        return -4;
    }
    dts_pts_15_29 = (dts_pts_15_29 >> 1) & 0x7fff;

    // ===========2B
    // 15bits DTS/PTS [14..0]
    // 1bit const '1'
    int64_t dts_pts_0_14 = stream->read_2bytes();
    if ((dts_pts_0_14 & 0x01) != 0x01) {
        return -5;
    }
    dts_pts_0_14 = (dts_pts_0_14 >> 1) & 0x7fff;

    int64_t v = 0x00;
    v |= (dts_pts_30_32 << 30) & 0x1c0000000LL;
    v |= (dts_pts_15_29 << 15) & 0x3fff8000LL;
    v |= dts_pts_0_14 & 0x7fff;
    *pv = v;

    return ret;
}

int SrsTsPayloadPES::encode_33bits_dts_pts(SrsStream* stream, uint8_t fb, int64_t v)
{
    int ret = ERROR_SUCCESS;

    if (!stream->require(5)) {
        return -1;
    }

    char* p = stream->data() + stream->pos();
    stream->skip(5);

    int32_t val = 0;
    
    val = fb << 4 | (((v >> 30) & 0x07) << 1) | 1;
    *p++ = val;
    
    val = (((v >> 15) & 0x7fff) << 1) | 1;
    *p++ = (val >> 8);
    *p++ = val;
    
    val = (((v) & 0x7fff) << 1) | 1;
    *p++ = (val >> 8);
    *p++ = val;

    return ret;
}

SrsTsPayloadPSI::~SrsTsPayloadPSI()
{
}

int SrsTsPayloadPSI::size()
{
    int sz = 0;

    // section size is the sl plus the crc32
    section_length = psi_size() + 4;

     sz += packet->payload_unit_start_indicator? 1:0;
     sz += 3;
     sz += section_length;

    return sz;
}

int SrsTsPayloadPSI::encode(SrsStream* stream)
{
    int ret = ERROR_SUCCESS;

    if (packet->payload_unit_start_indicator) {
        if (!stream->require(1)) {
            return -1;
        }
        stream->write_1bytes(pointer_field);
    }

    // to calc the crc32
    char* ppat = stream->data() + stream->pos();
    int pat_pos = stream->pos();

    // atleast 3B for all psi.
    if (!stream->require(3)) {
        return -1;
    }
    // 1B
    stream->write_1bytes(table_id);
    
    // 2B
    int16_t slv = section_length & 0x0FFF;
    slv |= (section_syntax_indicator << 15) & 0x8000;
    slv |= (const0_value << 14) & 0x4000;
    slv |= (const1_value << 12) & 0x3000;
    stream->write_2bytes(slv);

    // no section, ignore.
    if (section_length == 0) {
        return ret;
    }

    if (!stream->require(section_length)) {
        return -2;
    }

    // call the virtual method of actual PSI.
    if ((ret = psi_encode(stream)) != ERROR_SUCCESS) {
        return ret;
    }
    
    // 4B
    if (!stream->require(4)) {
        return -3;
    }
    CRC_32 = srs_crc32(ppat, stream->pos() - pat_pos);
    stream->write_4bytes(CRC_32);

    return ret;
}

SrsTsPayloadPATProgram::SrsTsPayloadPATProgram(int16_t n, int16_t p)
{
    number = n;
    pid = p;
    const1_value = 0x07;
}

SrsTsPayloadPATProgram::~SrsTsPayloadPATProgram()
{
}

int SrsTsPayloadPATProgram::size()
{
    return 4;
}

int SrsTsPayloadPATProgram::encode(SrsStream* stream)
{
    int ret = ERROR_SUCCESS;

    // atleast 4B for PAT program specified
    if (!stream->require(4)) {
        return -1;
    }

    int tmpv = pid & 0x1FFF;
    tmpv |= (number << 16) & 0xFFFF0000;
    tmpv |= (const1_value << 13) & 0xE000;
    stream->write_4bytes(tmpv);

    return ret;
}

SrsTsPayloadPAT::SrsTsPayloadPAT(SrsTsPacket* p) : SrsTsPayloadPSI(p)
{
    const3_value = 3;
}

SrsTsPayloadPAT::~SrsTsPayloadPAT()
{
    std::vector<SrsTsPayloadPATProgram*>::iterator it;
    for (it = programs.begin(); it != programs.end(); ++it) {
        SrsTsPayloadPATProgram* program = *it;
        srs_freep(program);
    }
    programs.clear();
}

int SrsTsPayloadPAT::psi_size()
{
    int sz = 5;
    for (int i = 0; i < (int)programs.size(); i ++) {
        SrsTsPayloadPATProgram* program = programs.at(i);
        sz += program->size();
    }
    return sz;
}

int SrsTsPayloadPAT::psi_encode(SrsStream* stream)
{
    int ret = ERROR_SUCCESS;

    // atleast 5B for PAT specified
    if (!stream->require(5)) {
        return -1;
    }

    // 2B
    stream->write_2bytes(transport_stream_id);
    
    // 1B
    int8_t cniv = current_next_indicator & 0x01;
    cniv |= (version_number << 1) & 0x3E;
    cniv |= (const1_value << 6) & 0xC0;
    stream->write_1bytes(cniv);
    
    // 1B
    stream->write_1bytes(section_number);
    // 1B
    stream->write_1bytes(last_section_number);

    // multiple 4B program data.
    for (int i = 0; i < (int)programs.size(); i ++) {
        SrsTsPayloadPATProgram* program = programs.at(i);
        if ((ret = program->encode(stream)) != ERROR_SUCCESS) {
            return ret;
        }

        // update the apply pid table.
        packet->context->set(program->pid, SrsTsPidApplyPMT);
    }

    // update the apply pid table.
    packet->context->set(packet->pid, SrsTsPidApplyPAT);

    return ret;
}

SrsTsPayloadPMTESInfo::SrsTsPayloadPMTESInfo(SrsTsStream st, int16_t epid)
{
    stream_type = st;
    elementary_PID = epid;

    const1_value0 = 7;
    const1_value1 = 0x0f;
    ES_info_length = 0;
    ES_info = NULL;
}

SrsTsPayloadPMTESInfo::~SrsTsPayloadPMTESInfo()
{
    srs_freepa(ES_info);
}

int SrsTsPayloadPMTESInfo::size()
{
    return 5 + ES_info_length;
}

int SrsTsPayloadPMTESInfo::encode(SrsStream* stream)
{
    int ret = ERROR_SUCCESS;

    // 5B
    if (!stream->require(5)) {
        return -1;
    }

    stream->write_1bytes(stream_type);

    int16_t epv = elementary_PID & 0x1FFF;
    epv |= (const1_value0 << 13) & 0xE000;
    stream->write_2bytes(epv);
    
    int16_t eilv = ES_info_length & 0x0FFF;
    eilv |= (const1_value1 << 12) & 0xF000;
    stream->write_2bytes(eilv);

    if (ES_info_length > 0) {
        if (!stream->require(ES_info_length)) {
            return -2;
        }
        stream->write_bytes(ES_info, ES_info_length);
    }

    return ret;
}

SrsTsPayloadPMT::SrsTsPayloadPMT(SrsTsPacket* p) : SrsTsPayloadPSI(p)
{
    const1_value0 = 3;
    const1_value1 = 7;
    const1_value2 = 0x0f;
    program_info_length = 0;
    program_info_desc = NULL;
}

SrsTsPayloadPMT::~SrsTsPayloadPMT()
{
    srs_freepa(program_info_desc);

    std::vector<SrsTsPayloadPMTESInfo*>::iterator it;
    for (it = infos.begin(); it != infos.end(); ++it) {
        SrsTsPayloadPMTESInfo* info = *it;
        srs_freep(info);
    }
    infos.clear();
}

int SrsTsPayloadPMT::psi_size()
{
    int sz = 9;
    sz += program_info_length;
    for (int i = 0; i < (int)infos.size(); i ++) {
        SrsTsPayloadPMTESInfo* info = infos.at(i);
        sz += info->size();
    }
    return sz;
}

int SrsTsPayloadPMT::psi_encode(SrsStream* stream)
{
    int ret = ERROR_SUCCESS;

    // atleast 9B for PMT specified
    if (!stream->require(9)) {
        return -1;
    }

    // 2B
    stream->write_2bytes(program_number);
    
    // 1B
    int8_t cniv = current_next_indicator & 0x01;
    cniv |= (const1_value0 << 6) & 0xC0;
    cniv |= (version_number << 1) & 0xFE;
    stream->write_1bytes(cniv);
    
    // 1B
    stream->write_1bytes(section_number);
    
    // 1B
    stream->write_1bytes(last_section_number);

    // 2B
    int16_t ppv = PCR_PID & 0x1FFF;
    ppv |= (const1_value1 << 13) & 0xE000;
    stream->write_2bytes(ppv);
    
    // 2B
    int16_t pilv = program_info_length & 0xFFF;
    pilv |= (const1_value2 << 12) & 0xF000;
    stream->write_2bytes(pilv);

    if (program_info_length > 0) {
        if (!stream->require(program_info_length)) {
            return -2;
        }

        stream->write_bytes(program_info_desc, program_info_length);
    }

    for (int i = 0; i < (int)infos.size(); i ++) {
        SrsTsPayloadPMTESInfo* info = infos.at(i);
        if ((ret = info->encode(stream)) != ERROR_SUCCESS) {
            return ret;
        }

        // update the apply pid table
        switch (info->stream_type) {
            case SrsTsStreamVideoH264:
            case SrsTsStreamVideoMpeg4:
                packet->context->set(info->elementary_PID, SrsTsPidApplyVideo, info->stream_type);
                break;
            case SrsTsStreamAudioAAC:
            case SrsTsStreamAudioAC3:
            case SrsTsStreamAudioDTS:
            case SrsTsStreamAudioMp3:
                packet->context->set(info->elementary_PID, SrsTsPidApplyAudio, info->stream_type);
                break;
            default:
                break;
        }
    }

    // update the apply pid table.
    packet->context->set(packet->pid, SrsTsPidApplyPMT);

    return ret;
}


SrsTsAdaptationField::SrsTsAdaptationField(SrsTsPacket* pkt)
{
    packet = pkt;

    adaption_field_length = 0;
    discontinuity_indicator = 0;
    random_access_indicator = 0;
    elementary_stream_priority_indicator = 0;
    PCR_flag = 0;
    OPCR_flag = 0;
    splicing_point_flag = 0;
    transport_private_data_flag = 0;
    adaptation_field_extension_flag = 0;
    program_clock_reference_base = 0;
    program_clock_reference_extension = 0;
    original_program_clock_reference_base = 0;
    original_program_clock_reference_extension = 0;
    splice_countdown = 0;
    transport_private_data_length = 0;
    transport_private_data = NULL;
    adaptation_field_extension_length = 0;
    ltw_flag = 0;
    piecewise_rate_flag = 0;
    seamless_splice_flag = 0;
    ltw_valid_flag = 0;
    ltw_offset = 0;
    piecewise_rate = 0;
    splice_type = 0;
    DTS_next_AU0 = 0;
    marker_bit0 = 0;
    DTS_next_AU1 = 0;
    marker_bit1 = 0;
    DTS_next_AU2 = 0;
    marker_bit2 = 0;
    nb_af_ext_reserved = 0;
    nb_af_reserved = 0;

    const1_value0 = 0x3F;
    const1_value1 = 0x1F;
    const1_value2 = 0x3F;
}

SrsTsAdaptationField::~SrsTsAdaptationField()
{
    srs_freepa(transport_private_data);
}

int SrsTsAdaptationField::size()
{
    int sz = 2;

    sz += PCR_flag? 6 : 0;
    sz += OPCR_flag? 6 : 0;
    sz += splicing_point_flag? 1 : 0;
    sz += transport_private_data_flag? 1 + transport_private_data_length : 0;
    sz += adaptation_field_extension_flag? 2 + adaptation_field_extension_length : 0;
    sz += nb_af_ext_reserved;
    sz += nb_af_reserved;

    adaption_field_length = sz - 1;

    return sz;
}

int SrsTsAdaptationField::encode(SrsStream* stream)
{
    int ret = ERROR_SUCCESS;

    if (!stream->require(2)) {
        return -1;
    }
    stream->write_1bytes(adaption_field_length);

    // When the adaptation_field_control value is '11', the value of the adaptation_field_length shall
    // be in the range 0 to 182. 
    if (packet->adaption_field_control == SrsTsAdaptationFieldTypeBoth && adaption_field_length > 182) {
        return -2;
    }
    // When the adaptation_field_control value is '10', the value of the adaptation_field_length shall
    // be 183.
    if (packet->adaption_field_control == SrsTsAdaptationFieldTypeAdaptionOnly && adaption_field_length != 183) {
        return -3;
    }
    
    // no adaptation field.
    if (adaption_field_length == 0) {
        return ret;
    }
    int8_t tmpv = adaptation_field_extension_flag & 0x01;
    tmpv |= (discontinuity_indicator << 7) & 0x80;
    tmpv |= (random_access_indicator << 6) & 0x40;
    tmpv |= (elementary_stream_priority_indicator << 5) & 0x20;
    tmpv |= (PCR_flag << 4) & 0x10;
    tmpv |= (OPCR_flag << 3) & 0x08;
    tmpv |= (splicing_point_flag << 2) & 0x04;
    tmpv |= (transport_private_data_flag << 1) & 0x02;
    stream->write_1bytes(tmpv);
    
    if (PCR_flag) {
        if (!stream->require(6)) {
            return -5;
        }

        char* pp = NULL;
        char* p = stream->data() + stream->pos();
        stream->skip(6);
        
        // @remark, use pcr base and ignore the extension
        // @see https://github.com/ossrs/srs/issues/250#issuecomment-71349370
        int64_t pcrv = program_clock_reference_extension & 0x1ff;
        pcrv |= (const1_value0 << 9) & 0x7E00;
        pcrv |= (program_clock_reference_base << 15) & 0xFFFFFFFF8000LL;

        pp = (char*)&pcrv;
        *p++ = pp[5];
        *p++ = pp[4];
        *p++ = pp[3];
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }

    if (OPCR_flag) {
        if (!stream->require(6)) {
            return -6;
        }
        stream->skip(6);
    }

    if (splicing_point_flag) {
        if (!stream->require(1)) {
            return -7;
        }
        stream->write_1bytes(splice_countdown);
    }
    
    if (transport_private_data_flag) {
        if (!stream->require(1)) {
            return -8;
        }
        stream->write_1bytes(transport_private_data_length);

        if (transport_private_data_length> 0) {
            if (!stream->require(transport_private_data_length)) {
		return -9;
            }
            stream->write_bytes(transport_private_data, transport_private_data_length);
        }
    }
    
    if (adaptation_field_extension_flag) {
        if (!stream->require(2)) {
            return -10;
        }
        stream->write_1bytes(adaptation_field_extension_length);
        int8_t ltwfv = const1_value1 & 0x1F;
        ltwfv |= (ltw_flag << 7) & 0x80;
        ltwfv |= (piecewise_rate_flag << 6) & 0x40;
        ltwfv |= (seamless_splice_flag << 5) & 0x20;
        stream->write_1bytes(ltwfv);

        if (ltw_flag) {
            if (!stream->require(2)) {
                return -11;
            }
            stream->skip(2);
        }

        if (piecewise_rate_flag) {
            if (!stream->require(3)) {
                return -12;
            }
            stream->skip(3);
        }

        if (seamless_splice_flag) {
            if (!stream->require(5)) {
                return -14;
            }
            stream->skip(5);
        }

        if (nb_af_ext_reserved) {
            stream->skip(nb_af_ext_reserved);
        }
    }

    if (nb_af_reserved) {
        stream->skip(nb_af_reserved);
    }

    return ret;
}
