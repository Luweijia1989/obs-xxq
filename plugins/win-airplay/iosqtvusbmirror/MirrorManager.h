#pragma once

extern "C"
{
#include "qt_configuration.h"
}

class MirrorManager
{
public:
	MirrorManager();
	~MirrorManager();

	int checkAndChangeMode(int vid, int pid);

private:
	struct usb_dev_handle *openUsbDevice(int vid, int pid, struct usb_device **device);
};
