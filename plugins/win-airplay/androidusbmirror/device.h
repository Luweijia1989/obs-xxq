#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "common.h"
#include "util/net.h"

#define DEVICE_NAME_FIELD_LENGTH 64

// name must be at least DEVICE_NAME_FIELD_LENGTH bytes
bool device_read_info(socket_t device_socket, char *device_name,
		      struct size *size);

#ifdef __cplusplus
}
#endif

#endif
