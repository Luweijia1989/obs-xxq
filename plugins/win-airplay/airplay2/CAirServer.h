#pragma once

#include "../common-define.h"
#include "CAirServerCallback.h"

class CAirServer {
public:
	CAirServer();
	~CAirServer();

	void outputStatus(bool isConnected, const char *remoteName="",
			  const char *remoteDeviceId="");
	void outputStatus(mirror_status s, const char *remoteName ="",
			  const char *remoteDeviceId="");
	void outputAudio(uint8_t *data, size_t data_len, uint64_t pts, int serial,
			 const char *remoteName, const char *remoteDeviceId);
	void outputVideo(uint8_t *data, size_t data_len, uint64_t pts,
			 const char *remoteName, const char *remoteDeviceId);
	void outputMediaInfo(media_info *info, const char *remoteName,
			     const char *remoteDeviceId);

public:
	bool start();
	void stop();

private:
	CAirServerCallback *m_pCallback;
	void *m_pServer;
	struct IPCClient *client = nullptr;
};
