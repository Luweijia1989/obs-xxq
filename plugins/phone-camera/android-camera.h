#pragma once

#include <qthread.h>
#include <libusb-1.0/libusb.h>
#include "usb-helper.h"
#include <util/circlebuf.h>

typedef struct t_accessory_droid {
	libusb_device_handle *usbHandle = nullptr;
	unsigned char inendp;
	unsigned char outendp;
	unsigned char audioendp;

	int inpacketsize;
	int outpacketsize;
	int audiopacketsize;

	unsigned char bulkInterface;
	unsigned char audioInterface;
	unsigned char audioAlternateSetting;

	bool connected = false;
	uint16_t c_vid;
	uint16_t c_pid;
} accessory_droid;

class AndroidCamera : public QThread {
	Q_OBJECT
public:
	AndroidCamera(QObject *parent = nullptr);
	~AndroidCamera();
	void setCurrentDevice(QString devicePath);

	void startTask(QString path);
	void stopTask();

signals:
	void mediaData(uint8_t *data, size_t size, int64_t timestamp, bool isVideo);
	void mediaFinish();

private:
	bool setupDevice(libusb_device *device, libusb_device_handle *handle);
	int setupDroid(libusb_device *usbDevice, libusb_device_handle *handle);
	bool handleMediaData(circlebuf *buffer);

protected:
	void run() override;

private:
	QString m_devicePath;
	bool m_running = false;
	t_accessory_droid m_droid = {0};

	uint8_t *m_cacheBuffer = nullptr;
	size_t m_cacheBufferSize = 0;
};
