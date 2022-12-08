#pragma once

#include <qobject.h>
#include <qbytearray.h>
#include <qtimer.h>
#include <qset.h>

extern QSet<QString> runningDevices;
class MediaTask : public QObject {
	Q_OBJECT
public:
	MediaTask(QObject *parent = nullptr) : QObject(parent)
	{
		m_scanTimer.setInterval(100);
		m_scanTimer.setSingleShot(false);
	}

	void setExpectedDevice(QString device)
	{
		do {
			if (m_expectedDevice.isEmpty())
				break;

			if (device == "auto")
				return;
			else {
				if (m_expectedDevice == "auto") //todo check auto device is same with target device
					break;
				else {
					if (m_expectedDevice == device)
						return;
					else
						break;
				}
			}
		} while (1);

		stopTask();
		m_expectedDevice = device;
		m_scanTimer.start();
	}

	virtual void startTask(QString path, uint32_t handle = 0)
	{
		Q_UNUSED(handle)

		m_scanTimer.stop();
		m_connectedDevice = path;
		runningDevices.insert(path);
	}

	Q_INVOKABLE virtual void stopTask()
	{
		runningDevices.remove(m_connectedDevice);
		m_connectedDevice = QString();
		m_scanTimer.start();
	}
signals:
	void mediaData(QByteArray data, int64_t timestamp, bool isVideo);
	void mediaFinish();

public:
	QString m_expectedDevice;
	QString m_connectedDevice;
	QTimer m_scanTimer;
};
