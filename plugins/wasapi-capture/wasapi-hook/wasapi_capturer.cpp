#include "wasapi_capturer.h"
#include <process.h>
#include "MinHook.h"
#include "wasapi-hook.h"

#include "wasapi_capture_proxy.h"

#define ELASTICS_SAFE_RELEASE(X)      \
	{                             \
		if (X) {              \
			X->Release(); \
			X = NULL;     \
		}                     \
	}

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

BOOL gRender = FALSE;
core *gWASAPIAudioCapture = nullptr;
proxy *gWASAPIAudioCaptureProxy = nullptr;

#ifdef __cplusplus
extern "C" {
#endif
typedef HRESULT(STDMETHODCALLTYPE *AudioClientInitialize)(IAudioClient *pAudioClient, AUDCLNT_SHAREMODE ShareMode, DWORD stream_flags, REFERENCE_TIME hns_buffer_duration, REFERENCE_TIME hns_periodicity, const WAVEFORMATEX *p_format, LPCGUID audio_session_guid);
typedef HRESULT(STDMETHODCALLTYPE *AudioClientIsFormatSupported)(IAudioClient *pAudioClient, AUDCLNT_SHAREMODE ShareMode, const WAVEFORMATEX *pFormat, WAVEFORMATEX **ppClosestMatch);
typedef HRESULT(STDMETHODCALLTYPE *AudioClientGetMixFormat)(IAudioClient *pAudioClient, WAVEFORMATEX **ppDeviceFormat);
typedef HRESULT(STDMETHODCALLTYPE *AudioClientStop)(IAudioClient *pAudioClient);
typedef HRESULT(STDMETHODCALLTYPE *AudioClientReset)(IAudioClient *pAudioClient);
typedef HRESULT(STDMETHODCALLTYPE *AduioClientGetCurrentPadding)(IAudioClient* pAudioClient, UINT32* padding);

typedef HRESULT(STDMETHODCALLTYPE *AudioRenderClientGetBuffer)(IAudioRenderClient *audio_render_client, UINT32 num_frames_requested, BYTE **ppData);
typedef HRESULT(STDMETHODCALLTYPE *AudioRenderClientReleaseBuffer)(IAudioRenderClient *audio_render_client, UINT32 num_frames_written, DWORD dw_flags);
#ifdef __cplusplus
}
#endif

AudioClientInitialize realAudioClientInitialize = NULL;
AudioClientIsFormatSupported realAudioClientIsFormatSupported = NULL;
AudioClientGetMixFormat realAudioClientGetMixFormat = NULL;
AudioClientStop realAudioClientStop = NULL;
AudioClientReset realAudioClientReset = NULL;
AduioClientGetCurrentPadding realAudioClientGetCurrentPadding = NULL;

AudioRenderClientGetBuffer realAudioRenderClientGetBuffer = NULL;
AudioRenderClientReleaseBuffer realAudioRenderClientReleaseBuffer = NULL;

