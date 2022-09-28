/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Rtmp/utils.h"
#include "H264Rtmp.h"

#include "common-define.h"
extern IPCClient *ipc_client;

char start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
char g_buffer[1024*1024*2] = { 0 };

namespace mediakit{

H264RtmpDecoder::H264RtmpDecoder() {
    _h264frame = obtainFrame();
}

H264Frame::Ptr H264RtmpDecoder::obtainFrame() {
    auto frame = FrameImp::create<H264Frame>();
    frame->_prefix_size = 4;
    return frame;
}

/**
 * 返回不带0x00 00 00 01头的sps pps
 */
static bool getH264Config(const RtmpPacket &thiz, string &sps, string &pps) {
    if (thiz.getMediaType() != FLV_CODEC_H264) {
        return false;
    }
    if (!thiz.isCfgFrame()) {
        return false;
    }
    if (thiz.buffer.size() < 13) {
        return false;
    }
    uint16_t sps_size;
    memcpy(&sps_size, thiz.buffer.data() + 11, 2);
    sps_size = ntohs(sps_size);

    if ((int) thiz.buffer.size() < 13 + sps_size + 1 + 2) {
        return false;
    }
    uint16_t pps_size;
    memcpy(&pps_size, thiz.buffer.data() + 13 + sps_size + 1, 2);
    pps_size = ntohs(pps_size);

    if ((int) thiz.buffer.size() < 13 + sps_size + 1 + 2 + pps_size) {
        return false;
    }
    sps.assign(thiz.buffer.data() + 13, sps_size);
    pps.assign(thiz.buffer.data() + 13 + sps_size + 1 + 2, pps_size);
    return true;
}

static void sendMediaInfo(const char* sps, size_t sps_len, const char* pps, size_t pps_len) {
	char start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
	size_t copy_index = 0;
	media_video_info info;
	memcpy(info.video_extra + copy_index, start_code, 4);
	copy_index += 4;
	memcpy(info.video_extra + copy_index, sps, sps_len);
	copy_index += sps_len;
	memcpy(info.video_extra + copy_index, start_code, 4);
	copy_index += 4;
	memcpy(info.video_extra + copy_index, pps, pps_len);
	copy_index += pps_len;
	info.video_extra_len = copy_index;
	struct av_packet_info pack_info = {0};
	pack_info.size = sizeof(struct media_video_info);
	pack_info.type = FFM_MEDIA_VIDEO_INFO;

	ipc_client_write_2(ipc_client, &pack_info, sizeof(struct av_packet_info), &info, sizeof(struct media_video_info), INFINITE);
}

static void send264Data(const std::list<std::pair<char *, size_t>> &nalus, uint32_t pts, size_t total) {
	if (nalus.empty() ||  !total)
		return;

	size_t copy_index = 0;
	size_t used_count = 0;
	bool send = false;
	for (auto iter=nalus.begin(); iter!=nalus.end(); iter++) {
		auto &nalu = *iter;
		auto data = nalu.first;
		auto len = nalu.second;

		if ((data[0] & 0x1F) &0x1F == 0x06)
			continue;

		if (!send)
			send = true;

		memcpy(g_buffer + copy_index, start_code, 4);
		copy_index += 4;
		memcpy(g_buffer + copy_index, data, len);
		copy_index += len;
		used_count = used_count + 4 + len;
	}

	if (!send) {
		return;
	}

	char start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
	struct av_packet_info pack_info = {0};
	pack_info.size = used_count;
	pack_info.type = FFM_PACKET_VIDEO;
	pack_info.pts = (int64_t)pts * 1000000;
	ipc_client_write_2(ipc_client, &pack_info, sizeof(struct av_packet_info), g_buffer, used_count, INFINITE);
}

void H264RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt) {
    if (pkt->isCfgFrame()) {
        //缓存sps pps，后续插入到I帧之前
        if (!getH264Config(*pkt, _sps, _pps)) {
            WarnL << "get h264 sps/pps failed, rtmp packet is: " << hexdump(pkt->data(), pkt->size());
            return;
        }
        onGetH264(_sps.data(), _sps.size(), pkt->time_stamp, pkt->time_stamp);
        onGetH264(_pps.data(), _pps.size(), pkt->time_stamp, pkt->time_stamp);

	sendMediaInfo(_sps.data(), _sps.size(), _pps.data(), _pps.size());
        return;
    }

    if (pkt->buffer.size() > 9) {
        auto total_len = pkt->buffer.size();
        size_t offset = 5;
        uint8_t *cts_ptr = (uint8_t *) (pkt->buffer.data() + 2);
        int32_t cts = (((cts_ptr[0] << 16) | (cts_ptr[1] << 8) | (cts_ptr[2])) + 0xff800000) ^ 0xff800000;
        auto pts = pkt->time_stamp + cts;

	std::list<std::pair<char *, size_t>> nalus;
	size_t total = 0;
	size_t nalu_count = 0;
        while (offset + 4 < total_len) {
            uint32_t frame_len;
            memcpy(&frame_len, pkt->buffer.data() + offset, 4);
            frame_len = ntohl(frame_len);
            offset += 4;
            if (frame_len + offset > total_len) {
                break;
            }
            onGetH264(pkt->buffer.data() + offset, frame_len, pkt->time_stamp, pts);
	    nalus.push_back(std::make_pair(pkt->buffer.data() + offset, frame_len));
	    total += frame_len;
	    nalu_count++;
            offset += frame_len;
        }

	send264Data(nalus, pts, total + nalu_count * 4);
    }
}

