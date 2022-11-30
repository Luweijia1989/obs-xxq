#pragma once

#include <qthread.h>
#include <qtimer.h>
#include <qmap>

class iOSCamera : public QObject{
	Q_OBJECT
public:
	iOSCamera(QObject *parent = nullptr);
	~iOSCamera();
	void start();

private:
	QString getDeviceName(QString udid);

signals:
	void updateDeviceList(QMap<QString, QString> devices);

private:
	QThread *m_updateDeviceThread = nullptr;
	QTimer *m_updateTimer = nullptr;
	QMap<QString, uint32_t> m_devices;
};
