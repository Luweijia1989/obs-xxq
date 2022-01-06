/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <stddef.h>
#include <cstring>
#include <signal.h>
#include <string>
#include <vector>
#include <fstream>

#ifdef WIN32
#include <windows.h>
#include <synchapi.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <Shlwapi.h>
#include "resource.h"
#include <iphlpapi.h>
#include <errhandlingapi.h>
#include <dbghelp.h>
#include <Shlobj.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

#include "../ipc.h"
#include "../common-define.h"
#include "log.h"
#include "lib/raop.h"
#include "lib/stream.h"
#include "lib/logger.h"
#include "lib/dnssd.h"

#include "aacdecoder_lib.h"

#define VERSION "1.2"

#define DEFAULT_DEBUG_LOG false
#define DEFAULT_HW_ADDRESS { (char) 0x48, (char) 0x5d, (char) 0x60, (char) 0x7c, (char) 0xee, (char) 0x22 }

int start_server(std::vector<char> hw_addr, std::string name, bool debug_log);
int stop_server();

#define N_SAMPLE 480
static int pcm_pkt_size = 4 * N_SAMPLE;
static unsigned char decode_buffer[1920];

static dnssd_t *dnssd = NULL;
static raop_t *raop = NULL;
static logger_t *render_logger = NULL;
static IPCClient *ipc_client = NULL;
static HANDLE_AACDECODER aacdecoder_handler;

static HANDLE_AACDECODER create_fdk_aac_decoder()
{
	HANDLE_AACDECODER phandle = aacDecoder_Open(TT_MP4_RAW, 1);
	if (phandle == NULL) {
		LOGI("aacDecoder open faild!");
		return NULL;
	}
	/* ASC config binary data */
	UCHAR eld_conf[] = {0xF8, 0xE8, 0x50, 0x00};
	UCHAR *conf[] = {eld_conf};
	static UINT conf_len = sizeof(eld_conf);
	if (aacDecoder_ConfigRaw(phandle, conf, &conf_len) != AAC_DEC_OK) {
		LOGI("Unable to set configRaw");
		return NULL;
	}
	CStreamInfo *aac_stream_info = aacDecoder_GetStreamInfo(phandle);
	if (aac_stream_info == NULL) {
		LOGI("aacDecoder_GetStreamInfo failed!");
		return NULL;
	}
	LOGI("> stream info: channel = %d\tsample_rate = %d\tframe_size = %d\taot = %d\tbitrate = %d",
		aac_stream_info->channelConfig, aac_stream_info->aacSampleRate,
		aac_stream_info->aacSamplesPerFrame, aac_stream_info->aot,
		aac_stream_info->bitRate);
	return phandle;
}

static void close_fdk_aac_decoder(HANDLE_AACDECODER decoder)
{
	aacDecoder_Close(decoder);
}

static bool do_fdk_aac_decode(aac_decode_struct *data)
{
	unsigned int pkt_size = data->data_len;
	unsigned int valid_size = data->data_len;
	unsigned char *input_buf[1] = { data->data };
	auto ret = aacDecoder_Fill(aacdecoder_handler, input_buf, &pkt_size, &valid_size);
	if (ret != AAC_DEC_OK) {
		LOGE("aacDecoder_Fill error : %x", ret);
		return false;
	}
	ret = aacDecoder_DecodeFrame(aacdecoder_handler, (INT_PCM *)decode_buffer, pcm_pkt_size, 0);
	if (ret != AAC_DEC_OK) {
		LOGE("aacDecoder_DecodeFrame error : 0x%x", ret);
		return false;
	}

	return true;
}

static BOOL Is64BitWindows()
{
#if defined(_WIN64)
	return TRUE; // 64-bit programs run only on Win64
#elif defined(_WIN32)
	// 32-bit programs run on both 32-bit and 64-bit Windows
	// so must sniff
	BOOL f64 = FALSE;
	return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
	return FALSE; // Win64 does not support Win16
#endif
}


