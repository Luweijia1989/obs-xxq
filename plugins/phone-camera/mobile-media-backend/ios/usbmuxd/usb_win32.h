#pragma once

#ifdef WIN32
#include <stdint.h>

void usb_win32_init();
int usb_win32_get_configuration(const char serial[], uint8_t *configuration);
void usb_win32_set_configuration(const char serial[], uint8_t configuration);
void usb_win32_activate_quicktime(const char serial[]);
void usb_win32_extra_cmd(const char serial[]);
#endif
