#include "CAirServer.h"
#include <cstdio>
#include "../common-define.h"

CAirServer::CAirServer() : m_pCallback(NULL), m_pServer(NULL)
{
	m_pCallback = new CAirServerCallback();
	ipc_client_create(&client);
}

CAirServer::~CAirServer()
{
	ipc_client_destroy(&client);
	delete m_pCallback;
	stop();
}

void CAirServer::outputStatus(bool isConnected, const char *remoteName,
			      const char *remoteDeviceId)
{
	outputStatus(isConnected ? MIRROR_START : MIRROR_STOP, remoteName,
		     remoteDeviceId);
}

void CAirServer::outputStatus(mirror_status s, const char *remoteName,
			      const char *remoteDeviceId)
{
	send_status(client, s);
}

void CAirServer::outputAudio(uint8_t *data, size_t data_len, uint64_t pts,
			     int serial, const char *remoteName,
			     const char *remoteDeviceId)
{
	if (data_len <= 0)
		return;

	struct av_packet_info pack_info = {0};
	pack_info.size = data_len;
	pack_info.type = FFM_PACKET_AUDIO;
	pack_info.pts = pts;
	pack_info.serial = serial;

	ipc_client_write_2(client, &pack_info, sizeof(struct av_packet_info), data, data_len, INFINITE);
}

void CAirServer::outputVideo(uint8_t *data, size_t data_len, uint64_t pts,
			     const char *remoteName, const char *remoteDeviceId)
{
	if (data_len <= 0)
		return;

	struct av_packet_info pack_info = {0};
	pack_info.size = data_len;
	pack_info.type = FFM_PACKET_VIDEO;
	pack_info.pts = pts;

	ipc_client_write_2(client, &pack_info, sizeof(struct av_packet_info), data, data_len, INFINITE);
}

void CAirServer::outputMediaInfo(media_video_info *info, const char *remoteName,
				 const char *remoteDeviceId)
{
	struct av_packet_info pack_info = {0};
	pack_info.size = sizeof(struct media_video_info);
	pack_info.type = FFM_MEDIA_VIDEO_INFO;
	ipc_client_write_2(client, &pack_info, sizeof(struct av_packet_info), info, sizeof(struct media_video_info), INFINITE);

	media_audio_info audio_info;
	audio_info.samples_per_sec = 44100;
	audio_info.format = AUDIO_FORMAT_16BIT;
	audio_info.speakers = SPEAKERS_STEREO;
	struct av_packet_info audio_pack_info = {0};
	audio_pack_info.size = sizeof(struct media_audio_info);
	audio_pack_info.type = FFM_MEDIA_AUDIO_INFO;
	ipc_client_write_2(client, &audio_pack_info, sizeof(struct av_packet_info), &audio_info, sizeof(struct media_audio_info), INFINITE);
}

bool getHostName(char hostName[512])
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	gethostname(hostName, 511);
	if (strlen(hostName) > 0) {
		return true;
	} else {
		DWORD n = 511;

		if (::GetComputerNameA(hostName, &n)) {
			if (n > 2) {
				hostName[n] = '\0';
			}
		}
	}
}

bool CAirServer::start()
{
	stop();
	m_pCallback->setAirplayServer(this);
	char hostName[512];
	memset(hostName, 0, sizeof(hostName));
	getHostName(hostName);
	char serverName[1024] = {0};
	sprintf_s(serverName, 1024, "yuerzhibo[%s]", hostName);
	m_pServer = fgServerStart(serverName, 5001, 7001, m_pCallback);
	return m_pServer != NULL;
}

void CAirServer::stop()
{
	if (m_pServer != NULL) {
		fgServerStop(m_pServer);
		m_pServer = NULL;
	}
}
