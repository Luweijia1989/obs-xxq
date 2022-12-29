#pragma once

#include "media-task.h"
#include "../xxq-tcp.h"

class AndroidCamera : public MediaTask {
	Q_OBJECT
public:
	AndroidCamera(QObject *parent = nullptr);
	~AndroidCamera();

	void startTask(QString path, uint32_t handle = 0) override;
	void stopTask(bool finalStop) override;

private:
	TcpClient *m_mirrorSocket = nullptr;
	QByteArray m_mediaCache;
};
