#pragma once

struct device_socket_pair
{
	char serial[256];
	int fd;
};

void mirror_devices_init();
void mirror_devices_clear();
void mirror_devices_add(const char *udid, int fd);
void mirror_devices_remove(const char *udid);
int mirror_devices_fd_from_udid(const char *udid);
