#include "wasapi_capturer.h"
#include <process.h>
#include "wasapi-hook.h"

#include <detours.h>

#include "wasapi_capture_proxy.h"

#define SAFE_RELEASE(X)               \
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
std::mutex gMutex;

#ifdef __cplusplus
extern "C" {
#endif
typedef HRESULT(STDMETHODCALLTYPE *AudioClientInitialize)(IAudioClient *pAudioClient, AUDCLNT_SHAREMODE ShareMode, DWORD stream_flags,
							  REFERENCE_TIME hns_buffer_duration, REFERENCE_TIME hns_periodicity, const WAVEFORMATEX *p_format,
							  LPCGUID audio_session_guid);
typedef HRESULT(STDMETHODCALLTYPE *AudioClientStop)(IAudioClient *pAudioClient);

typedef HRESULT(STDMETHODCALLTYPE *AudioRenderClientGetBuffer)(IAudioRenderClient *audio_render_client, UINT32 num_frames_requested, BYTE **ppData);
typedef HRESULT(STDMETHODCALLTYPE *AudioRenderClientReleaseBuffer)(IAudioRenderClient *audio_render_client, UINT32 num_frames_written, DWORD dw_flags);
#ifdef __cplusplus
}
#endif

AudioClientInitialize realAudioClientInitialize = NULL;
AudioClientStop realAudioClientStop = NULL;
AudioRenderClientGetBuffer realAudioRenderClientGetBuffer = NULL;
AudioRenderClientReleaseBuffer realAudioRenderClientReleaseBuffer = NULL;