inline void H264RtmpDecoder::onGetH264(const char* data, size_t len, uint32_t dts, uint32_t pts) {
    if (!len) {
        return;
    }
    _h264frame->_dts = dts;
    _h264frame->_pts = pts;
    _h264frame->_buffer.assign("\x00\x00\x00\x01", 4);  //添加264头
    _h264frame->_buffer.append(data, len);

    //写入环形缓存
    RtmpCodec::inputFrame(_h264frame);
    _h264frame = obtainFrame();
}

////////////////////////////////////////////////////////////////////////

H264RtmpEncoder::H264RtmpEncoder(const Track::Ptr &track) {
    _track = dynamic_pointer_cast<H264Track>(track);
}

void H264RtmpEncoder::makeConfigPacket(){
    if (_track && _track->ready()) {
        //尝试从track中获取sps pps信息
        _sps = _track->getSps();
        _pps = _track->getPps();
    }

    if (!_sps.empty() && !_pps.empty()) {
        //获取到sps/pps
        makeVideoConfigPkt();
        _got_config_frame = true;
    }
}

bool H264RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto data = frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto type = H264_TYPE(data[0]);
    switch (type) {
        case H264Frame::NAL_SPS: {
            if (!_got_config_frame) {
                _sps = string(data, len);
                makeConfigPacket();
            }
            break;
        }
        case H264Frame::NAL_PPS: {
            if (!_got_config_frame) {
                _pps = string(data, len);
                makeConfigPacket();
            }
            break;
        }
        default : break;
    }

    if (!_rtmp_packet) {
        _rtmp_packet = RtmpPacket::create();
        //flags/not config/cts预占位
        _rtmp_packet->buffer.resize(5);
    }

    return _merger.inputFrame(frame, [this](uint32_t dts, uint32_t pts, const Buffer::Ptr &, bool have_key_frame) {
        //flags
        _rtmp_packet->buffer[0] = FLV_CODEC_H264 | ((have_key_frame ? FLV_KEY_FRAME : FLV_INTER_FRAME) << 4);
        //not config
        _rtmp_packet->buffer[1] = true;
        int32_t cts = pts - dts;
        if (cts < 0) {
            cts = 0;
        }
        //cts
        set_be24(&_rtmp_packet->buffer[2], cts);

        _rtmp_packet->time_stamp = dts;
        _rtmp_packet->body_size = _rtmp_packet->buffer.size();
        _rtmp_packet->chunk_id = CHUNK_VIDEO;
        _rtmp_packet->stream_index = STREAM_MEDIA;
        _rtmp_packet->type_id = MSG_VIDEO;
        //输出rtmp packet
        RtmpCodec::inputRtmp(_rtmp_packet);
        _rtmp_packet = nullptr;
    }, &_rtmp_packet->buffer);
}

void H264RtmpEncoder::makeVideoConfigPkt() {
    if (_sps.size() < 4) {
        WarnL << "sps长度不足4字节";
        return;
    }
    int8_t flags = FLV_CODEC_H264;
    flags |= (FLV_KEY_FRAME << 4);
    bool is_config = true;

    auto rtmpPkt = RtmpPacket::create();
    //header
    rtmpPkt->buffer.push_back(flags);
    rtmpPkt->buffer.push_back(!is_config);
    //cts
    rtmpPkt->buffer.append("\x0\x0\x0", 3);

    //AVCDecoderConfigurationRecord start
    rtmpPkt->buffer.push_back(1); // version
    rtmpPkt->buffer.push_back(_sps[1]); // profile
    rtmpPkt->buffer.push_back(_sps[2]); // compat
    rtmpPkt->buffer.push_back(_sps[3]); // level
    rtmpPkt->buffer.push_back((char)0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
    rtmpPkt->buffer.push_back((char)0xe1); // 3 bits reserved + 5 bits number of sps (00001)
    //sps
    uint16_t size = (uint16_t)_sps.size();
    size = htons(size);
    rtmpPkt->buffer.append((char *) &size, 2);
    rtmpPkt->buffer.append(_sps);
    //pps
    rtmpPkt->buffer.push_back(1); // version
    size = (uint16_t)_pps.size();
    size = htons(size);
    rtmpPkt->buffer.append((char *) &size, 2);
    rtmpPkt->buffer.append(_pps);

    rtmpPkt->body_size = rtmpPkt->buffer.size();
    rtmpPkt->chunk_id = CHUNK_VIDEO;
    rtmpPkt->stream_index = STREAM_MEDIA;
    rtmpPkt->time_stamp = 0;
    rtmpPkt->type_id = MSG_VIDEO;
    RtmpCodec::inputRtmp(rtmpPkt);
}

}//namespace mediakit
