#include "CAirServer.h"
#include <cstdio>

CAirServer::CAirServer() : m_pCallback(NULL), m_pServer(NULL)
{
	m_pCallback = new CAirServerCallback();
	memset(&ipc_client, 0, sizeof(ipc_pipe_client_t));
	if (!ipc_pipe_client_open(&ipc_client, PIPE_NAME)) {
	}
}

CAirServer::~CAirServer()
{
	ipc_pipe_client_free(&ipc_client);
	delete m_pCallback;
	stop();
}

void CAirServer::outputAudio(uint8_t *data, size_t data_len, uint64_t pts,
			     const char *remoteName, const char *remoteDeviceId)
{
	if (data_len <= 0)
		return;

	struct av_packet_info pack_info = {0};
	pack_info.size = data_len;
	pack_info.type = FFM_PACKET_AUDIO;
	pack_info.pts = pts;

	ipc_pipe_client_write(&ipc_client, &pack_info,
			      sizeof(struct av_packet_info));
	ipc_pipe_client_write(&ipc_client, data, data_len);
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

	ipc_pipe_client_write(&ipc_client, &pack_info,
			      sizeof(struct av_packet_info));
	ipc_pipe_client_write(&ipc_client, data, data_len);
}

void CAirServer::outputMediaInfo(media_info *info)
{
	struct av_packet_info pack_info = {0};
	pack_info.size = sizeof(struct media_info);
	pack_info.type = FFM_MEDIA_INFO;

	ipc_pipe_client_write(&ipc_client, &pack_info,
			      sizeof(struct av_packet_info));
	ipc_pipe_client_write(&ipc_client, info, sizeof(struct media_info));
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
	sprintf_s(serverName, 1024, "YuerLive[%s]", hostName);
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
