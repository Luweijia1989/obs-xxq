#pragma once

#include <qobject.h>
#include <qbytearray.h>
#include <qtimer.h>
#include <qset.h>
#include <plist/plist.h>
#include "ios/usbmuxd/usbmuxd-proto.h"

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

		stopTask(true);
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

	Q_INVOKABLE virtual void stopTask(bool finalStop)
	{
		runningDevices.remove(m_connectedDevice);
		m_connectedDevice = QString();
		if (!finalStop)
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

static inline QByteArray usbmuxdTaskCMD(QString deviceId, bool isStart)
{
	QByteArray result;
	plist_t dict = plist_new_dict();
	plist_dict_set_item(dict, "CmdType", plist_new_string(isStart ? "Start" : "Stop"));
	plist_dict_set_item(dict, "DeviceId", plist_new_string(deviceId.toUtf8().data()));

	char *xml = NULL;
	uint32_t xmlsize = 0;
	plist_to_xml(dict, &xml, &xmlsize);
	if (xml) {
		struct usbmuxd_header hdr;
		auto size = sizeof(hdr) + xmlsize;
		hdr.version = 1;
		hdr.length = size;
		hdr.message = MESSAGE_CUSTOM;
		hdr.tag = 1;
		result.resize(size);
		memcpy(result.data(), &hdr, sizeof(hdr));
		memcpy(result.data() + sizeof(hdr), xml, xmlsize);
		free(xml);
	}
	plist_free(dict);

	return result;
}
