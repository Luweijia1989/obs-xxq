#include "xxq-mirror.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <algorithm>
#include <sstream>
using namespace std;

#include <srs_kernel_error.hpp>
#include <srs_kernel_codec.hpp>
#include <srs_rtmp_amf0.hpp>
#include <srs_rtmp_stack.hpp>
#include <srs_app_config.hpp>
#include <srs_app_source.hpp>
#include <srs_core_autofree.hpp>
#include <srs_rtmp_stack.hpp>
#include <srs_app_pithy_print.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_kernel_codec.hpp>
#include <srs_kernel_file.hpp>
#include <srs_protocol_buffer.hpp>
#include <srs_kernel_ts.hpp>
#include <srs_app_utility.hpp>
#include <srs_app_http_hooks.hpp>

const uint8_t start_code[4] = {0x00, 0x00, 0x00, 0x01};

XXQMirror::XXQMirror()
{
	_req = NULL;
	source = NULL;

	codec = new SrsAvcAacCodec();
	sample = new SrsCodecSample();
	jitter = new SrsRtmpJitter();

	pcm_buffer = (uint8_t *)malloc(102400);
	ipc_client_create(&ipc_client);

#ifdef DUMP_AUDIO
	audio_dump = fopen("E:\\audio.pcm", "wb");
#endif // DUMP_AUDIO

}

XXQMirror::~XXQMirror()
{
#ifdef DUMP_AUDIO
	fclose(audio_dump);
#endif // DUMP_AUDIO
	srs_freep(_req);
	srs_freep(codec);
	srs_freep(sample);
	srs_freep(jitter);

	free(pcm_buffer);
	if (video_buffer)
		free(video_buffer);
	ipc_client_destroy(&ipc_client);
}

int XXQMirror::initialize(SrsSource *s)
{
	int ret = ERROR_SUCCESS;
	source = s;
	return ret;
}

int XXQMirror::on_publish(SrsRequest *req)
{
	int ret = ERROR_SUCCESS;

	srs_freep(_req);
	_req = req->copy();

	send_status(ipc_client, MIRROR_START);

	return ret;
}

void XXQMirror::on_unpublish()
{
	send_status(ipc_client, MIRROR_STOP);
}

int XXQMirror::on_meta_data(SrsAmf0Object *metadata)
{
	int ret = ERROR_SUCCESS;

	if (!metadata) {
		srs_trace("no metadata persent, hls ignored it.");
		return ret;
	}

	if (metadata->count() <= 0) {
		srs_trace("no metadata persent, hls ignored it.");
		return ret;
	}

	return ret;
}

int XXQMirror::on_audio(SrsSharedPtrMessage *shared_audio)
{
	int ret = ERROR_SUCCESS;

	SrsSharedPtrMessage *audio = shared_audio->copy();
	SrsAutoFree(SrsSharedPtrMessage, audio);

	sample->clear();
	if ((ret = codec->audio_aac_demux(audio->payload, audio->size,
					  sample)) != ERROR_SUCCESS) {
		if (ret != ERROR_HLS_TRY_MP3) {
			srs_error("hls aac demux audio failed. ret=%d", ret);
			return ret;
		}
		if ((ret = codec->audio_mp3_demux(audio->payload, audio->size,
						  sample)) != ERROR_SUCCESS) {
			srs_error("hls mp3 demux audio failed. ret=%d", ret);
			return ret;
		}
	}
	srs_info(
		"audio decoded, type=%d, codec=%d, cts=%d, size=%d, time=%" PRId64,
		sample->frame_type, codec->audio_codec_id, sample->cts,
		audio->size, audio->timestamp);
	SrsCodecAudio acodec = (SrsCodecAudio)codec->audio_codec_id;

	// ts support audio codec: aac/mp3
	if (acodec != SrsCodecAudioAAC && acodec != SrsCodecAudioMP3) {
	    return ret;
	}
	
	// ignore sequence header
	if (acodec == SrsCodecAudioAAC && sample->aac_packet_type == SrsCodecAudioTypeSequenceHeader) {
	    handle = aacDecoder_Open(codec->aac_extra_size ? TT_MP4_RAW : TT_MP4_ADTS, 1);
	    if (handle) {
		if (codec->aac_extra_size) {
		    UCHAR *conf[] = {(UCHAR *)codec->aac_extra_data};
		    const UINT conf_len = codec->aac_extra_size;
		    AAC_DECODER_ERROR err = aacDecoder_ConfigRaw(handle, conf, &conf_len);
		    if (err != AAC_DEC_OK) {
			aacDecoder_Close(handle);
			handle = nullptr;
		    }
		}
	    }

	    return ret;
	}
	
	// TODO: FIXME: config the jitter of HLS.
	if ((ret = jitter->correct(audio, SrsRtmpJitterAlgorithmOFF)) != ERROR_SUCCESS) {
	    srs_error("rtmp jitter correct audio failed. ret=%d", ret);
	    return ret;
	}
	
	for (int i=0; i < sample->nb_sample_units; i++)
	{
		const UINT pkt_size = sample->sample_units[i].size;
		UINT valid_size = pkt_size;
		UCHAR *input_buf[1] = { (UCHAR *)sample->sample_units[i].bytes };
		AAC_DECODER_ERROR err = aacDecoder_Fill(handle, input_buf, &pkt_size, &valid_size);
		if (err == AAC_DEC_OK) {
		    err = aacDecoder_DecodeFrame(handle, (INT_PCM *)pcm_buffer, 102400 / sizeof(INT_PCM), 0);
		    if (err == AAC_DEC_OK) {
			    CStreamInfo *streamInfo = aacDecoder_GetStreamInfo(handle);
			    if (streamInfo != NULL) {
#ifdef DUMP_AUDIO
				    fwrite(pcm_buffer, 1, streamInfo->frameSize * streamInfo->numChannels * sizeof(short), audio_dump);
#endif // DUMP_AUDIO
				    struct av_packet_info pack_info = {0};
				    pack_info.size =
					    streamInfo->frameSize * streamInfo->numChannels * sizeof(short);
				    pack_info.type = FFM_PACKET_AUDIO;
				    pack_info.pts = audio->timestamp * 1000000;
				    ipc_client_write_2(
					    ipc_client, &pack_info,
					    sizeof(struct av_packet_info),
					    pcm_buffer, pack_info.size, INFINITE);
			    }
		    }
		}
	}

	return ret;
}

