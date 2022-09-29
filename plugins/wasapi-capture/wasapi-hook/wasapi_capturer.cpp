#include "wasapi_capturer.h"
#include <process.h>
#include "wasapi-hook.h"

#include <set>
#include <detours.h>

#include "wasapi_capture_proxy.h"

#define SAFE_RELEASE(X)               \
	{                             \
		if (X) {              \
			X->Release(); \
			X = NULL;     \
		}                     \
	}

static WASCaptureData capture_data;

#ifdef __cplusplus
extern "C" {
#endif
typedef HRESULT(STDMETHODCALLTYPE *AudioClientStop)(IAudioClient *pAudioClient);
typedef HRESULT(STDMETHODCALLTYPE *AduioClientGetCurrentPadding)(IAudioClient *pAudioClient, UINT32 *padding);

typedef HRESULT(STDMETHODCALLTYPE *AudioRenderClientGetBuffer)(IAudioRenderClient *audio_render_client, UINT32 num_frames_requested, BYTE **ppData);
typedef HRESULT(STDMETHODCALLTYPE *AudioRenderClientReleaseBuffer)(IAudioRenderClient *audio_render_client, UINT32 num_frames_written, DWORD dw_flags);
#ifdef __cplusplus
}
#endif

AudioClientStop realAudioClientStop = NULL;
AduioClientGetCurrentPadding realAudioClientGetCurrentPadding = NULL;
AudioRenderClientGetBuffer realAudioRenderClientGetBuffer = NULL;
AudioRenderClientReleaseBuffer realAudioRenderClientReleaseBuffer = NULL;

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

	if (SUCCEEDED(hr)) {
		capture_data.capture_proxy.on_audioclient_stopped(pAudioClient, render_client, FALSE);
	} else {
		capture_data.capture_proxy.on_audioclient_stopped(pAudioClient, render_client, TRUE);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioClientGetCurrentPadding(IAudioClient *pAudioClient, UINT32 *padding)
{
	HRESULT hr = realAudioClientGetCurrentPadding(pAudioClient, padding);
	capture_data.on_init(pAudioClient, *(WAVEFORMATEX **)((uint8_t *)pAudioClient + capture_data.waveformatOffset));
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioRenderClientGetBuffer(IAudioRenderClient *pAudioRenderClient, UINT32 nFrameRequested, BYTE **ppData)
{
	HRESULT hr = realAudioRenderClientGetBuffer(pAudioRenderClient, nFrameRequested, ppData);
	capture_data.capture_proxy.push_audio_data(pAudioRenderClient, ppData);
	return hr;
}

HRESULT STDMETHODCALLTYPE hookAudioRenderClientReleaseBuffer(IAudioRenderClient *pAudioRenderClient, UINT32 nFrameWritten, DWORD dwFlags)
{
	//capture_data.capture_proxy.capture_audio(pAudioRenderClient, nFrameWritten, capture_data.audio_block_align(pAudioRenderClient),
	//					 dwFlags == AUDCLNT_BUFFERFLAGS_SILENT);
	//capture_data.capture_proxy.pop_audio_data(pAudioRenderClient);

	IAudioClient *aclient = *(IAudioClient **)((uintptr_t)pAudioRenderClient + capture_data.audioClientOffset);
	uint8_t *buffer = *(uint8_t **)((uintptr_t)pAudioRenderClient + capture_data.bufferOffset);
	WAVEFORMATEX *wfex = *(WAVEFORMATEX **)((uintptr_t)pAudioRenderClient + capture_data.waveformatOffset);

	capture_data.on_receive(buffer, nFrameWritten * wfex->nChannels * wfex->wBitsPerSample / 8, wfex);

	return realAudioRenderClientReleaseBuffer(pAudioRenderClient, nFrameWritten, dwFlags);
}

WASCaptureData::WASCaptureData()
{
	capture_proxy.set_audio_capture_proxy_receiver(this);
	circlebuf_init(&_audio_data_buffer);
}

WASCaptureData::~WASCaptureData()
{
	circlebuf_free(&_audio_data_buffer);

	std::unique_lock<std::mutex> lk(_mutex);
	_audio_clients.clear();
	_audio_render_blocks.clear();
}

int32_t WASCaptureData::audio_block_align(IAudioRenderClient *render)
{
	return _audio_render_blocks[render];
}

void WASCaptureData::on_init(IAudioClient *audio_client, const WAVEFORMATEX *wfex)
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
	if (render) {
		_audio_render_blocks[render] = wfex->nBlockAlign;
		render->Release();
	}

	_audio_clients[audio_client] = info;
}

void WASCaptureData::on_receive(uint8_t *data, uint32_t data_size, WAVEFORMATEX *wfex)
{
	std::unique_lock<std::mutex> lk(_mutex);
	if (capture_active())
		circlebuf_push_back(&_audio_data_buffer, data, data_size);

	if (capture_should_stop()) {
		capture_reset();
	}

	if (/*_audio_clients.size() == 0*/ false)
		capture_reset();
	else {
		if (capture_should_init()) {
			//const audio_info_t &info = _audio_clients.cbegin()->second;
			BOOL bfloat = FALSE;
			if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
				WAVEFORMATEXTENSIBLE *wfext = (WAVEFORMATEXTENSIBLE *)wfex;
				if (wfext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
					bfloat = TRUE;
			} else if (wfex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
				bfloat = TRUE;
			}

			uint32_t _format = 0;
			if (bfloat) {
				if (wfex->wBitsPerSample == 32)
					_format = 4;
			} else {
				switch (wfex->wBitsPerSample) {
				case 8:
					_format = 1;
					break;
				case 16:
					_format = 2;
					break;
				case 32:
					_format = 3;
					break;
				default:
					break;
				}
			}
			bool success = capture_init_shmem(&_shmem_data_info, &_audio_data_pointer, wfex->nChannels, wfex->nSamplesPerSec,
							  wfex->wBitsPerSample / 8, _format);
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

void WASCaptureData::on_stop(IAudioClient *audio_client, IAudioRenderClient *render)
{
	std::unique_lock<std::mutex> lk(_mutex);
	_audio_clients.erase(audio_client);

	if (_audio_render_blocks.count(render))
		_audio_render_blocks.erase(render);
}

void SearchMemory(PVOID waveformat, DWORD size1, std::set<uintptr_t> &waveformatCheck, PVOID audioClient, DWORD size2, std::set<uintptr_t> &audioClientCheck,
		  PVOID buffer, DWORD size3, std::set<uintptr_t> &bufferCheck, BYTE *startPoint)
{
	HANDLE hProcess = ::GetCurrentProcess();
	if (NULL == hProcess) {
		return;
	}
	BYTE *pSearchAddress = startPoint;
	MEMORY_BASIC_INFORMATION mbi = {0};
	DWORD dwRet = 0;
	BOOL bRet = false;
	BYTE *pTemp = nullptr;
	DWORD i = 0;
	BYTE *pBuf = nullptr;

	while (true) {
		::RtlZeroMemory(&mbi, sizeof(mbi));
		dwRet = ::VirtualQueryEx(hProcess, pSearchAddress, &mbi, sizeof(mbi));
		if (0 == dwRet) {
			break;
		}
		if ((MEM_COMMIT == mbi.State) && (PAGE_READONLY == mbi.Protect || PAGE_READWRITE == mbi.Protect || PAGE_EXECUTE_READ == mbi.Protect ||
						  PAGE_EXECUTE_READWRITE == mbi.Protect)) {
			pBuf = new BYTE[mbi.RegionSize];
			::RtlZeroMemory(pBuf, mbi.RegionSize);
			bRet = ::ReadProcessMemory(hProcess, mbi.BaseAddress, pBuf, mbi.RegionSize, &dwRet);
			if (FALSE == bRet) {
				break;
			}

			auto max = std::max<DWORD>(std::max<DWORD>(size1, size2), size3);
			for (i = 0; i < (mbi.RegionSize - max); i++) {
				pTemp = (BYTE *)pBuf + i;
				if (RtlEqualMemory(pTemp, waveformat, size1)) {
					waveformatCheck.insert((uintptr_t)((BYTE *)mbi.BaseAddress + i));
				}

				if (RtlEqualMemory(pTemp, audioClient, size2)) {
					audioClientCheck.insert((uintptr_t)((BYTE *)mbi.BaseAddress + i));
				}

				if (RtlEqualMemory(pTemp, buffer, size3)) {
					bufferCheck.insert((uintptr_t)((BYTE *)mbi.BaseAddress + i));
				}
			}

			delete[] pBuf;
			pBuf = NULL;
		}
		pSearchAddress = pSearchAddress + mbi.RegionSize;
	}

	if (pBuf) {
		delete[] pBuf;
		pBuf = NULL;
	}
	::CloseHandle(hProcess);
}

void WASCaptureData::capture_reset()
{
	capture_free();
	circlebuf_free(&_audio_data_buffer);
}

IAudioClient *createDummyAudioClient(void)
{
	::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	REFERENCE_TIME hnsReqDuration = 10000000;
	IMMDeviceEnumerator *pMMDevEnum = NULL;
	IMMDevice *pMMDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	WAVEFORMATEX *pFormat = NULL;

	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void **)&pMMDevEnum);
	if (hr == CO_E_NOTINITIALIZED) {
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&pMMDevEnum);
		if (SUCCEEDED(hr)) {
			hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void **)&pMMDevEnum);
		}
	}

	pMMDevEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pMMDevice);
	pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&pAudioClient);
	pAudioClient->GetMixFormat(&pFormat);
	pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsReqDuration, 0, pFormat, NULL);
	IAudioRenderClient *renderClient = nullptr;
	pAudioClient->GetService(__uuidof(IAudioRenderClient), (void **)&renderClient);
	BYTE *data = NULL;
	renderClient->GetBuffer(1024, &data);

	std::set<uintptr_t> waveformatCheck;
	std::set<uintptr_t> audioClientCheck;
	std::set<uintptr_t> bufferCheck;
	SearchMemory(pFormat, sizeof(WAVEFORMATEX), waveformatCheck, &pAudioClient, sizeof(void *), audioClientCheck, &data, sizeof(void *), bufferCheck,
		     (BYTE *)renderClient);
	if (waveformatCheck.size() > 0 && audioClientCheck.size() > 0 && bufferCheck.size() > 0) {
		uint8_t *pRawAudioClient = (uint8_t *)renderClient;
		bool gotWaveformatOffset = false;
		bool gotAudioClientOffset = false;
		bool gotBufferOffset = false;
		uint32_t offset = 0;
		while (offset < 1024) // The highest offset I got was 904, i never got any segfault errors. This value should maybe be tweaked
		{
			uintptr_t *curr = (uintptr_t *)(pRawAudioClient + offset);

			if (waveformatCheck.find(*curr) != waveformatCheck.end() && !gotWaveformatOffset) {
				gotWaveformatOffset = true;
				capture_data.waveformatOffset = offset;
			}

			if (audioClientCheck.find((uintptr_t)curr) != audioClientCheck.end() && !gotAudioClientOffset) {
				gotAudioClientOffset = true;
				capture_data.audioClientOffset = offset;
			}

			if (bufferCheck.find((uintptr_t)curr) != bufferCheck.end() && !gotBufferOffset) {
				gotBufferOffset = true;
				capture_data.bufferOffset = offset;
			}
			++offset;
		}
	}
	CoTaskMemFree(pFormat);

	SAFE_RELEASE(pMMDevEnum);
	SAFE_RELEASE(pMMDevice);
	SAFE_RELEASE(renderClient);

	CoUninitialize();
	return pAudioClient;
}

