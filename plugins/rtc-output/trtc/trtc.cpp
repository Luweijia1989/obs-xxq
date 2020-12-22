#include "trtc.h"
#include "util/base.h"

TRTC::TRTC() : m_trtc(getTRTCShareInstance()){
	videoEncParams.videoResolution = TRTCVideoResolution_1920_1080;
	videoEncParams.videoBitrate = 2000;
	videoEncParams.videoFps = 20;

	qosParams.preference = TRTCVideoQosPreferenceClear;
	qosParams.controlMode = TRTCQosControlModeServer;
}

 TRTC::~TRTC() {
	destroyTRTCShareInstance();
 }

void TRTC::enterRoom(uint32_t appid, uint32_t roomid, const char *userid,
		     const char *usersig)
{
	blog(LOG_INFO, "TRTC Enter Room, %ld %ld %s %s", appid, roomid, userid,
	     usersig);
	TRTCParams params;
	params.sdkAppId = appid;
	params.roomId = roomid;
	params.userId = userid;
	params.userSig = usersig;
	//params.privateMapKey = privateMapKey.c_str();

	// 大画面的编码器参数设置
	// 设置视频编码参数，包括分辨率、帧率、码率等等，这些编码参数来自于 TRTCSettingViewController 的设置
	// 注意（1）：不要在码率很低的情况下设置很高的分辨率，会出现较大的马赛克
	// 注意（2）：不要设置超过25FPS以上的帧率，因为电影才使用24FPS，我们一般推荐15FPS，这样能将更多的码率分配给画质
	m_trtc->setVideoEncoderParam(videoEncParams);
	m_trtc->setNetworkQosParam(qosParams);

	if (m_bPushSmallVideo) {
		//小画面的编码器参数设置
		//TRTC SDK 支持大小两路画面的同时编码和传输，这样网速不理想的用户可以选择观看小画面
		//注意：iPhone & Android 不要开启大小双路画面，非常浪费流量，大小路画面适合 Windows 和 MAC 这样的有线网络环境
		TRTCVideoEncParam param;
		param.videoFps = 15;
		param.videoBitrate = 100;
		param.videoResolution = TRTCVideoResolution_320_240;
		getTRTCCloud()->enableSmallVideoStream(true, param);
	}
	if (m_bPlaySmallVideo) {
		getTRTCCloud()->setPriorRemoteVideoStreamType(
			TRTCVideoStreamTypeSmall);
	}

	getTRTCCloud()->callExperimentalAPI(
		"{\"api\":\"setCustomRenderMode\",\"params\" :{\"mode\":1}}");

	getTRTCCloud()->enterRoom(
		params, TRTCStorageConfigMgr::GetInstance()->appScene);
}

void TRTC::leaveRoom() {}

void TRTC::onError(TXLiteAVError errCode, const char *errMsg, void *arg) {}

void TRTC::onWarning(TXLiteAVWarning warningCode, const char *warningMsg,
		     void *arg)
{
}

void TRTC::onEnterRoom(int result) {}

void TRTC::onExitRoom(int reason) {}

void TRTC::onUserEnter(const char *userId) {}

void TRTC::onUserExit(const char *userId, int reason) {}
