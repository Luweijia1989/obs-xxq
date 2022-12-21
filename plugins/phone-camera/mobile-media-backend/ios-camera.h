#pragma once

#include <qthread.h>
#include <qmap>
#include <util/circlebuf.h>
#include "media-task.h"

class iOSTask : public QObject {
	Q_OBJECT
public:
	iOSTask(QObject *parent = nullptr) : QObject(parent) {}
	virtual ~iOSTask() {}
	virtual void startTask(QString udid, uint32_t deviceHandle)
	{
		m_deviceHandle = deviceHandle;
		m_udid = udid;
	}

	virtual void stopTask()
	{
	}

signals:
	void mediaData(QByteArray data, int64_t timestamp, bool isVideo);
	void mediaFinish();
	void finished();

public:
	uint32_t m_deviceHandle = 0;
	QString m_udid;
};

class iOSCameraTaskThread : public iOSTask {
	Q_OBJECT
public:
	// This is what we send as the header for each frame.
	typedef struct _PortalFrame {
		// The version of the frame and protocol.
		uint32_t version;

		// Type of frame
		uint32_t type;

		// Unless zero, a tag is retained in frames that are responses to previous
		// frames. Applications can use this to build transactions or request-response
		// logic.
		uint32_t tag;

		// If payloadSize is larger than zero, *payloadSize* number of bytes are
		// following, constituting application-specific data.
		uint32_t payloadSize;

	} PortalFrame;

	iOSCameraTaskThread(QObject *parent = nullptr);

	void startTask(QString udid, uint32_t deviceHandle) override;
	void stopTask() override;

	static void run(void *p);

private:
	void parseMediaData();
	void taskInternal();

private:
	circlebuf m_dataBuf;
	bool m_running = false;
	std::thread m_taskTh;
};

class iOSCamera : public MediaTask {
	Q_OBJECT
public:
	iOSCamera(QObject *parent = nullptr);
	~iOSCamera();
	void startTask(QString device, uint32_t handle = 0) override;
	void stopTask(bool finalStop) override;

public slots:
	void onUpdateDeviceList();

private:
	iOSTask *m_taskThread = nullptr;
};
