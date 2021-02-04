#pragma once
#include <obs-module.h>
#include <pthread.h>
#include "rtc-base.h"
#include <QJsonObject>
#include <obs.hpp>
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

	static void muteChanged(void *param, calldata_t *calldata);
	static void volumeChanged(void *param, calldata_t *calldata);
	static void settingChanged(void *param, calldata_t *calldata);
	void connectMicSignals(obs_source_t *source);

public:
	obs_output_t *m_output = nullptr;
	OBSSignal micVolumeSignal;
	OBSSignal micMuteSignal;
	OBSSignal micSettingSignal;
	OBSSignal channelChangeSignal;
	pthread_t stop_thread;
	bool stop_thread_active = false;
	RTCBase *m_rtcBase = nullptr;
};
