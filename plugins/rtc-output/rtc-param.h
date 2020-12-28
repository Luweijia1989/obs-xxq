#pragma once

#include <TRTC/ITRTCCloud.h>
class VideoLinkParam
{
public:
	static VideoLinkParam *getInstance();

	TRTCVideoEncParam videoEncParams;
	TRTCNetworkQosParam qosParams;
	TRTCAppScene appScene = TRTCAppSceneVideoCall;

private:
	VideoLinkParam();

};
