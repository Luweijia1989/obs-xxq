#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "config.h"
#include "util/net.h"
#include "ipc.h"

struct video_buffer;

struct stream {
	socket_t socket;
	pthread_t thread;
	struct IPCClient *ipc_client;
	bool stop;
};

void stream_init(struct stream *stream, socket_t socket,
		 struct IPCClient *client);

bool stream_start(struct stream *stream);

void stream_stop(struct stream *stream);

void stream_join(struct stream *stream);

#ifdef __cplusplus
}
#endif

#endif
