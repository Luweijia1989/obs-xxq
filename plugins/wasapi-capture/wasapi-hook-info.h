#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "hook-helpers.h"

#define EVENT_CAPTURE_RESTART L"CaptureHook_Restart"
#define EVENT_CAPTURE_STOP L"CaptureHook_Stop"

#define EVENT_HOOK_READY L"CaptureHook_HookReady"
#define EVENT_HOOK_EXIT L"CaptureHook_Exit"

#define EVENT_HOOK_INIT L"CaptureHook_Initialize"

#define WINDOW_HOOK_KEEPALIVE L"CaptureHook_KeepAlive"

#define AUDIO_DATA_MUTEX L"CaptureHook_Audio_Data_Mutex"

#define SHMEM_HOOK_INFO L"CaptureHook_HookInfo"
#define SHMEM_AUDIO L"CaptureHook_Audio"

#define PIPE_NAME "CaptureHook_Pipe"

#pragma pack(push, 8)

struct shmem_data {
	volatile int available_audio_size;
	uint32_t audio_offset;
};

struct hook_info {
	/* capture info */
	uint32_t channels;
	uint32_t samplerate;
	uint32_t byte_persample;
	uint32_t format;

	uint32_t map_id;
	uint32_t map_size;
};

#pragma pack(pop)

#define GC_MAPPING_FLAGS (FILE_MAP_READ | FILE_MAP_WRITE)

static inline HANDLE create_hook_info(DWORD id)
{
	wchar_t new_name[64];
	_snwprintf(new_name, 64, L"%s%lu", SHMEM_HOOK_INFO, id);

	return CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
				  sizeof(struct hook_info), new_name);
}
