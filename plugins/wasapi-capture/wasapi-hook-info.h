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

#define MUTEX_TEXTURE1 L"CaptureHook_TextureMutex1"
#define MUTEX_TEXTURE2 L"CaptureHook_TextureMutex2"

#define SHMEM_HOOK_INFO L"CaptureHook_HookInfo"
#define SHMEM_TEXTURE L"CaptureHook_Texture"

#define PIPE_NAME "CaptureHook_Pipe"

#pragma pack(push, 8)

struct shmem_data {
	volatile int last_tex;
	uint32_t tex1_offset;
	uint32_t tex2_offset;
};

struct hook_info {
	/* capture info */
	int sample_rate;
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
