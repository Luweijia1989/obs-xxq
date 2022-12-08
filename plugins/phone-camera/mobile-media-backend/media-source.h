#pragma once

#include <qobject.h>
#include <qpointer.h>

#include "media-task.h"
#include "../common.h"

class QTcpSocket;
class MediaSource : public QObject {
	Q_OBJECT
public:
	MediaSource(QObject *parent = nullptr);
	~MediaSource();
	void connectToHost(int port);
	void setCurrentDevice(PhoneType type, QString deviceId);

private:
	QTcpSocket *m_socket = nullptr;
	QPointer<MediaTask> m_mediaTask = nullptr;
	PhoneType m_phoneType = PhoneType::None;
};
