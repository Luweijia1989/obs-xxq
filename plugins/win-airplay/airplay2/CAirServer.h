#pragma once

#include <ipc-util/pipe.h>
#include "CAirServerCallback.h"

class CAirServer {
public:
	CAirServer();
	~CAirServer();

	void outputAudio(uint8_t *data, size_t data_len, uint64_t pts,
			 const char *remoteName, const char *remoteDeviceId);
	void outputVideo(uint8_t *data, size_t data_len, uint64_t pts,
			 const char *remoteName, const char *remoteDeviceId);
	void outputMediaInfo(media_info *info);

public:
	bool start();
	void stop();

private:
	CAirServerCallback *m_pCallback;
	void *m_pServer;
	ipc_pipe_client_t ipc_client;
};
