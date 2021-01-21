#pragma once

enum RTC_EVENT_TYPE {
	RTC_EVENT_SUCCESS,
	RTC_EVENT_FAIL,
	RTC_EVENT_REJOIN,
	RTC_EVENT_RECONNECTING,	
 
	RTC_EVENT_ERROR = 100,
	RTC_EVENT_WARNING,
	RTC_EVENT_ENTERROOM_RESULT,
	RTC_EVENT_EXITROOM_RESULT,
	RTC_EVENT_REMOTE_USER_ENTER,
	RTC_EVENT_REMOTE_USER_LEAVE,
	RTC_EVENT_USER_AUDIO_AVAILABLE,
	RTC_EVENT_USER_VIDEO_AVAILABLE, 
	RTC_EVENT_PUBLISH_CDN,
	RTC_EVENT_FIRST_VIDEO,
	RTC_EVENT_MIX_STREAM,
};

#define MIX_CANVAS_WIDTH 750
#define MIX_CANVAS_HEIGHT 564
#define MIX_FPS 20
