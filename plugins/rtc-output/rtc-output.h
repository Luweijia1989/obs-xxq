#pragma once
#include <obs-module.h>
#include "rtc-base.h"
class RTCOutput {
public:
	enum RTC_TYPE
	{
		RTC_TYPE_TRTC,
		RTC_TYPE_QINIU,
	};
	RTCOutput(RTC_TYPE type, obs_output_t *output);
	~RTCOutput();
public:
	obs_output_t *m_output = nullptr;
	pthread_t stop_thread;
	bool stop_thread_active = false;
	RTCBase *m_rtcBase = nullptr;
};
