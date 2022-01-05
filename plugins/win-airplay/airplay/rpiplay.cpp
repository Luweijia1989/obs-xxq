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

#define DEFAULT_NAME "yuerzhibo"
#define DEFAULT_LOW_LATENCY false
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

static void do_fdk_aac_decode(aac_decode_struct *data)
{
	unsigned int pkt_size = data->data_len;
	unsigned int valid_size = data->data_len;
	unsigned char *input_buf[1] = { data->data };
	auto ret = aacDecoder_Fill(aacdecoder_handler, input_buf, &pkt_size, &valid_size);
	if (ret != AAC_DEC_OK) {
		LOGE("aacDecoder_Fill error : %x", ret);
	}
	ret = aacDecoder_DecodeFrame(aacdecoder_handler, (INT_PCM *)decode_buffer, pcm_pkt_size, 0);
	if (ret != AAC_DEC_OK) {
		LOGE("aacDecoder_DecodeFrame error : 0x%x", ret);
	}
}

static int parse_hw_addr(std::string str, std::vector<char> &hw_addr) {
    for (int i = 0; i < str.length(); i += 3) {
        hw_addr.push_back((char) stol(str.substr(i), NULL, 16));
    }
    return 0;
}

std::string find_mac() {
    std::ifstream iface_stream("/sys/class/net/eth0/address");
    if (!iface_stream) {
        iface_stream.open("/sys/class/net/wlan0/address");
    }
    if (!iface_stream) return "";

    std::string mac_address;
    iface_stream >> mac_address;
    iface_stream.close();
    return mac_address;
}

FILE *f1;
//FILE *f2;

int main(int argc, char *argv[]) {
    bool isDebug = argc > 1 && strcmp(argv[1], "debug") == 0;
    if (!isDebug) {
	SetErrorMode(SEM_FAILCRITICALERRORS);
	freopen("NUL", "w", stderr);
    }
    f1 = fopen("D:\\airplay.aac", "wb");
    //f2 = fopen("D:\\airplay.264", "wb");

    aacdecoder_handler = create_fdk_aac_decoder();
    ipc_client_create(&ipc_client);

    std::string server_name = DEFAULT_NAME;
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
    fclose(f1);
    //fclose(f2);
}

// Server callbacks
extern "C" void conn_init(void *cls) {
    send_status(ipc_client, MIRROR_START);
}

extern "C" void conn_destroy(void *cls) {
    send_status(ipc_client, MIRROR_STOP);
}

extern "C" void audio_process(void *cls, raop_ntp_t *ntp, aac_decode_struct *data) {
    //fwrite(data->data, 1, data->data_len, f1);
    do_fdk_aac_decode(data);
    fwrite(decode_buffer, 1, 1920, f1);
}

extern "C" void video_process(void *cls, raop_ntp_t *ntp, h264_decode_struct *data) {
    //fwrite(data->data, 1, data->data_len, f2);
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