static void installBonjourService()
{
	BOOL is64 = Is64BitWindows();
	TCHAR path[MAX_PATH] = {0};
	DWORD len = GetTempPath(MAX_PATH, path);
	_tcscpy_s(path + len, MAX_PATH - len,
		  is64 ? L"Bonjour64.msi" : L"Bonjour.msi");

	if (!PathFileExists(path)) {
		HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ,
					  NULL, CREATE_ALWAYS,
					  FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			HRSRC res = FindResource(
				NULL,
				MAKEINTRESOURCE(is64 ? IDR_SETUPFILE2
						     : IDR_SETUPFILE1),
				RT_RCDATA);
			HGLOBAL res_handle = LoadResource(NULL, res);
			auto res_data = LockResource(res_handle);
			auto res_size = SizeofResource(NULL, res);

			DWORD written = 0;
			WriteFile(hFile, res_data, res_size, &written, NULL);
			CloseHandle(hFile);
		}
	}

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	TCHAR cmd[512] = {0};

	_stprintf_s(cmd, L"msiexec /i %s /qn", path);

	// Start the child process.
	if (!CreateProcess(NULL,  // No module name (use command line)
			   cmd,   // Command line
			   NULL,  // Process handle not inheritable
			   NULL,  // Thread handle not inheritable
			   FALSE, // Set handle inheritance to FALSE
			   0,     // No creation flags
			   NULL,  // Use parent's environment block
			   NULL,  // Use parent's starting directory
			   &si,   // Pointer to STARTUPINFO structure
			   &pi)   // Pointer to PROCESS_INFORMATION structure
	) {
		printf("CreateProcess failed (%d).\n", GetLastError());
		return;
	}

	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles.
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}


static void bonjourCheckInstall()
{
	DWORD dwOldCheckPoint;
	DWORD dwStartTickCount;
	DWORD dwWaitTime;
	SC_HANDLE schSCManager = OpenSCManager(
						NULL,                    // local computer
						NULL,                    // servicesActive database 
						SC_MANAGER_ALL_ACCESS);  // full access rights 
 
	if (NULL == schSCManager) 
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	SC_HANDLE schService = OpenService(schSCManager,        // SCM database
				 L"Bonjour Service",           // name of service
				 SERVICE_ALL_ACCESS); // full access

	if (schService == NULL) {
		installBonjourService();

		schService = OpenService(schSCManager,        // SCM database
					 L"Bonjour Service",  // name of service
					 SERVICE_ALL_ACCESS); // full access
		if (schService == NULL)
			goto END;
	}

	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwBytesNeeded;
	if (!QueryServiceStatusEx(
		    schService,                     // handle to service
		    SC_STATUS_PROCESS_INFO,         // information level
		    (LPBYTE)&ssStatus,              // address of structure
		    sizeof(SERVICE_STATUS_PROCESS), // size of structure
		    &dwBytesNeeded)) // size needed if buffer is too small
		goto END;

	if (ssStatus.dwCurrentState == SERVICE_STOPPED) {
		if (!StartService(
			schService,  // handle to service 
			0,           // number of arguments 
			NULL) )      // no arguments 
			goto END;

			// Check the status until the service is no longer start pending. 
 
		if (!QueryServiceStatusEx( 
			schService,                     // handle to service 
			SC_STATUS_PROCESS_INFO,         // info level
			(LPBYTE) &ssStatus,             // address of structure
			sizeof(SERVICE_STATUS_PROCESS), // size of structure
			&dwBytesNeeded ) )              // if buffer too small
			goto END;
 
		// Save the tick count and initial checkpoint.

		dwStartTickCount = GetTickCount();
		dwOldCheckPoint = ssStatus.dwCheckPoint;

		while (ssStatus.dwCurrentState == SERVICE_START_PENDING) 
		{ 
			// Do not wait longer than the wait hint. A good interval is 
			// one-tenth the wait hint, but no less than 1 second and no 
			// more than 10 seconds. 
 
			dwWaitTime = ssStatus.dwWaitHint / 10;

			if( dwWaitTime < 1000 )
				dwWaitTime = 1000;
			else if ( dwWaitTime > 10000 )
				dwWaitTime = 10000;

			Sleep( dwWaitTime );

			// Check the status again. 
 
			if (!QueryServiceStatusEx( 
						schService,             // handle to service 
						SC_STATUS_PROCESS_INFO, // info level
						(LPBYTE) &ssStatus,             // address of structure
						sizeof(SERVICE_STATUS_PROCESS), // size of structure
						&dwBytesNeeded ) )              // if buffer too small
			{
				printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
				break; 
			}
 
			if ( ssStatus.dwCheckPoint > dwOldCheckPoint )
			{
				// Continue to wait and check.

				dwStartTickCount = GetTickCount();
				dwOldCheckPoint = ssStatus.dwCheckPoint;
			}
			else
			{
				if(GetTickCount()-dwStartTickCount > ssStatus.dwWaitHint)
				{
					// No progress made within the wait hint.
					break;
				}
			}
		} 
	}

END:
	if (schService)
		CloseServiceHandle(schService);
	if (schSCManager)
		CloseServiceHandle(schSCManager);
}

