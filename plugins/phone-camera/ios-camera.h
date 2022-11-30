#pragma once

#include <qthread.h>
#include <qtimer.h>
#include <qmap>
#include <util/circlebuf.h>

class iOSCameraTaskThread : public QThread {
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
	void run() override;
	void startByInfo(QString udid, uint32_t deviceHandle);
	void stopTask();

private:
	void parseMediaData();

signals:
	void connectResult(QString udid, bool success);
	void mediaData(uint8_t *data, size_t size, bool isVideo);
	void mediaFinish();

private:
	bool m_running = false;
	uint32_t m_deviceHandle = 0;
	QString m_udid;
	circlebuf m_dataBuf;
};

class iOSCamera : public QObject {
	Q_OBJECT
public:
	enum State {
		UnConnected,
		Connecting,
		Connected,
	};

	iOSCamera(QObject *parent = nullptr);
	~iOSCamera();
	void start();
	void stop();
	void setCurrentDevice(QString udid);

private:
	QString getDeviceName(QString udid);

public slots:
	void onUpdateDeviceList(QMap<QString, QPair<QString, uint32_t>> devices);

signals:
	void updateDeviceList(QMap<QString, QPair<QString, uint32_t>> devices);
	void mediaData(uint8_t *data, size_t size, bool isVideo);
	void mediaFinish();

private:
	QThread *m_updateDeviceThread = nullptr;
	QTimer *m_updateTimer = nullptr;
	QString m_currentDevice;
	QString m_connectedDevice;
	State m_state = UnConnected;
	iOSCameraTaskThread *m_taskThread = nullptr;
};
