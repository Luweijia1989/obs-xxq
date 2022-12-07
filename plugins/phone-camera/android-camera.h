#pragma once

#include <qtimer.h>
#include <thread>
#include <libusb-1.0/libusb.h>
#include "usb-helper.h"
#include <util/circlebuf.h>
#include "media-task.h"

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

class AndroidCamera : public MediaTask {
	Q_OBJECT
public:
	AndroidCamera(QObject *parent = nullptr);
	~AndroidCamera();
	bool setExpectedDevice(QString devicePath) override;

	void startTask(QString path) override;
	void stopTask() override;

	static void run(void *p);

private:
	bool setupDevice(libusb_device *device, libusb_device_handle *handle);
	int setupDroid(libusb_device *usbDevice, libusb_device_handle *handle);
	bool handleMediaData(circlebuf *buffer, uint8_t **cacheBuffer, size_t *cacheBufferSize);
	void taskInternal();

private:
	QString m_connectedPath;
	bool m_running = false;
	t_accessory_droid m_droid = {0};
	std::thread m_taskTh;
	QTimer m_scanTimer;
};
