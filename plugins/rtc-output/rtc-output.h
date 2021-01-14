#pragma once
#include <obs-module.h>
#include <pthread.h>
#include "rtc-base.h"
#include <QJsonObject>
class RTCOutput {
public:
	enum RTC_TYPE
	{
		RTC_TYPE_TRTC,
		RTC_TYPE_QINIU,
	};
	RTCOutput(RTC_TYPE type, obs_output_t *output);
	~RTCOutput();

	void sigEvent(int type, QJsonObject data);
public:
	obs_output_t *m_output = nullptr;
	pthread_t stop_thread;
	bool stop_thread_active = false;
	RTCBase *m_rtcBase = nullptr;
};
