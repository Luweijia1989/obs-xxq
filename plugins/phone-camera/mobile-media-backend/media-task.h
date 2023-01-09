#pragma once

#include <qobject.h>
#include <qbytearray.h>
#include <qtimer.h>
#include <qset.h>
#include "../common.h"

class MediaTask : public QObject {
	Q_OBJECT
public:
	MediaTask(PhoneType type, QObject *parent = nullptr);
	~MediaTask();

	void setExpectedDevice(QString device);

	virtual void startTask(QString path, uint32_t handle = 0);
	Q_INVOKABLE virtual void stopTask(bool finalStop);

	static void deviceData(char *buf, int size, void *cb_data);
	static void deviceLost(void *cb_data);

private:
	void deviceDataInternal(char *buf, int size);

public slots:
	void onScanTimerTimeout();

signals:
	void mediaData(char *data, int size, int64_t timestamp, bool isVideo);
	void mediaState(bool start);

public:
	PhoneType m_phoneType = PhoneType::None;
	QString m_expectedDevice;
	QString m_connectedDevice;
	QTimer m_scanTimer;
	bool m_taskStarted = false;

	QTimer m_usbmuxdConnectTimer;
	uint32_t m_iOSDeviceHandle = 0;
	int m_usbmuxdFD = -1;
	QString m_finalId;
	QByteArray m_mediaCache;
};
