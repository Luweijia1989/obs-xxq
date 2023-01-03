#pragma once

#include "ios-camera.h"
#include <media-io/audio-io.h>
#include <qtcpsocket.h>
#include <qpointer.h>
#include "../common.h"

class ScreenMirrorInfo;
class iOSScreenMirrorTaskThread : public iOSTask {
	Q_OBJECT
public:
	iOSScreenMirrorTaskThread(QObject *parent = nullptr);

	virtual void stopTask() override;
	virtual void startTask(QString udid, uint32_t deviceHandle) override;

	static void mirrorData(char *buf, int size, void *cb_data);
	static void deviceLost(void *cb_data);

private:
	void mirrorDataInternal(char *buf, int size);

private:
	QByteArray m_mediaCache;
};
