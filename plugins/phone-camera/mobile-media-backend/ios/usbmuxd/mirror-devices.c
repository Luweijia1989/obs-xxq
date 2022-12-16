#include "mirror-devices.h"
#include "utils.h"
#include <stdlib.h>
#include <memory.h>
#include <string.h>

static struct collection mirror_devices;

void mirror_devices_init()
{
	collection_init(&mirror_devices);
}

void mirror_devices_clear()
{
	FOREACH(struct device_socket_pair *pair, &mirror_devices) {
		collection_remove(&mirror_devices, pair);
		free(pair);
	} ENDFOREACH

	collection_free(&mirror_devices);
}

void mirror_devices_add(const char *udid, int fd)
{
	struct device_socket_pair *pair = malloc(sizeof(struct device_socket_pair));
	memset(pair, 0, sizeof(struct device_socket_pair));
	memcpy(pair->serial, udid, strlen(udid));
	pair->fd = fd;
	collection_add(&mirror_devices, pair);
}

void mirror_devices_remove(const char *udid)
{
	struct device_socket_pair *ret = NULL;
	FOREACH(struct device_socket_pair *pair, &mirror_devices) {
		if (strcmp(udid, pair->serial) == 0) {
			ret = pair;
			break;
		}
	} ENDFOREACH

	if (ret) {
		collection_remove(&mirror_devices, ret);
		free(ret);
	}
}

int mirror_devices_fd_from_udid(const char *udid)
{
	int ret = -1;
	FOREACH(struct device_socket_pair *pair, &mirror_devices) {
		if (strcmp(udid, pair->serial) == 0) {
			ret = pair->fd;
			break;
		}
	} ENDFOREACH

	return ret;
}
