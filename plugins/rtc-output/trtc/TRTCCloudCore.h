#pragma once
#include "TRTC/ITRTCCloud.h"
#include "DataCenter.h"
#include <map>
#include <string>
#include <mutex>
#include <QObject>
#include <QJsonObject>

class TRTC;

struct DashboardInfo {
	int streamType = -1;
	std::string userId;
	std::string buffer;
};
typedef ITRTCCloud *(__cdecl *GetTRTCShareInstance)();
typedef void(__cdecl *DestroyTRTCShareInstance)();

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
	void PreUninit();
	ITRTCCloud *getTRTCCloud();
	ITXDeviceManager *getDeviceManager();
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
	virtual void onSwitchRoom(TXLiteAVError errCode, const char *errMsg);
	virtual void onFirstAudioFrame(const char *userId);
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
	virtual void onConnectOtherRoom(const char *userId,
					TXLiteAVError errCode,
					const char *errMsg);
	virtual void onDisconnectOtherRoom(TXLiteAVError errCode,
					   const char *errMsg);
	virtual void onSetMixTranscodingConfig(int errCode, const char *errMsg);
	virtual void onStartPublishing(int err, const char *errMsg);
	virtual void onStopPublishing(int err, const char *errMsg);

public:
	void connectOtherRoom(QString userId, uint32_t roomId);
	void startCloudMixStream();
	void stopCloudMixStream();
	void updateMixTranCodeInfo();
	void getMixVideoPos(int index, int &left, int &top, int &right,
			    int &bottom);

	void startCustomCaptureAudio(std::wstring filePath, int samplerate,
				     int channel);
	void stopCustomCaptureAudio();
	void startCustomCaptureVideo(std::wstring filePat, int width,
				     int height);
	void stopCustomCaptureVideo();

	void sendCustomAudioFrame();
	void sendCustomVideoFrame();

protected:
	void setPresetLayoutConfig(TRTCTranscodingConfig &config);
	std::string GetPathNoExt(std::string path);

private:
	static TRTCCloudCore *m_instance;
	std::string m_localUserId;

	TRTC *m_rtcInstance = nullptr;

	std::vector<MediaDeviceInfo> m_vecSpeakDevice;
	std::vector<MediaDeviceInfo> m_vecMicDevice;
	std::vector<MediaDeviceInfo> m_vecCameraDevice;

	ITRTCCloud *m_pCloud = nullptr;
	ITXDeviceManager *m_pDeviceManager = nullptr;
	bool m_bFirstUpdateDevice = false;

	//云端混流功能

	bool m_bStartCloudMixStream = false;

	//自定义采集功能:
	std::wstring m_videoFilePath, m_audioFilePath;
	bool m_bStartCustomCaptureAudio = false;
	bool m_bStartCustomCaptureVideo = false;
	uint32_t _offset_videoread = 0, _offset_audioread = 0;
	uint32_t _video_file_length = 0, _audio_file_length = 0;
	char *_audio_buffer = nullptr;
	char *_video_buffer = nullptr;
	int _audio_samplerate = 0, _audio_channel = 0;
	int _video_width = 0, _video_height = 0;

	bool m_bPreUninit = false;

	std::thread *custom_audio_thread_ = nullptr;
	std::thread *custom_video_thread_ = nullptr;

private:
	HMODULE trtc_module_;
	GetTRTCShareInstance getTRTCShareInstance_ = nullptr;
	DestroyTRTCShareInstance destroyTRTCShareInstance_ = nullptr;
};