HRESULT STDMETHODCALLTYPE hookAudioClientInitialize(
	IAudioClient *pAudioClient, AUDCLNT_SHAREMODE share_mode,
	DWORD stream_flags, REFERENCE_TIME hns_buffer_duration,
	REFERENCE_TIME hns_periodicity, const WAVEFORMATEX *pFormat,
	LPCGUID audio_session_guid)
{
	HRESULT hr = realAudioClientInitialize(pAudioClient, share_mode, stream_flags, hns_buffer_duration, hns_periodicity, pFormat, audio_session_guid);
	gWASAPIAudioCapture->on_init(pAudioClient, pFormat);
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioClientIsFormatSupported(
	IAudioClient *pAudioClient, AUDCLNT_SHAREMODE ShareMode,
	const WAVEFORMATEX *pFormat, WAVEFORMATEX **ppClosestMatch)
{
	HRESULT hr = realAudioClientIsFormatSupported(pAudioClient, ShareMode, pFormat, ppClosestMatch);
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioClientGetMixFormat(
	IAudioClient *pAudioClient, WAVEFORMATEX **ppDeviceFormat)
{
	HRESULT hr = S_OK;
	hr = realAudioClientGetMixFormat(pAudioClient, ppDeviceFormat);

	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioClientStop(IAudioClient *pAudioClient)
{
	HRESULT hr = S_OK;

	hr = realAudioClientStop(pAudioClient);

	if (hr == AUDCLNT_E_SERVICE_NOT_RUNNING)
		hlog("[%d] AudioClientStopHook: called!! (%s)",
		     GetCurrentThreadId(), "AUDCLNT_E_SERVICE_NOT_RUNNING");
	if (hr == AUDCLNT_E_NOT_INITIALIZED)
		hlog("[%d] AudioClientStopHook: called!! (%s)",
		     GetCurrentThreadId(), "AUDCLNT_E_NOT_INITIALIZED");

	if (SUCCEEDED(hr)) {
		gWASAPIAudioCaptureProxy->on_audioclient_stopped(pAudioClient, FALSE);
	} else {
		gWASAPIAudioCaptureProxy->on_audioclient_stopped(pAudioClient, TRUE);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioClientReset(IAudioClient *pAudioClient)
{
	HRESULT hr = S_OK;
	hr = realAudioClientReset(pAudioClient);

	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioClientGetCurrentPadding(IAudioClient *pAudioClient, UINT32* padding)
{
	HRESULT hr = S_OK;
	hr = realAudioClientGetCurrentPadding(pAudioClient, padding);

	return hr;
}

HRESULT STDMETHODCALLTYPE
hookAudioRenderClientGetBuffer(IAudioRenderClient *pAudioRenderClient,
			       UINT32 nFrameRequested, BYTE **ppData)
{
	HRESULT hr = realAudioRenderClientGetBuffer(pAudioRenderClient,
						    nFrameRequested, ppData);
	gWASAPIAudioCaptureProxy->push_audio_data(pAudioRenderClient, ppData);
	return hr;
}

HRESULT STDMETHODCALLTYPE
hookAudioRenderClientReleaseBuffer(IAudioRenderClient *pAudioRenderClient,
				   UINT32 nFrameWritten, DWORD dwFlags)
{
	static int cc = 0;
	hlog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d", cc++);
	/* AUDCLNT_BUFFERFLAGS_SILENT = 0x2
	Treat all of the data in the packet as silence and ignore the actual data values */

	//if (dwFlags == AUDCLNT_BUFFERFLAGS_SILENT) {
	//	//gWASAPIAudioCaptureProxy->output_stream_added(pAudioRenderClient);
	//} else {
	gWASAPIAudioCaptureProxy->capture_audio(
		pAudioRenderClient,
		gWASAPIAudioCaptureProxy->front_audio_data(pAudioRenderClient),
		nFrameWritten);
	gWASAPIAudioCaptureProxy->pop_audio_data(pAudioRenderClient);
	//}

	return realAudioRenderClientReleaseBuffer(pAudioRenderClient,
						  nFrameWritten, dwFlags);
}

core::core(void) : _thread(INVALID_HANDLE_VALUE), _run(FALSE)
{
	circlebuf_init(&_audio_data_buffer);
}

core::~core(void)
{
	circlebuf_free(&_audio_data_buffer);
}

int32_t core::initialize(void)
{
	gWASAPIAudioCaptureProxy->initialize();
	gWASAPIAudioCaptureProxy->set_audio_capture_proxy_receiver(this);

	return err_code_t::success;
}

int32_t core::release(void)
{
	gWASAPIAudioCaptureProxy->reset_data();
	gWASAPIAudioCaptureProxy->set_audio_capture_proxy_receiver(NULL);

	return err_code_t::success;
}

int32_t core::start(void)
{
	_run = TRUE;
	unsigned thrdaddr = 0;
	_thread = (HANDLE)::_beginthreadex(NULL, 0, core::process_cb, this, 0,
					   &thrdaddr);
	if (!_thread || _thread == INVALID_HANDLE_VALUE)
		return err_code_t::fail;
	return err_code_t::success;
}

int32_t core::stop(void)
{
	_run = FALSE;
	if (::WaitForSingleObject(_thread, INFINITE) == WAIT_OBJECT_0) {
		::CloseHandle(_thread);
		_thread = INVALID_HANDLE_VALUE;
	}
	return err_code_t::success;
}

void core::on_init(IAudioClient *audio_client, const WAVEFORMATEX *wfex)
{
	std::unique_lock<std::mutex> lk(_mutex);

	if (_audio_clients.count(audio_client) > 0)
		return;

	BOOL bfloat = FALSE;
	if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE *wfext = (WAVEFORMATEXTENSIBLE *)wfex;
		if (wfext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
			bfloat = TRUE;
	} else if (wfex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
		bfloat = TRUE;
	}

	audio_info_t info;
	info._channels = wfex->nChannels;
	info._samplerate = wfex->nSamplesPerSec;
	info._byte_per_sample = wfex->wBitsPerSample / 8;

	info._channels = 2;
	info._samplerate = 48000;
	info._byte_per_sample = 4;

	_audio_clients[audio_client] = info;

	if (!_new_begin && _audio_clients.size() == 1)
		_new_begin = true;
}

void core::on_get_current_padding(IAudioClient *audio_client, UINT32 *padding)
{

}

void core::on_receive(uint8_t *data, uint32_t data_size)
{
	std::unique_lock<std::mutex> lk(_mutex);
	if (capture_active())
		circlebuf_push_back(&_audio_data_buffer, data, data_size);
}

#if 0
WAVEFORMATPCMEX 	waveFormatPCMEx;
waveFormatPCMEx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
waveFormatPCMEx.Format.nChannels = 4;
waveFormatPCMEx.Format.nSamplesPerSec = 44100L;
waveFormatPCMEx.Format.nAvgBytesPerSec = 352800L;
waveFormatPCMEx.Format.nBlockAlign = 8;  /* Same as the usual */
waveFormatPCMEx.Format.wBitsPerSample = 16;
waveFormatPCMEx.Format.cbSize = 22;  /* After this to GUID */
waveFormatPCMEx.wValidBitsPerSample = 16;  /* All bits have data */
waveFormatPCMEx.dwChannelMask = SPEAKER_FRONT_LEFT |
SPEAKER_FRONT_RIGHT |
SPEAKER_BACK_LEFT |
SPEAKER_BACK_RIGHT;
// Quadraphonic = 0x00000033
waveFormatPCMEx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;  // Specify PCM
#endif

void core::on_stop(IAudioClient *audio_client)
{
	std::unique_lock<std::mutex> lk(_mutex);
	_audio_clients.erase(audio_client);
}

IAudioClient *core::create_dummy_audio_client(void)
{
	HRESULT hr = S_OK;
	REFERENCE_TIME hnsReqDuration = REFTIMES_PER_SEC;
	IMMDeviceEnumerator *pMMDevEnum = NULL;
	IMMDevice *pMMDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	WAVEFORMATEX *pFormat = NULL;

	//LOGGER::make_trace_log(ELASC, "%s()_%d ", __FUNCTION__, __LINE__);

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
			      CLSCTX_INPROC_SERVER,
			      __uuidof(IMMDeviceEnumerator),
			      (void **)&pMMDevEnum);
	if (hr == CO_E_NOTINITIALIZED) {
		/*
		std::wostringstream wos2;
		wos2 << "[" << GetCurrentThreadId() << "] CoCreateInstance fails with CO_E_NOTINITIALIZED" << std::endl;
		OutputDebugString(wos2.str().c_str());
		*/

		// We have seen crashes which indicates that this method can in fact
		// fail with CO_E_NOTINITIALIZED in combination with certain 3rd party
		// modules. Calling CoInitializeEx is an attempt to resolve the reported
		// issues.
		//LOGGER::make_trace_log(ELASC, "%s()_%d : CoCreateInstance CO_E_NOTINITIALIZED", __FUNCTION__, __LINE__);
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		//LOGGER::make_trace_log(ELASC, "%s()_%d ", __FUNCTION__, __LINE__);
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
				      CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
				      (void **)&pMMDevEnum);
		//LOGGER::make_trace_log(ELASC, "%s()_%d : hr=%d", __FUNCTION__, __LINE__, hr);
		if (SUCCEEDED(hr)) {
			hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
					      NULL, CLSCTX_INPROC_SERVER,
					      __uuidof(IMMDeviceEnumerator),
					      (void **)&pMMDevEnum);
		}
	}
	//LOGGER::make_trace_log(ELASC, "%s()_%d : hr=%d", __FUNCTION__, __LINE__, hr);
	hr = pMMDevEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pMMDevice);
	hr = pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
				 (void **)&pAudioClient);
	hr = pAudioClient->GetMixFormat(&pFormat);

//#define	CHROMIUM_HARD_CODED_VALUE
#ifdef CHROMIUM_HARD_CODED_VALUE
	int channels = p_wfx->nChannels;
	// Preferred sample rate.
	int sample_rate = p_wfx->nSamplesPerSec;

	// We use a hard-coded value of 16 bits per sample today even if most audio
	// engines does the actual mixing in 32 bits per sample.
	int bits_per_sample = 16;
	g_nblock_align = (bits_per_sample / 8) * channels;
#else
#if 0
	WAVEFORMATPCMEX waveFormatPCMEx;
	waveFormatPCMEx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	waveFormatPCMEx.Format.nChannels = 2;
	waveFormatPCMEx.Format.nSamplesPerSec = 44100L;
	waveFormatPCMEx.Format.nAvgBytesPerSec = 176400L;
	waveFormatPCMEx.Format.nBlockAlign = 4;  /* Same as the usual */
	waveFormatPCMEx.Format.wBitsPerSample = 16;
	waveFormatPCMEx.Format.cbSize = 22;  /* After this to GUID */
	waveFormatPCMEx.Samples.wValidBitsPerSample = 16;  /* All bits have data */
	waveFormatPCMEx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	waveFormatPCMEx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;  // Specify PCM

	pFormat = (WAVEFORMATEX*)&waveFormatPCMEx;
	/*
	pFormat->cbSize = 0;
	pFormat->nAvgBytesPerSec = 176400;
	pFormat->nBlockAlign = 4;
	pFormat->nChannels = 2;
	pFormat->nSamplesPerSec = 44100;
	pFormat->wBitsPerSample = 16;
	pFormat->wFormatTag = 1;
	*/
#endif
#endif
	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
				      hnsReqDuration, 0, pFormat, NULL);

	if (SUCCEEDED(hr)) {
		CoTaskMemFree(pFormat);
		ELASTICS_SAFE_RELEASE(pMMDevEnum);
		ELASTICS_SAFE_RELEASE(pMMDevice);
		return pAudioClient;
	} else {
		CoTaskMemFree(pFormat);
		ELASTICS_SAFE_RELEASE(pMMDevEnum);
		ELASTICS_SAFE_RELEASE(pMMDevice);
		ELASTICS_SAFE_RELEASE(pAudioClient);
		return NULL;
	}
}

