#ifndef XXQ_MIRROR_HPP
#define XXQ_MIRROR_HPP

/*
#include <srs_app_hls.hpp>
*/
#include <srs_core.hpp>

#include <string>
#include <vector>

#include <srs_kernel_codec.hpp>
#include <srs_kernel_file.hpp>
#include <srs_app_async_call.hpp>

#include "ipc.h"
#include "common-define.h"
#include "aacdecoder_lib.h"

class SrsSharedPtrMessage;
class SrsCodecSample;
class SrsAmf0Object;
class SrsRtmpJitter;
class SrsTSMuxer;
class SrsAvcAacCodec;
class SrsRequest;
class SrsPithyPrint;
class SrsSource;
class SrsFileWriter;
class SrsSimpleBuffer;
class SrsTsAacJitter;
class SrsTsCache;
class SrsHlsSegment;
class SrsTsCache;
class SrsTsContext;

/**
* delivery RTMP stream to HLS(m3u8 and ts),
* SrsHls provides interface with SrsSource.
* TODO: FIXME: add utest for hls.
*/
class XXQMirror
{
private:
    SrsRequest* _req;
private:
    SrsSource* source;
    SrsAvcAacCodec* codec;
    SrsCodecSample* sample;
    SrsRtmpJitter* jitter;

    IPCClient *ipc_client = nullptr;
    HANDLE_AACDECODER handle = nullptr;
    uint8_t *pcm_buffer = nullptr;
    uint8_t *video_buffer = nullptr;
    size_t video_buffer_len = 0;
public:
    XXQMirror();
    virtual ~XXQMirror();
public:
    virtual int initialize(SrsSource* s);

    virtual int on_publish(SrsRequest* req);

    virtual void on_unpublish();

    virtual int on_meta_data(SrsAmf0Object* metadata);

    virtual int on_audio(SrsSharedPtrMessage* shared_audio);

    virtual int on_video(SrsSharedPtrMessage* shared_video, bool is_sps_pps);
};

#endif
