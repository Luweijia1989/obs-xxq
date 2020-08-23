#include "CAirServer.h"
#include <cstdio>

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

void CAirServer::outputAudio(uint8_t *data, size_t data_len, uint64_t pts,
			     const char *remoteName, const char *remoteDeviceId)
{
	if (data_len <= 0)
		return;

	struct av_packet_info pack_info = {0};
	pack_info.size = data_len;
	pack_info.type = FFM_PACKET_AUDIO;
	pack_info.pts = pts;

	ipc_client_write(client, &pack_info, sizeof(struct av_packet_info),
			 INFINITE);
	ipc_client_write(client, data, data_len, INFINITE);
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

	ipc_client_write(client, &pack_info,
			      sizeof(struct av_packet_info), INFINITE);
	ipc_client_write(client, data, data_len, INFINITE);
}

void CAirServer::outputMediaInfo(media_info *info,const char *remoteName,
				 const char *remoteDeviceId)
{
	struct av_packet_info pack_info = {0};
	pack_info.size = sizeof(struct media_info);
	pack_info.type = FFM_MEDIA_INFO;

	ipc_client_write(client, &pack_info,
			      sizeof(struct av_packet_info), INFINITE);
	ipc_client_write(client, info, sizeof(struct media_info), INFINITE);
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