void core::capture_reset()
{
	capture_free();
	circlebuf_free(&_audio_data_buffer);
}

unsigned core::process_cb(void *param)
{
	core *self = static_cast<core *>(param);
	self->process();
	return 0;
}

void core::process(void)
{
	::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	HRESULT hr = S_OK;
#if 1
	IAudioClient *pAudioClient = create_dummy_audio_client();
#else
	REFERENCE_TIME hnsReqDuration = REFTIMES_PER_SEC;
	IMMDeviceEnumerator *pMMDevEnum = NULL;
	IMMDevice *pMMDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	WAVEFORMATEX *pFormat = NULL;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
			      CLSCTX_INPROC_SERVER,
			      __uuidof(IMMDeviceEnumerator),
			      (void **)&pMMDevEnum);
	if (hr == CO_E_NOTINITIALIZED) {
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
				      CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
				      (void **)&pMMDevEnum);
		if (SUCCEEDED(hr))
			hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
					      NULL, CLSCTX_INPROC_SERVER,
					      __uuidof(IMMDeviceEnumerator),
					      (void **)&pMMDevEnum);
	}
	hr = pMMDevEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pMMDevice);
	hr = pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
				 (void **)&pAudioClient);
	hr = pAudioClient->GetMixFormat(&pFormat);

	/*
	WAVEFORMATPCMEX waveFormatPCMEx;
	waveFormatPCMEx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	waveFormatPCMEx.Format.nChannels = 2;
	waveFormatPCMEx.Format.nSamplesPerSec = 48000L;
	waveFormatPCMEx.Format.nAvgBytesPerSec = 192000L;
	waveFormatPCMEx.Format.nBlockAlign = 4;
	waveFormatPCMEx.Format.wBitsPerSample = 16;
	waveFormatPCMEx.Format.cbSize = 22;
	waveFormatPCMEx.Samples.wValidBitsPerSample = 16;
	waveFormatPCMEx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	waveFormatPCMEx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;  // Specify PCM
	pFormat = (WAVEFORMATEX*)&waveFormatPCMEx;
	*/
