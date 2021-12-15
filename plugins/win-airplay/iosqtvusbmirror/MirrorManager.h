#pragma once

#include <stdint.h>
extern "C"
{
#include "qt_configuration.h"
}

class MirrorManager
{
public:
	MirrorManager();
	~MirrorManager();

	int startMirrorTask(int vid, int pid);

private:
	int checkAndChangeMode(int vid, int pid);

private:
	uint8_t m_interface, ep_in, ep_out;
	uint8_t m_interface_fa, ep_in_fa, ep_out_fa;
	struct usb_dev_handle *m_deviceHandle = nullptr;
};
