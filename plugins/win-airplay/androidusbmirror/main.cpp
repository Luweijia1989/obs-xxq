#include "server.h"
#include "device.h"
#include "stream.h"
#include <fcntl.h>
#include <io.h>
#include "../common-define.h"

struct server g_s;
struct stream g_stream;
bool g_exit = false;

static char *android_device()
{
	char *ret = NULL;
	int count = 0;
	char **devices;
	adb_devices(&count, &devices);

	if (count > 0) {
		ret = strdup(devices[0]);
	}

	for (int i = 0; i < count; i++) {
		free(devices[i]);
	}
	free(devices);

	return ret;
}

static bool is_server_running()
{
	return g_s.serial && g_s.video_socket;
}

static void server_clear()
{
	stream_stop(&g_stream);

	server_stop(&g_s);
	server_destroy(&g_s);
	memset(&g_s, 0, sizeof(struct server));
}

static bool server_init(char *device)
{
	server_init(&g_s);
	struct server_params p;
	memset(&p, 0, sizeof(struct server_params));
	p.bit_rate = 8000000;
	p.port_range.first = 2543;
	p.port_range.last = 2599;
	p.max_fps = 60;
	p.lock_video_orientation = 1;

	if (!server_start(&g_s, device, &p))
		goto err;

	if (!server_connect_to(&g_s))
		goto err;

	char device_name[DEVICE_NAME_FIELD_LENGTH];
	struct size frame_size;
	if (!device_read_info(g_s.video_socket, device_name, &frame_size)) {
		goto err;
	}

	stream_init(&g_stream, g_s.video_socket, g_s.ipc_client);
	if (!stream_start(&g_stream))
		goto err;

	return true;
err:
	server_clear();
	return false;
}

void *stdin_read_thread(void *data)
{
	uint8_t buf[1024] = {0};
	while (true) {
		int read_len =
			fread(buf, 1, 1024,
			      stdin); // read 0 means parent has been stopped
		if (read_len) {
			if (buf[0] == 1) {
				g_exit = true;
				break;
			}
		} else {
			g_exit = true;
			break;
		}
	}
	return NULL;
}

static pthread_t create_stdin_thread()
{
	pthread_t th;
	pthread_create(&th, NULL, stdin_read_thread, NULL);
	return th;
}

int main(int argv, char *argc[])
{
	SetErrorMode(SEM_FAILCRITICALERRORS);
	_setmode(_fileno(stdin), O_BINARY);
	freopen("NUL", "w", stderr);

	struct IPCClient *client = NULL;
	ipc_client_create(&client);
	pthread_t stdin_th = create_stdin_thread();

	net_init();
	while (!g_exit) {
		Sleep(1000);
		char *device = android_device();
		if (device) {
			if (!is_server_running()) {
				send_status(client, MIRROR_START);
				g_s.ipc_client = client;
				server_init(device);
			}

			free(device);
		} else {
			if (is_server_running()) {
				send_status(client, MIRROR_STOP);
				server_clear();
			}
		}
	}
	pthread_join(stdin_th, NULL);
	net_cleanup();
	ipc_client_destroy(&client);
	return 0;
}
