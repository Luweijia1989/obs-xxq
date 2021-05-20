#pragma once
#include "ITRTCCloud.h"
#include "DataCenter.h"
#include <map>
#include <string>
#include <mutex>
#include <QObject>
#include <QJsonObject>

class TRTC;

typedef ITRTCCloud *(__cdecl *GetTRTCShareInstance)();
typedef void(__cdecl *DestroyTRTCShareInstance)();

struct MixInfo
{
	uint32_t width;
	uint32_t height;
	uint32_t vbitrate;
	uint32_t fps;
	int mixerCount;
	bool onlyAnchorVideo;
};

class TRTCCloudCore : public QObject,
		      public ITRTCCloudCallback,
		      public ITRTCLogCallback,
		      public ITRTCAudioFrameCallback {
	Q_OBJECT
public:
	typedef struct _tagMediaDeviceInfo {
		std::wstring _text;
		std::wstring _type; //识别类别选项:camera/speaker/mic
		std::wstring _deviceId;
		int _index = 0;
		bool _select = false;
	} MediaDeviceInfo;

	static TRTCCloudCore *GetInstance();
	static void Destory();
	TRTCCloudCore();
	~TRTCCloudCore();

public:
	void Init();
	void Uninit();
	void PreUninit(bool isVideoLink);
	ITRTCCloud *getTRTCCloud();
	ITRTCCloudCallback *GetITRTCCloudCallback();

signals:
	void trtcEvent(int type, QJsonObject data);

public:
	//interface ITRTCCloudCallback
	virtual void onError(TXLiteAVError errCode, const char *errMsg,
			     void *arg);
	virtual void onWarning(TXLiteAVWarning warningCode,
			       const char *warningMsg, void *arg);
	virtual void onEnterRoom(int result);
	virtual void onExitRoom(int reason);
	virtual void onRemoteUserEnterRoom(const char *userId);
	virtual void onRemoteUserLeaveRoom(const char *userId, int reason);
	virtual void onUserAudioAvailable(const char *userId, bool available);
	virtual void onFirstAudioFrame(const char *userId);
	virtual void onFirstVideoFrame(const char* userId, const TRTCVideoStreamType streamType, const int width, const int height);
	virtual void onUserVoiceVolume(TRTCVolumeInfo *userVolumes,
				       uint32_t userVolumesCount,
				       uint32_t totalVolume);
	virtual void onUserSubStreamAvailable(const char *userId,
					      bool available);
	virtual void onUserVideoAvailable(const char *userId, bool available);
	virtual void onNetworkQuality(TRTCQualityInfo localQuality,
				      TRTCQualityInfo *remoteQuality,
				      uint32_t remoteQualityCount);
	virtual void onStatistics(const TRTCStatistics &statis);
	virtual void onConnectionLost();
	virtual void onTryToReconnect();
	virtual void onConnectionRecovery();

	virtual void onLog(const char *log, TRTCLogLevel level,
			   const char *module);
	virtual void onSetMixTranscodingConfig(int errCode, const char *errMsg);
	virtual void onStartPublishing(int err, const char *errMsg);
	virtual void onStopPublishing(int err, const char *errMsg);
	virtual void onStartPublishCDNStream(int err, const char *errMsg);
	virtual void onStopPublishCDNStream(int err, const char *errMsg);

public:
	void connectOtherRoom(QString userId, uint32_t roomId);
	void startCloudMixStream(const char *remoteRoomId, int cdnAppID, int bizID, const MixInfo &info);
	void stopCloudMixStream();

protected:
	void setPresetLayoutConfig(TRTCTranscodingConfig &config, const char *remoteRoomId, const MixInfo &info);

private:
	static TRTCCloudCore *m_instance;
	TRTC *m_rtcInstance = nullptr;
	ITRTCCloud *m_pCloud = nullptr;

private:
	HMODULE trtc_module_;
	GetTRTCShareInstance getTRTCShareInstance_ = nullptr;
	DestroyTRTCShareInstance destroyTRTCShareInstance_ = nullptr;
};
