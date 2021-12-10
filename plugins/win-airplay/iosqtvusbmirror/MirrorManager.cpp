#include "MirrorManager.h"

MirrorManager::MirrorManager()
{
	usb_init();
}

MirrorManager::~MirrorManager() {}



usb_dev_handle *MirrorManager::openUsbDevice(int vid, int pid,  struct usb_device **device)
{
	usb_find_busses();
	usb_find_devices();

	struct usb_bus *bus;
	struct usb_device *dev;

	for (bus = usb_get_busses(); bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor != vid || dev->descriptor.idProduct != pid) {
				continue;
			}
			*device = dev;
			return usb_open(dev);
		}
	}
	return nullptr;
}

int MirrorManager::checkAndChangeMode(int vid, int pid)
{
	int ret = -1;
	struct usb_device *device = nullptr;
	auto deviceHandle = openUsbDevice(vid, pid, &device);
	if (!deviceHandle)
		return ret;

	int muxConfigIndex = -1;
	int qtConfigIndex = -1;
	findConfigurations(&device->descriptor, device, &muxConfigIndex, &qtConfigIndex);
	if (qtConfigIndex == -1) {
		usb_control_msg(deviceHandle, 0x40, 0x52, 0x00, 0x02, NULL, 0, 5000 /* LIBUSB_DEFAULT_TIMEOUT */);
		ret = 0;
	}

	usb_close(deviceHandle);
	ret = 1;

	return ret;
}
