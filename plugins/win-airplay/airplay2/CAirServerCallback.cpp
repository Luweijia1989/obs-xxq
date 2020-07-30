#include "CAirServerCallback.h"
#include <stdio.h>
#include "FgUtf8Utils.h"
#include <locale.h>
#include "CAirServer.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))

CAirServerCallback::CAirServerCallback()
{
	memset(m_chRemoteDeviceId, 0, 128);
}

CAirServerCallback::~CAirServerCallback() {}

void CAirServerCallback::setAirplayServer(CAirServer *s)
{
	m_airplayServer = s;
}

void CAirServerCallback::connected(const char *remoteName,
				   const char *remoteDeviceId)
{
	if (remoteDeviceId != NULL) {
		strncpy(m_chRemoteDeviceId, remoteDeviceId, 128);
	}

	setlocale(LC_CTYPE, "");
	std::wstring name = CFgUtf8Utils::UTF8_To_UTF16(remoteName);
	wprintf(L"Client Name: %s\n", name.c_str());
}

void CAirServerCallback::disconnected(const char *remoteName,
				      const char *remoteDeviceId)
{
	memset(m_chRemoteDeviceId, 0, 128);
}

void CAirServerCallback::outputAudio(uint8_t *data, size_t data_len,
				     uint64_t pts, const char *remoteName,
				     const char *remoteDeviceId)
{
	if (m_airplayServer) {
		if (m_chRemoteDeviceId[0] == '\0' && remoteDeviceId != NULL) {
			strncpy(m_chRemoteDeviceId, remoteDeviceId, 128);
		}
		if (0 != strcmp(m_chRemoteDeviceId, remoteDeviceId)) {
			return;
		}
		m_airplayServer->outputAudio(data, data_len, pts, remoteName,
					     remoteDeviceId);
	}
}

void CAirServerCallback::outputVideo(uint8_t *data, size_t data_len,
				     uint64_t pts, const char *remoteName,
				     const char *remoteDeviceId)
{
	if (m_airplayServer) {
		if (m_chRemoteDeviceId[0] == '\0' && remoteDeviceId != NULL) {
			strncpy(m_chRemoteDeviceId, remoteDeviceId, 128);
		}
		if (0 != strcmp(m_chRemoteDeviceId, remoteDeviceId)) {
			return;
		}
		m_airplayServer->outputVideo(data, data_len, pts, remoteName,
					     remoteDeviceId);
	}
}

void CAirServerCallback::outputMediaInfo(media_info *info) {}

void CAirServerCallback::videoPlay(char *url, double volume, double startPos)
{
	printf("Play: %s", url);
}

double dbDuration = 10000;
double dbPosition = 0;
void CAirServerCallback::videoGetPlayInfo(double *duration, double *position,
					  double *rate)
{
	*duration = 1000;
	*position = dbPosition;
	*rate = 1.0;
	dbPosition += 1;
}

void CAirServerCallback::log(int level, const char *msg)
{
#ifdef _DEBUG
	OutputDebugStringA(msg);
	OutputDebugStringA("\n");
#else
	printf("%s\n", msg);
#endif
}
