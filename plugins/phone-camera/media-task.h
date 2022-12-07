#pragma once

#include <qobject.h>

class MediaTask : public QObject {
	Q_OBJECT
public:
	MediaTask(QObject *parent = nullptr) : QObject(parent) {}
	virtual void startTask(QString device = QString()) {}
	Q_INVOKABLE virtual void stopTask() {}
	virtual bool setExpectedDevice(QString device)
	{
		do {
			if (m_expectedDevice.isEmpty())
				break;

			if (device == "auto")
				return false;
			else {
				if (m_expectedDevice == "auto") //todo check auto device is same with target device
					break;
				else {
					if (m_expectedDevice == device)
						return false;
					else
						break;
				}
			}
		} while (1);

		return true;
	}
signals:
	void mediaData(uint8_t *data, size_t size, int64_t timestamp, bool isVideo);
	void mediaFinish();

public:
	QString m_expectedDevice;
};