int XXQMirror::on_video(SrsSharedPtrMessage *shared_video, bool is_sps_pps)
{
	int ret = ERROR_SUCCESS;

	SrsSharedPtrMessage *video = shared_video->copy();
	SrsAutoFree(SrsSharedPtrMessage, video);

	// user can disable the sps parse to workaround when parse sps failed.
	// @see https://github.com/ossrs/srs/issues/474
	if (is_sps_pps) {
		codec->avc_parse_sps = _srs_config->get_parse_sps(_req->vhost);
	}

	sample->clear();
	if ((ret = codec->video_avc_demux(video->payload, video->size,
					  sample)) != ERROR_SUCCESS) {
		srs_error("hls codec demux video failed. ret=%d", ret);
		return ret;
	}
	srs_info(
		"video decoded, type=%d, codec=%d, avc=%d, cts=%d, size=%d, time=%" PRId64,
		sample->frame_type, codec->video_codec_id,
		sample->avc_packet_type, sample->cts, video->size,
		video->timestamp);

	// ignore info frame,
	// @see https://github.com/ossrs/srs/issues/288#issuecomment-69863909
	if (sample->frame_type == SrsCodecVideoAVCFrameVideoInfoFrame) {
		return ret;
	}

	if (codec->video_codec_id != SrsCodecVideoAVC) {
		return ret;
	}

	// ignore sequence header
	if (sample->frame_type == SrsCodecVideoAVCFrameKeyFrame &&
	    sample->avc_packet_type == SrsCodecVideoAVCTypeSequenceHeader) {
		uint8_t sps_buf[256] = {0};
		int copy_index = 0;

		memcpy(sps_buf + copy_index, start_code, 4);
		copy_index += 4;
		memcpy(sps_buf + copy_index, codec->sequenceParameterSetNALUnit,
		       codec->sequenceParameterSetLength);
		copy_index += codec->sequenceParameterSetLength;

		memcpy(sps_buf + copy_index, start_code, 4);
		copy_index += 4;
		memcpy(sps_buf + copy_index, codec->pictureParameterSetNALUnit,
		       codec->pictureParameterSetLength);
		copy_index += codec->pictureParameterSetLength;

		struct av_packet_info pack_info = {0};
		pack_info.size = sizeof(struct media_video_info);
		pack_info.type = FFM_MEDIA_VIDEO_INFO;

		struct media_video_info info;
		info.video_extra_len = copy_index;
		memcpy(info.video_extra, sps_buf, copy_index);

		ipc_client_write_2(ipc_client, &pack_info,
				   sizeof(struct av_packet_info), &info,
				   sizeof(struct media_video_info), INFINITE);

		return ret;
	}

	if ((ret = jitter->correct(video, SrsRtmpJitterAlgorithmOFF)) !=
	    ERROR_SUCCESS) {
		srs_error("rtmp jitter correct video failed. ret=%d", ret);
		return ret;
	}

	for (int i = 0; i < sample->nb_sample_units; i++) {
		int nalu_type = sample->sample_units[i].bytes[0] & 0x1F;
		if (nalu_type != 1 && nalu_type != 5)
			continue;

		if (!video_buffer) {
			video_buffer_len = sample->sample_units[i].size + 4;
			video_buffer = (uint8_t *)malloc(video_buffer_len);
		} else {
			if (video_buffer_len < sample->sample_units[i].size + 4) {
				video_buffer_len = sample->sample_units[i].size + 4;
				video_buffer = (uint8_t *)realloc(video_buffer, video_buffer_len);
			}
		}

		memcpy(video_buffer, start_code, 4);
		memcpy(video_buffer + 4, sample->sample_units[i].bytes, sample->sample_units[i].size);

		struct av_packet_info pack_info = {0};
		pack_info.size = sample->sample_units[i].size + 4;
		pack_info.type = FFM_PACKET_VIDEO;
		pack_info.pts = video->timestamp * 1000000;
		ipc_client_write_2(ipc_client, &pack_info, sizeof(struct av_packet_info), video_buffer, pack_info.size, INFINITE);
	}

	return ret;
}
