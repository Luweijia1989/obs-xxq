#pragma once

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC void usb_device_reset(const char *serial);
EXTERNC void add_usb_device_change_event();
EXTERNC bool consume_usb_device_change_event();