bool hook_wasapi()
{
	bool success = false;
	IAudioClient *pAudioClient = createDummyAudioClient();
	if (pAudioClient) {

		DetourTransactionBegin();

		DWORD_PTR *pAudioClientVTable = (DWORD_PTR *)pAudioClient;
		pAudioClientVTable = (DWORD_PTR *)pAudioClientVTable[0];

		//realAudioClientStop = (AudioClientStop)pAudioClientVTable[11];
		//DetourAttach((PVOID *)&realAudioClientStop, hookAudioClientStop);

		//realAudioClientGetCurrentPadding = (AduioClientGetCurrentPadding)pAudioClientVTable[6];
		//DetourAttach((PVOID *)&realAudioClientGetCurrentPadding, hookAudioClientGetCurrentPadding);

		IAudioRenderClient *pAudioRenderClient = NULL;
		pAudioClient->GetService(__uuidof(IAudioRenderClient), (void **)&pAudioRenderClient);
		DWORD_PTR *pAudioRenderClientVTable = (DWORD_PTR *)pAudioRenderClient;
		pAudioRenderClientVTable = (DWORD_PTR *)pAudioRenderClientVTable[0];

		//realAudioRenderClientGetBuffer = (AudioRenderClientGetBuffer)pAudioRenderClientVTable[3];
		//DetourAttach((PVOID *)&realAudioRenderClientGetBuffer, hookAudioRenderClientGetBuffer);

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
			realAudioClientStop = nullptr;
			realAudioRenderClientGetBuffer = nullptr;
			realAudioRenderClientReleaseBuffer = nullptr;
			hlog("Failed to attach Detours hook: %ld", error);
		}

		SAFE_RELEASE(pAudioClient);
		SAFE_RELEASE(pAudioRenderClient);
	}

	return success;
}