static int parse_hw_addr(std::string str, std::vector<char> &hw_addr) {
    for (int i = 0; i < str.length(); i ++) {
        hw_addr.push_back((char)str[i]);
    }
    return 0;
}

std::string find_mac()
{
	std::string ret;
	PIP_ADAPTER_INFO pAdapterInfo;
	DWORD AdapterInfoSize;
	DWORD Err;
	AdapterInfoSize = 0;
	Err = GetAdaptersInfo(NULL, &AdapterInfoSize);
	if ((Err != 0) && (Err != ERROR_BUFFER_OVERFLOW)) {
		return ret;
	}
	
	pAdapterInfo = (PIP_ADAPTER_INFO)GlobalAlloc(GPTR, AdapterInfoSize);
	if (pAdapterInfo == NULL) {
		return ret;
	}
	if (GetAdaptersInfo(pAdapterInfo, &AdapterInfoSize) != 0) {
		GlobalFree(pAdapterInfo);
		return ret;
	}

	ret.resize(6);
	for (int i = 0; i < 6; i++) {
		ret[i] = pAdapterInfo->Address[i];
	}

	GlobalFree(pAdapterInfo);
	return ret;
}

size_t wchar_convert(const wchar_t *in, size_t insize, char *out, size_t outsize)
{
	int i_insize = (int)insize;
	int ret;

	if (i_insize == 0)
		i_insize = (int)wcslen(in);

	ret = WideCharToMultiByte(CP_UTF8, 0, in, i_insize, out, (int)outsize,
				  NULL, NULL);

	return (ret > 0) ? (size_t)ret : 0;
}

std::string get_host_name()
{
	std::string ret;
	TCHAR buffer[512] = { 0 };
	char u8_buffer[512] = { 0 };
	DWORD len = 511;
	if (GetComputerName(buffer, &len)) {
		wchar_convert(buffer, 0, u8_buffer, 512);
		ret = u8_buffer;
	}

	return ret;
}

LONG CALLBACK crash_handler(PEXCEPTION_POINTERS exception)
{
	static bool inside_handler = false;

	/* don't use if a debugger is present */
	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;

	if (inside_handler)
		return EXCEPTION_CONTINUE_SEARCH;

	inside_handler = true;

	HANDLE file = CreateFile(L"airplay-server-crash.dump", GENERIC_WRITE, 0,
				 NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
				 NULL);

	if (file != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION di;
		di.ExceptionPointers = exception;
		di.ThreadId = GetCurrentThreadId();
		di.ClientPointers = TRUE;

		BOOL ret = MiniDumpWriteDump(GetCurrentProcess(),
					     GetCurrentProcessId(), file,
					     MiniDumpNormal, &di, NULL, NULL);
		CloseHandle(file);
	}

	inside_handler = false;
	return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[]) {
    SetUnhandledExceptionFilter(crash_handler);

    bool isDebug = argc > 1 && strcmp(argv[1], "debug") == 0;
    if (!isDebug) {
	SetErrorMode(SEM_FAILCRITICALERRORS);
	freopen("NUL", "w", stderr);
    }

    bonjourCheckInstall();

    aacdecoder_handler = create_fdk_aac_decoder();
    ipc_client_create(&ipc_client);

    std::string server_name = "yuerzhibo[" + get_host_name() + "]";
    std::vector<char> server_hw_addr = DEFAULT_HW_ADDRESS;
    bool debug_log = DEFAULT_DEBUG_LOG;

    std::string mac_address = find_mac();
    if (!mac_address.empty()) {
        server_hw_addr.clear();
        parse_hw_addr(mac_address, server_hw_addr);
    }

    if (start_server(server_hw_addr, server_name, debug_log) != 0) {
        return 1;
    }

    uint8_t buf[1024] = {0};
    while (true) {
	    int read_len = fread(buf, 1, 1024,
				 stdin); // read 0 means parent has been stopped
	    if (!read_len || buf[0] == 1) {
		    break;
	    }
    }

    LOGI("Stopping...");
    stop_server();

    ipc_client_destroy(&ipc_client);
    close_fdk_aac_decoder(aacdecoder_handler);
}

