#include <inttypes.h>
#include <stdio.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <set>
#include <algorithm>
#include "../wasapi-hook-info.h"

#define SAFE_RELEASE(X)               \
	{                             \
		if (X) {              \
			X->Release(); \
			X = NULL;     \
		}                     \
	}

static inline uint32_t vtable_offset(HMODULE module, void *cls, unsigned int offset)
{
	uintptr_t *vtable = *(uintptr_t **)cls;
	return (uint32_t)(vtable[offset] - (uintptr_t)module);
}

void SearchMemory(PVOID waveformat, DWORD size1, std::set<uintptr_t> &waveformatCheck, PVOID audioClient, DWORD size2, std::set<uintptr_t> &audioClientCheck,
		  PVOID buffer, DWORD size3, std::set<uintptr_t> &bufferCheck)
{
	HANDLE hProcess = ::GetCurrentProcess();
	if (NULL == hProcess) {
		return;
	}

	SYSTEM_INFO info;
	GetSystemInfo(&info);
	BYTE *pSearchAddress = (BYTE *)info.lpMinimumApplicationAddress;
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

void get_wasapi_offset(struct wasapi_offset *ret)
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
	SearchMemory(pFormat, sizeof(WAVEFORMATEX), waveformatCheck, &pAudioClient, sizeof(void *), audioClientCheck, &data, sizeof(void *), bufferCheck);
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
				ret->waveformat_offset = offset;
			}

			if (audioClientCheck.find((uintptr_t)curr) != audioClientCheck.end() && !gotAudioClientOffset) {
				gotAudioClientOffset = true;
				ret->audio_client_offset = offset;
			}

			if (bufferCheck.find((uintptr_t)curr) != bufferCheck.end() && !gotBufferOffset) {
				gotBufferOffset = true;
				ret->buffer_offset = offset;
			}
			++offset;
		}
	}
	CoTaskMemFree(pFormat);

	auto module = GetModuleHandleA("AudioSes.dll");
	ret->release_buffer = vtable_offset(module, renderClient, 4);
	ret->get_service = vtable_offset(module, pAudioClient, 14);

	SAFE_RELEASE(pMMDevEnum);
	SAFE_RELEASE(pMMDevice);
	SAFE_RELEASE(pAudioClient);
	SAFE_RELEASE(renderClient);

	CoUninitialize();
}

int main(int argc, char *argv[])
{
	struct wasapi_offset offset = {0};
	get_wasapi_offset(&offset);
	printf("[wasapi]\n");
	printf("release_buffer=0x%" PRIx32 "\n", offset.release_buffer);
	printf("get_service=0x%" PRIx32 "\n", offset.get_service);
	printf("audio_client_offset=0x%" PRIx32 "\n", offset.audio_client_offset);
	printf("waveformat_offset=0x%" PRIx32 "\n", offset.waveformat_offset);
	printf("buffer_offset=0x%" PRIx32 "\n", offset.buffer_offset);

	(void)argc;
	(void)argv;
	return 0;
}
