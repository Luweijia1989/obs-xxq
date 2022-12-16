#include "qt_configuration.h"

int findInterfaceForSubclass(struct usb_config_descriptor *descriptor, int classType)
{
	int ret = -1;
	for (int n = 0; n < descriptor->bNumInterfaces; n++)
	{
		if (descriptor->interface[n].altsetting[0].bInterfaceClass == INTERFACE_CLASS && descriptor->interface[n].altsetting[0].bInterfaceSubClass == classType) {
			ret = 0;
			break;
		} else
			ret = -1;
	}

	return ret;
}

static int isQtConfig(struct usb_config_descriptor *descriptor)
{
	return findInterfaceForSubclass(descriptor, INTERFACE_QUICKTIMECLASS);
}

static int isMuxConfig(struct usb_config_descriptor *descriptor)
{
	return findInterfaceForSubclass(descriptor, INTERFACE_SUBCLASS);
}

void findConfigurations(struct usb_device_descriptor *desc, struct usb_device *device, int *qtConfigIndex)
{
	*qtConfigIndex = -1;

	for (int m = 0; m < desc->bNumConfigurations; m++)
	{
		struct usb_config_descriptor config = device->config[m];
		if (isQtConfig(&config) == 0) {
			*qtConfigIndex = config.bConfigurationValue;
		}
	}
}
