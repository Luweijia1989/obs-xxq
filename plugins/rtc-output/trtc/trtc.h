#pragma once

#include <TRTC/ITRTCCloud.h>
class TRTC : public ITRTCCloudCallback {
public:
	TRTC();
	~TRTC();

	void enterRoom(uint32_t appid, uint32_t roomid, const char *userid,
		       const char *usersig);
	void leaveRoom();

	virtual void onError(TXLiteAVError errCode, const char *errMsg,
			     void *arg);
	virtual void onWarning(TXLiteAVWarning warningCode,
			       const char *warningMsg, void *arg);
	virtual void onEnterRoom(int result);
	virtual void onExitRoom(int reason);
	virtual void onUserEnter(const char *userId);
	virtual void onUserExit(const char *userId, int reason);

private:
	ITRTCCloud *m_trtc = nullptr;
	bool bPushSmallVideo = false; //推流打开推双流标志。
	bool bPlaySmallVideo = false; //默认拉低请视频流标志。
};
