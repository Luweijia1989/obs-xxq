#pragma once

#include <lusb0_usb.h>

#define INTERFACE_CLASS 255
#define INTERFACE_SUBCLASS 254
#define INTERFACE_QUICKTIMECLASS 42
#define INTERFACE_PROTOCOL 2

int findInterfaceForSubclass(struct usb_config_descriptor *descriptor, int classType);
int isQtConfig(struct usb_config_descriptor *descriptor);
int isMuxConfig(struct usb_config_descriptor *descriptor);
void findConfigurations(struct usb_device_descriptor *desc, struct usb_device *device, int *qtConfigIndex);
