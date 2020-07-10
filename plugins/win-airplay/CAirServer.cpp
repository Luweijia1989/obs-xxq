#include "CAirServer.h"
#include "CAirServerCallback.h"
#include <cstdio>

CAirServer::CAirServer() : m_pCallback(NULL), m_pServer(NULL)
{
	m_pCallback = new CAirServerCallback();
}

CAirServer::~CAirServer()
{
	delete m_pCallback;
	stop();
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

bool CAirServer::start(AirPlayServer *s)
{
	stop();
	m_pCallback->setAirplayServer(s);
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

float CAirServer::setVideoScale(float fRatio)
{
	return fgServerScale(m_pServer, fRatio);
}
