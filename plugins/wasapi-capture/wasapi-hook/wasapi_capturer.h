#ifndef _WASAPI_AUDIO_CAPTURE_H_
#define _WASAPI_AUDIO_CAPTURE_H_

#include <windows.h>
#include <shlobj.h>
#include <psapi.h>
#pragma intrinsic(memcpy, memset, memcmp)

#include <objbase.h>
#include <string>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <mutex>
#include <map>
#include <ctime>   // std::time
#include <iomanip> // std::put_time
#include <immintrin.h>
#include <emmintrin.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>

#include <initguid.h>
#include <wincodec.h>

#include "circlebuf.h"
#include "wasapi_capture_proxy.h"

class WASCaptureData {
public:
	typedef struct _audio_info_ {
		int32_t _channels;
		int32_t _samplerate;
		int32_t _byte_per_sample;
		uint32_t _format;
	} audio_info_t;

	uint32_t waveformatOffset = 0;
	uint32_t audioClientOffset = 0;
	uint32_t bufferOffset = 0;
	WASCaptureProxy capture_proxy;

	WASCaptureData();
	~WASCaptureData();

	void on_receive(uint8_t *data, uint32_t data_size, WAVEFORMATEX *wfex);
	void on_stop(IAudioClient *audio_client, IAudioRenderClient *render);
	void on_init(IAudioClient *audio_client, const WAVEFORMATEX *wfex);

	int32_t audio_block_align(IAudioRenderClient *render);

private:
	void capture_reset();

private:
	struct shmem_data *_shmem_data_info;
	uint8_t *_audio_data_pointer;
	std::mutex _mutex;
	std::map<IAudioClient *, audio_info_t> _audio_clients;
	std::map<IAudioRenderClient *, int32_t> _audio_render_blocks;
	circlebuf _audio_data_buffer;
};

#endif