#endif

	IAudioRenderClient *pAudioRenderClient = NULL;
	DWORD_PTR *pAudioClientVTable = NULL;
	DWORD_PTR *pAudioRenderClientVTable = NULL;

	if (MH_Initialize() != MH_OK)
		return;

	do {
		if (pAudioClient) {
			pAudioClientVTable = (DWORD_PTR *)pAudioClient;
			pAudioClientVTable = (DWORD_PTR *)pAudioClientVTable[0];

			if (MH_CreateHook((DWORD_PTR *)pAudioClientVTable[3], hookAudioClientInitialize, reinterpret_cast<void **>(&realAudioClientInitialize)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioClientVTable[3]) != MH_OK)
				break;
			if (MH_CreateHook((DWORD_PTR *)pAudioClientVTable[6], hookAudioClientGetCurrentPadding, reinterpret_cast<void **>(&realAudioClientGetCurrentPadding)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioClientVTable[6]) != MH_OK)
				break;
			if (MH_CreateHook((DWORD_PTR *)pAudioClientVTable[7], hookAudioClientIsFormatSupported, reinterpret_cast<void **>(&realAudioClientIsFormatSupported)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioClientVTable[7]) != MH_OK)
				break;
			if (MH_CreateHook((DWORD_PTR *)pAudioClientVTable[8], hookAudioClientGetMixFormat, reinterpret_cast<void **>(&realAudioClientGetMixFormat)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioClientVTable[8]) != MH_OK)
				break;
			if (MH_CreateHook((DWORD_PTR *)pAudioClientVTable[11], hookAudioClientStop, reinterpret_cast<void **>(&realAudioClientStop)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioClientVTable[11]) != MH_OK)
				break;
			if (MH_CreateHook((DWORD_PTR *)pAudioClientVTable[12], hookAudioClientReset, reinterpret_cast<void **>(&realAudioClientReset)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioClientVTable[12]) != MH_OK)
				break;

#if 0
			hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsReqDuration, 0, pFormat, NULL);
			if (SUCCEEDED(hr))
			{
				CoTaskMemFree(pFormat);
				ELASTICS_SAFE_RELEASE(pMMDevEnum);
				ELASTICS_SAFE_RELEASE(pMMDevice);
			}
			else
			{
				CoTaskMemFree(pFormat);
				ELASTICS_SAFE_RELEASE(pMMDevEnum);
				ELASTICS_SAFE_RELEASE(pMMDevice);
				ELASTICS_SAFE_RELEASE(pAudioClient);
				break;
			}
#endif

			hr = pAudioClient->GetService(__uuidof(IAudioRenderClient), (void **)&pAudioRenderClient);
			pAudioRenderClientVTable = (DWORD_PTR *)pAudioRenderClient;
			pAudioRenderClientVTable = (DWORD_PTR *)pAudioRenderClientVTable[0];
			if (MH_CreateHook((DWORD_PTR *)pAudioRenderClientVTable[3], hookAudioRenderClientGetBuffer, reinterpret_cast<void **>(&realAudioRenderClientGetBuffer)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioRenderClientVTable[3]) != MH_OK)
				break;
			if (MH_CreateHook((DWORD_PTR *)pAudioRenderClientVTable[4], hookAudioRenderClientReleaseBuffer, reinterpret_cast<void **>(&realAudioRenderClientReleaseBuffer)) != MH_OK)
				break;
			if (MH_EnableHook((DWORD_PTR *)pAudioRenderClientVTable[4]) != MH_OK)
				break;
		}
	} while (0);

	while (_run) {
		{
			std::unique_lock<std::mutex> lk(_mutex);
			if (capture_should_stop() || _new_begin) {
				capture_reset();
				_new_begin = false;
			}
			if (capture_should_init()) {
				if (_audio_clients.size() > 0) {
					const audio_info_t &info = _audio_clients.cbegin()->second;
					bool success = capture_init_shmem(&_shmem_data_info, &_audio_data_pointer, info._channels, info._samplerate, info._byte_per_sample);
					if (!success)
						capture_reset();
				} else
					capture_reset();
			}
			if (capture_active()) {
				DWORD wait_result = WAIT_FAILED;
				wait_result = WaitForSingleObject(tex_mutexe, 0);
				if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {
					size_t len = _audio_data_buffer.size;
					if (len > 0) {
						circlebuf_pop_front(&_audio_data_buffer, _audio_data_pointer + _shmem_data_info->available_audio_size, len);
						_shmem_data_info->available_audio_size += len;
					}
					ReleaseMutex(tex_mutexe);
				}
			}
		}
		::Sleep(10);
	}

	if (MH_DisableHook((DWORD_PTR *)pAudioRenderClientVTable[4]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioRenderClientVTable[4]);
	if (MH_DisableHook((DWORD_PTR *)pAudioRenderClientVTable[3]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioRenderClientVTable[3]);

	::Sleep(1);

	if (MH_DisableHook((DWORD_PTR *)pAudioClientVTable[12]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioClientVTable[12]);
	if (MH_DisableHook((DWORD_PTR *)pAudioClientVTable[11]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioClientVTable[11]);
	if (MH_DisableHook((DWORD_PTR *)pAudioClientVTable[8]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioClientVTable[8]);
	if (MH_DisableHook((DWORD_PTR *)pAudioClientVTable[7]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioClientVTable[7]);
	if (MH_DisableHook((DWORD_PTR *)pAudioClientVTable[6]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioClientVTable[6]);
	if (MH_DisableHook((DWORD_PTR *)pAudioClientVTable[3]) == MH_OK)
		MH_RemoveHook((DWORD_PTR *)pAudioClientVTable[3]);

	::Sleep(1);

	ELASTICS_SAFE_RELEASE(pAudioClient);
	ELASTICS_SAFE_RELEASE(pAudioRenderClient);

	::Sleep(1);

	MH_Uninitialize();

	::CoUninitialize();
}

bool hook_wasapi()
{
	gWASAPIAudioCapture->start();
	return true;
}

void init_var()
{
	gWASAPIAudioCaptureProxy = new proxy();
	gWASAPIAudioCapture = new core;
	gWASAPIAudioCapture->initialize();
}

void reset_var()
{
	gWASAPIAudioCapture->stop();
	gWASAPIAudioCapture->release();
	delete gWASAPIAudioCapture;
	delete gWASAPIAudioCaptureProxy;
}
