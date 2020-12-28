#include "rtc-param.h"

VideoLinkParam *VideoLinkParam::getInstance() {
	static VideoLinkParam vlp;
	return &vlp;
}

VideoLinkParam::VideoLinkParam()
{
	videoEncParams.videoResolution = TRTCVideoResolution_1920_1080;
	videoEncParams.videoBitrate = 2000;
	videoEncParams.videoFps = 20;

	qosParams.preference = TRTCVideoQosPreferenceClear;
	qosParams.controlMode = TRTCQosControlModeServer;
}
