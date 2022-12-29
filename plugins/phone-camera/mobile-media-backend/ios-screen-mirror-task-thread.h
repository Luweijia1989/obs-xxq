#pragma once

#include "ios-camera.h"
#include <media-io/audio-io.h>
#include <qtcpsocket.h>
#include <qpointer.h>
#include "../common.h"
#include "../xxq-tcp.h"

class ScreenMirrorInfo;
class iOSScreenMirrorTaskThread : public iOSTask {
	Q_OBJECT
public:
	iOSScreenMirrorTaskThread(QObject *parent = nullptr);

	virtual void stopTask() override;
	virtual void startTask(QString udid, uint32_t deviceHandle) override;

private:
	void sendCmd(bool isStart);

private:
	TcpClient *m_mirrorSocket = nullptr;
	QByteArray m_mediaCache;
};
