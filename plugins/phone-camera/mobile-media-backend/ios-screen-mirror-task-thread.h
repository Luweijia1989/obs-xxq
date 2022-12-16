#pragma once

#include "ios-camera.h"
#include <qeventloop.h>
#include "../common.h"
#include <media-io/audio-io.h>
#include <qtcpsocket.h>


class ScreenMirrorInfo;
class iOSScreenMirrorTaskThread : public TaskThread {
	Q_OBJECT
public:
	iOSScreenMirrorTaskThread(QObject *parent = nullptr);

	virtual void stopTask() override;
	virtual void startTask(QString udid, uint32_t deviceHandle) override;

private:
	void sendCmd(bool isStart);

private:
	bool m_inMirror = false;
	QTcpSocket *m_mirrorSocket = nullptr;
	QEventLoop m_eventLoop;
};
