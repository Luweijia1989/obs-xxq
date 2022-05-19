#include "server.h"
#include "device.h"
#include "stream.h"
#include <fcntl.h>
#include <io.h>
#include <util/platform.h>
#include "common-define.h"
#include <QCoreApplication>
#include <QTimer>

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
	p.lock_video_orientation = -1;
	p.max_size = 1280;

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

int main(int argv, char *argc[])
{
	freopen("NUL", "w", stderr);

	QCoreApplication app(argv, argc);

	struct IPCClient *client = NULL;
	ipc_client_create(&client);
	send_status(client, MIRROR_STOP);

	net_init();

	MirrorRPC rpc;
	QTimer t;

	QObject::connect(&rpc, &MirrorRPC::quit, [&t, &app](){
		t.stop();
		app.quit();
	});
	QObject::connect(&t, &QTimer::timeout, &t, [=](){
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
	});
	t.start(200);

	int ret = app.exec();
	
	if (is_server_running())
		server_clear();

	ipc_client_destroy(&client);
	os_kill_process("adb.exe");
	net_cleanup();
	return ret;
}
