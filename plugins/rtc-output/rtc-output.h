#pragma once
#include <obs-module.h>

class RTCOutput {
public:
public:
	obs_output_t *m_output = nullptr;
	pthread_t stop_thread;
	bool stop_thread_active = false;
};
