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

class core {
	friend class proxy;

public:
	typedef struct _err_code_t {
		static const int32_t unknown = -1;
		static const int32_t success = 0;
		static const int32_t fail = 1;
	} err_code_t;

	typedef struct _audio_info_ {
		int32_t _channels;
		int32_t _samplerate;
		int32_t _byte_per_sample;
	} audio_info_t;

	core(void);
	virtual ~core(void);

	int32_t initialize(void);
	int32_t release(void);
	int32_t start(void);
	int32_t stop(void);

	void on_receive(uint8_t *data, uint32_t data_size);
	void on_stop(IAudioClient *audio_client);
	void on_init(IAudioClient *audio_client, const WAVEFORMATEX *wfex);
	void on_get_current_padding(IAudioClient *audio_client, UINT32 *padding);

private:
	IAudioClient *create_dummy_audio_client(void);
	void process(void);
	void capture_reset();
	static unsigned __stdcall process_cb(void *param);

private:
	HANDLE _thread;
	BOOL _run;

	struct shmem_data *_shmem_data_info;
	uint8_t *_audio_data_pointer;
	std::mutex _mutex;
	std::map<IAudioClient *, audio_info_t> _audio_clients;
	circlebuf _audio_data_buffer;
};

#endif
