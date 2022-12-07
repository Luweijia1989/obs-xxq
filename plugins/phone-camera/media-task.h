#pragma once

#include <qobject.h>

class MediaTask : public QObject {
	Q_OBJECT
public:
	MediaTask(QObject *parent = nullptr) : QObject(parent) {}
	virtual void startTask() {}
	virtual void stopTask() {}
	virtual bool setCurrentDevice(QString device)
	{
		do {
			if (m_currentDevice.isEmpty())
				break;

			if (device == "auto")
				return false;
			else {
				if (m_currentDevice == "auto")
					break;
				else {
					if (m_currentDevice == device)
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
	QString m_currentDevice;
};