HRESULT STDMETHODCALLTYPE hookAudioClientInitialize(IAudioClient *pAudioClient, AUDCLNT_SHAREMODE share_mode, DWORD stream_flags,
						    REFERENCE_TIME hns_buffer_duration, REFERENCE_TIME hns_periodicity, const WAVEFORMATEX *pFormat,
						    LPCGUID audio_session_guid)
{
	HRESULT hr = realAudioClientInitialize(pAudioClient, share_mode, stream_flags, hns_buffer_duration, hns_periodicity, pFormat, audio_session_guid);
	std::unique_lock<std::mutex> lk(gMutex);
	if (gWASAPIAudioCaptureProxy) {
		gWASAPIAudioCapture->on_init(pAudioClient, pFormat);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioClientStop(IAudioClient *pAudioClient)
{
	IAudioRenderClient *render_client = NULL;
	pAudioClient->GetService(__uuidof(IAudioRenderClient), (void **)&render_client);

	HRESULT hr = S_OK;
	hr = realAudioClientStop(pAudioClient);

	if (hr == AUDCLNT_E_SERVICE_NOT_RUNNING)
		hlog("[%d] AudioClientStopHook: called!! (%s)", GetCurrentThreadId(), "AUDCLNT_E_SERVICE_NOT_RUNNING");
	if (hr == AUDCLNT_E_NOT_INITIALIZED)
		hlog("[%d] AudioClientStopHook: called!! (%s)", GetCurrentThreadId(), "AUDCLNT_E_NOT_INITIALIZED");

	std::unique_lock<std::mutex> lk(gMutex);
	if (gWASAPIAudioCaptureProxy) {
		if (SUCCEEDED(hr)) {
			gWASAPIAudioCaptureProxy->on_audioclient_stopped(pAudioClient, render_client, FALSE);
		} else {
			gWASAPIAudioCaptureProxy->on_audioclient_stopped(pAudioClient, render_client, TRUE);
		}
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioRenderClientGetBuffer(IAudioRenderClient *pAudioRenderClient, UINT32 nFrameRequested, BYTE **ppData)
{
	HRESULT hr = realAudioRenderClientGetBuffer(pAudioRenderClient, nFrameRequested, ppData);
	std::unique_lock<std::mutex> lk(gMutex);
	if (gWASAPIAudioCaptureProxy) {
		gWASAPIAudioCaptureProxy->push_audio_data(pAudioRenderClient, ppData);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioRenderClientReleaseBuffer(IAudioRenderClient *pAudioRenderClient, UINT32 nFrameWritten, DWORD dwFlags)
{
	{
		std::unique_lock<std::mutex> lk(gMutex);
		if (gWASAPIAudioCaptureProxy) {
			gWASAPIAudioCaptureProxy->capture_audio(pAudioRenderClient, nFrameWritten, gWASAPIAudioCapture->audio_block_align(pAudioRenderClient),
								dwFlags == AUDCLNT_BUFFERFLAGS_SILENT);
			gWASAPIAudioCaptureProxy->pop_audio_data(pAudioRenderClient);
		}
	}

	return realAudioRenderClientReleaseBuffer(pAudioRenderClient, nFrameWritten, dwFlags);
}

core::core(void) : _thread(INVALID_HANDLE_VALUE), _run(FALSE)
{
	circlebuf_init(&_audio_data_buffer);
}

core::~core(void)
{
	circlebuf_free(&_audio_data_buffer);

	std::unique_lock<std::mutex> lk(_mutex);
	_audio_clients.clear();
	_audio_render_blocks.clear();
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
	_thread = (HANDLE)::_beginthreadex(NULL, 0, core::process_cb, this, 0, &thrdaddr);
	if (!_thread || _thread == INVALID_HANDLE_VALUE)
		return err_code_t::fail;
	return err_code_t::success;
}

int32_t core::stop(void)
{
	if (_run) {
		_run = FALSE;
		if (::WaitForSingleObject(_thread, INFINITE) == WAIT_OBJECT_0) {
			::CloseHandle(_thread);
			_thread = INVALID_HANDLE_VALUE;
		}
	}
	return err_code_t::success;
}

int32_t core::audio_block_align(IAudioRenderClient *render)
{
	return _audio_render_blocks[render];
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

	info._format = 0;
	if (bfloat) {
		if (wfex->wBitsPerSample == 32)
			info._format = 4;
	} else {
		switch (wfex->wBitsPerSample) {
		case 8:
			info._format = 1;
			break;
		case 16:
			info._format = 2;
			break;
		case 32:
			info._format = 3;
			break;
		default:
			break;
		}
	}

	IAudioRenderClient *render = NULL;
	audio_client->GetService(__uuidof(IAudioRenderClient), (void **)&render);
	if (render)
		_audio_render_blocks[render] = wfex->nBlockAlign;

	_audio_clients[audio_client] = info;
}

void core::on_get_current_padding(IAudioClient *audio_client, UINT32 *padding) {}

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

void core::on_stop(IAudioClient *audio_client, IAudioRenderClient *render)
{
	std::unique_lock<std::mutex> lk(_mutex);
	_audio_clients.erase(audio_client);

	if (_audio_render_blocks.count(render))
		_audio_render_blocks.erase(render);
}

IAudioClient *core::create_dummy_audio_client(void)
{
	HRESULT hr = S_OK;
	REFERENCE_TIME hnsReqDuration = REFTIMES_PER_SEC;
	IMMDeviceEnumerator *pMMDevEnum = NULL;
	IMMDevice *pMMDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	WAVEFORMATEX *pFormat = NULL;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void **)&pMMDevEnum);
	if (hr == CO_E_NOTINITIALIZED) {
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&pMMDevEnum);
		if (SUCCEEDED(hr)) {
			hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void **)&pMMDevEnum);
		}
	}

	hr = pMMDevEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pMMDevice);
	hr = pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&pAudioClient);
	hr = pAudioClient->GetMixFormat(&pFormat);
	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsReqDuration, 0, pFormat, NULL);
	
	CoTaskMemFree(pFormat);

	if (SUCCEEDED(hr)) {
		SAFE_RELEASE(pMMDevEnum);
		SAFE_RELEASE(pMMDevice);
		return pAudioClient;
	} else {
		SAFE_RELEASE(pMMDevEnum);
		SAFE_RELEASE(pMMDevice);
		SAFE_RELEASE(pAudioClient);
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

	bool success = false;
	IAudioClient *pAudioClient = create_dummy_audio_client();
	if (pAudioClient) {
		DetourTransactionBegin();

		DWORD_PTR *pAudioClientVTable = (DWORD_PTR *)pAudioClient;
		pAudioClientVTable = (DWORD_PTR *)pAudioClientVTable[0];

		realAudioClientInitialize = (AudioClientInitialize)pAudioClientVTable[3];
		DetourAttach((PVOID *)&realAudioClientInitialize, hookAudioClientInitialize);

		realAudioClientStop = (AudioClientStop)pAudioClientVTable[11];
		DetourAttach((PVOID *)&realAudioClientStop, hookAudioClientStop);

		IAudioRenderClient *pAudioRenderClient = NULL;
		pAudioClient->GetService(__uuidof(IAudioRenderClient), (void **)&pAudioRenderClient);
		DWORD_PTR *pAudioRenderClientVTable = (DWORD_PTR *)pAudioRenderClient;
		pAudioRenderClientVTable = (DWORD_PTR *)pAudioRenderClientVTable[0];

		realAudioRenderClientGetBuffer = (AudioRenderClientGetBuffer)pAudioRenderClientVTable[3];
		DetourAttach((PVOID *)&realAudioRenderClientGetBuffer, hookAudioRenderClientGetBuffer);

		realAudioRenderClientReleaseBuffer = (AudioRenderClientReleaseBuffer)pAudioRenderClientVTable[4];
		DetourAttach((PVOID *)&realAudioRenderClientReleaseBuffer, hookAudioRenderClientReleaseBuffer);

		const LONG error = DetourTransactionCommit();
		success = error == NO_ERROR;
		if (success) {
			hlog("Hooked IAudioClient::Initialize");
			hlog("Hooked IAudioClient::Stop");
			hlog("Hooked IAudioRenderClient::GetBuffer");
			hlog("Hooked IAudioRenderClient::ReleaseBuffer");
			hlog("Hooked wasapi");
		} else {
			realAudioClientInitialize = nullptr;
			realAudioClientStop = nullptr;
			realAudioRenderClientGetBuffer = nullptr;
			realAudioRenderClientReleaseBuffer = nullptr;
			hlog("Failed to attach Detours hook: %ld", error);
		}

		SAFE_RELEASE(pAudioClient);
		SAFE_RELEASE(pAudioRenderClient);
	}

	if (!success)
		return;

	while (_run) {
		{
			std::unique_lock<std::mutex> lk(_mutex);
			if (capture_should_stop()) {
				capture_reset();
			}

			if (_audio_clients.size() == 0)
				capture_reset();
			else {
				if (capture_should_init()) {
					const audio_info_t &info = _audio_clients.cbegin()->second;
					bool success = capture_init_shmem(&_shmem_data_info, &_audio_data_pointer, info._channels, info._samplerate,
									  info._byte_per_sample, info._format);
					if (!success)
						capture_reset();
				}
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
	std::unique_lock<std::mutex> lk(gMutex);

	gWASAPIAudioCapture->stop();
	gWASAPIAudioCapture->release();
	delete gWASAPIAudioCapture;
	gWASAPIAudioCapture = NULL;
	delete gWASAPIAudioCaptureProxy;
	gWASAPIAudioCaptureProxy = NULL;
}