// Server callbacks
extern "C" void conn_init(void *cls) {
	send_status(ipc_client, MIRROR_START);

	media_audio_info info;
	info.format = AUDIO_FORMAT_16BIT;
	info.samples_per_sec = 44100;
	info.speakers = SPEAKERS_STEREO;
	struct av_packet_info pack_info = {0};
	pack_info.size = sizeof(struct media_audio_info);
	pack_info.type = FFM_MEDIA_AUDIO_INFO;

	ipc_client_write_2(ipc_client, &pack_info, sizeof(struct av_packet_info), &info, sizeof(struct media_audio_info), INFINITE);
}

extern "C" void conn_destroy(void *cls) {
	send_status(ipc_client, MIRROR_STOP);
}

extern "C" void audio_process(void *cls, raop_ntp_t *ntp, aac_decode_struct *data) {
	if (do_fdk_aac_decode(data)) {
		struct av_packet_info pack_info = {0};
		pack_info.size = 1920;
		pack_info.type = FFM_PACKET_AUDIO;
		pack_info.pts = data->pts * 1000;
		ipc_client_write_2(ipc_client, &pack_info, sizeof(struct av_packet_info), decode_buffer, 1920, INFINITE);
	}
}

extern "C" void video_process(void *cls, raop_ntp_t *ntp, h264_decode_struct *data) {
	if (data->frame_type == 0) { //sps pps
		media_video_info info;
		memcpy(info.video_extra, data->data, data->data_len);
		info.video_extra_len = data->data_len;
		struct av_packet_info pack_info = {0};
		pack_info.size = sizeof(struct media_video_info);
		pack_info.type = FFM_MEDIA_VIDEO_INFO;

		ipc_client_write_2(ipc_client, &pack_info, sizeof(struct av_packet_info), &info, sizeof(struct media_video_info), INFINITE);
	} else {
		struct av_packet_info pack_info = {0};
		pack_info.size = data->data_len;
		pack_info.type = FFM_PACKET_VIDEO;
		pack_info.pts = data->pts * 1000;
		ipc_client_write_2(ipc_client, &pack_info, sizeof(struct av_packet_info), data->data, data->data_len, INFINITE);
	}
}

extern "C" void audio_flush(void *cls) {
    
}

extern "C" void video_flush(void *cls) {
    
}

extern "C" void audio_set_volume(void *cls, float volume) {
    
}

extern "C" void log_callback(void *cls, int level, const char *msg) {
    switch (level) {
        case LOGGER_DEBUG: {
            LOGD("%s", msg);
            break;
        }
        case LOGGER_WARNING: {
            LOGW("%s", msg);
            break;
        }
        case LOGGER_INFO: {
            LOGI("%s", msg);
            break;
        }
        case LOGGER_ERR: {
            LOGE("%s", msg);
            break;
        }
        default:
            break;
    }

}

int start_server(std::vector<char> hw_addr, std::string name, bool debug_log) {
    raop_callbacks_t raop_cbs;
    memset(&raop_cbs, 0, sizeof(raop_cbs));
    raop_cbs.conn_init = conn_init;
    raop_cbs.conn_destroy = conn_destroy;
    raop_cbs.audio_process = audio_process;
    raop_cbs.video_process = video_process;
    raop_cbs.audio_flush = audio_flush;
    raop_cbs.video_flush = video_flush;
    raop_cbs.audio_set_volume = audio_set_volume;

    raop = raop_init(10, &raop_cbs);
    if (raop == NULL) {
        LOGE("Error initializing raop!");
        return -1;
    }

    raop_set_log_callback(raop, log_callback, NULL);
    raop_set_log_level(raop, debug_log ? RAOP_LOG_DEBUG : LOGGER_INFO);

    render_logger = logger_init();
    logger_set_callback(render_logger, log_callback, NULL);
    logger_set_level(render_logger, debug_log ? LOGGER_DEBUG : LOGGER_INFO);

    unsigned short port = 0;
    raop_start(raop, &port);
    raop_set_port(raop, port);

    int error;
    dnssd = dnssd_init(name.c_str(), strlen(name.c_str()), hw_addr.data(), hw_addr.size(), &error);
    if (error) {
        LOGE("Could not initialize dnssd library!");
        return -2;
    }

    raop_set_dnssd(raop, dnssd);

    dnssd_register_raop(dnssd, port);
    dnssd_register_airplay(dnssd, port + 1);

    return 0;
}

int stop_server() {
    raop_destroy(raop);
    dnssd_unregister_raop(dnssd);
    dnssd_unregister_airplay(dnssd);
    logger_destroy(render_logger);
    return 0;
}
