#pragma once

#include <qobject.h>
#include <qpointer.h>

#include "media-task.h"
#include "../common.h"
#include "../ipc.h"

class QTcpSocket;
class MediaSource : public QObject {
	Q_OBJECT
public:
	MediaSource(QObject *parent = nullptr);
	~MediaSource();
	void connectToMediaTarget(QString name);
	void setCurrentDevice(PhoneType type, QString deviceId);

	Q_INVOKABLE void mediaTargetLost();

private:
	bool m_emitError = false;
	struct IPCClient *m_ipcClient = nullptr;
	QPointer<MediaTask> m_mediaTask = nullptr;
	PhoneType m_phoneType = PhoneType::None;
};
