#include "TRTCCloudCore.h"
#include <mutex>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <assert.h>
#include "util/base.h"
#include "../rtc-define.h"

TRTCCloudCore *TRTCCloudCore::m_instance = nullptr;
static std::mutex engine_mex;
TRTCCloudCore *TRTCCloudCore::GetInstance()
{
	if (m_instance == NULL) {
		engine_mex.lock();
		if (m_instance == NULL) {
			m_instance = new TRTCCloudCore();
		}
		engine_mex.unlock();
	}
	return m_instance;
}
void TRTCCloudCore::Destory()
{
	engine_mex.lock();
	if (m_instance) {
		delete m_instance;
		m_instance = nullptr;
	}
	engine_mex.unlock();
}
TRTCCloudCore::TRTCCloudCore()
{
	trtc_module_ = nullptr;
	if (trtc_module_ != nullptr)
		return;

	trtc_module_ = ::LoadLibraryExA("liteav.dll", NULL,
					LOAD_WITH_ALTERED_SEARCH_PATH);

	if (trtc_module_ == NULL) {
		DWORD dw_ret = GetLastError();
		blog(LOG_ERROR, "TRTC Load liteav.dll Fail ErrorCode[0x%04X]",
		     dw_ret);
		return;
	} else {

		getTRTCShareInstance_ = (GetTRTCShareInstance)::GetProcAddress(
			trtc_module_, "getTRTCShareInstance");

		destroyTRTCShareInstance_ =
			(DestroyTRTCShareInstance)::GetProcAddress(
				trtc_module_, "destroyTRTCShareInstance");

		m_pCloud = getTRTCShareInstance();
	}
}

TRTCCloudCore::~TRTCCloudCore()
{
	destroyTRTCShareInstance_();
	m_pCloud = nullptr;
	getTRTCShareInstance_ = nullptr;
	destroyTRTCShareInstance_ = nullptr;

	if (trtc_module_)
		FreeLibrary(trtc_module_);
}

void TRTCCloudCore::Init()
{
	m_pCloud->addCallback(this);
	m_pCloud->setLogCallback(this);
	m_pCloud->setAudioFrameCallback(this);
	CDataCenter::GetInstance()->Init();
}

void TRTCCloudCore::Uninit()
{
	m_pCloud->removeCallback(this);
	m_pCloud->setLogCallback(nullptr);
}

void TRTCCloudCore::PreUninit()
{
	m_pCloud->stopPublishing();
	m_pCloud->stopPublishCDNStream();
	stopCloudMixStream();
	m_pCloud->enableCustomAudioCapture(false);
	m_pCloud->enableCustomVideoCapture(false);
	m_pCloud->stopAllRemoteView();
	m_pCloud->stopLocalPreview();
	m_pCloud->muteLocalVideo(true);
	m_pCloud->muteLocalAudio(true);
}

ITRTCCloud *TRTCCloudCore::getTRTCCloud()
{
	return m_pCloud;
}

ITRTCCloudCallback *TRTCCloudCore::GetITRTCCloudCallback()
{
	return this;
}

void TRTCCloudCore::onError(TXLiteAVError errCode, const char *errMsg,
			    void *arg)
{
	blog(LOG_DEBUG, "onError errorCode[%d], errorInfo[%s]", errCode,
	     errMsg);

	QJsonObject data;
	data["code"] = errCode;
	data["msg"] = errMsg;
	emit trtcEvent(RTC_EVENT_ERROR, data);
}

void TRTCCloudCore::onWarning(TXLiteAVWarning warningCode,
			      const char *warningMsg, void *arg)
{
	blog(LOG_DEBUG, "onWarning errorCode[%d], errorInfo[%s]", warningCode,
	     warningMsg);

	QJsonObject data;
	data["code"] = warningCode;
	data["msg"] = warningMsg;
	emit trtcEvent(RTC_EVENT_WARNING, data);
}

void TRTCCloudCore::onEnterRoom(int result)
{
	blog(LOG_INFO, "onEnterRoom elapsed[%d]", result);

	QJsonObject data;
	data["result"] = result;
	emit trtcEvent(RTC_EVENT_ENTERROOM_RESULT, data);
}

void TRTCCloudCore::onExitRoom(int reason)
{ 
	Q_UNUSED(reason)
	blog(LOG_INFO, "onExitRoom reason[%d]", reason);

	emit trtcEvent(RTC_EVENT_EXITROOM_RESULT, QJsonObject());
}

void TRTCCloudCore::onRemoteUserEnterRoom(const char *userId)
{
	blog(LOG_INFO, "onMemberEnter userId[%s]", userId);

	QJsonObject data;
	data["userId"] = userId;
	emit trtcEvent(RTC_EVENT_REMOTE_USER_ENTER, data);
}

void TRTCCloudCore::onRemoteUserLeaveRoom(const char *userId, int reason)
{
	Q_UNUSED(reason)
	blog(LOG_INFO, "onMemberExit userId[%s]", userId);

	QJsonObject data;
	data["userId"] = userId;
	emit trtcEvent(RTC_EVENT_REMOTE_USER_LEAVE, data);
}

void TRTCCloudCore::onUserAudioAvailable(const char *userId, bool available)
{
	blog(LOG_INFO, "onUserAudioAvailable userId[%s] available[%d]", userId, available);

	QJsonObject data;
	data["userId"] = userId;
	data["available"] = available;
	emit trtcEvent(RTC_EVENT_USER_AUDIO_AVAILABLE, data);
}

void TRTCCloudCore::onFirstAudioFrame(const char *userId)
{
	blog(LOG_INFO, "onFirstAudioFrame userId[%s]", userId);
}

void TRTCCloudCore::onFirstVideoFrame(const char *userId,
				      const TRTCVideoStreamType streamType,
				      const int width, const int height)
{
	blog(LOG_INFO, "onFirstAudioFrame userId[%s]", userId);
	emit trtcEvent(RTC_EVENT_FIRST_VIDEO, QJsonObject());
}

void TRTCCloudCore::onUserVoiceVolume(TRTCVolumeInfo *userVolumes,
				      uint32_t userVolumesCount,
				      uint32_t totalVolume)
{
	//更新远端音量信息，暂时不需要
}

void TRTCCloudCore::onNetworkQuality(TRTCQualityInfo localQuality,
				     TRTCQualityInfo *remoteQuality,
				     uint32_t remoteQualityCount)
{
	//网络质量检测回调
}

void TRTCCloudCore::onUserSubStreamAvailable(const char *userId, bool available)
{
	//这是用户屏幕分享的画面，我们的连麦场景不应该有这一条流
	blog(LOG_INFO, "onUserSubStreamAvailable userId[%s] available[%d]", userId, available);
}

void TRTCCloudCore::onUserVideoAvailable(const char *userId, bool available)
{
	blog(LOG_INFO, "onUserVideoAvailable userId[%s] available[%d]\n", userId, available);

	QJsonObject data;
	data["userId"] = userId;
	data["available"] = available;
	emit trtcEvent(RTC_EVENT_USER_VIDEO_AVAILABLE, data);
}

void TRTCCloudCore::onStatistics(const TRTCStatistics &statis)
{
	//更新云端混流的结构信息
	
}

void TRTCCloudCore::onLog(const char *log, TRTCLogLevel level,
			  const char *module)
{
	//LINFO(L"onStatus userId[%s], paramCount[%d] \n", UTF82Wide(userId).c_str());

	int type = (int)level;
	if (type <= -1000 && type > -1999 && log && module) {
		int streamType = -1000 - type;
		blog(LOG_DEBUG, "Dashboard log data, stream type: %1, msg: %s", streamType, log);
	} else if (type == -2000 && log && module) {
		int streamType = 0;
		blog(LOG_DEBUG, "SDK Event data, stream type: %1, msg: %s", streamType, log);
	}
}

void TRTCCloudCore::onSetMixTranscodingConfig(int errCode, const char *errMsg)
{
	blog(LOG_INFO, "onSetMixTranscodingConfig errCode[%d], errMsg[%s]", errCode, errMsg);
	QJsonObject data;
	data["errCode"] = errCode;
	data["errMsg"] = errMsg;
	emit trtcEvent(RTC_EVENT_MIX_STREAM, data);
}

void TRTCCloudCore::onStartPublishing(int err, const char *errMsg)
{
	blog(LOG_INFO, "onStartPublishing err[%d], errMsg[%s]", err, errMsg);
	QJsonObject data;
	data["errCode"] = err;
	data["errMsg"] = errMsg;
	emit trtcEvent(RTC_EVENT_PUBLISH_CDN, data);
}

void TRTCCloudCore::onStopPublishing(int err, const char *errMsg)
{
	blog(LOG_INFO, "onStartPublishing err[%d], errMsg[%s]", err, errMsg);
}

void TRTCCloudCore::onStartPublishCDNStream(int err, const char *errMsg)
{
	blog(LOG_INFO, "onStartPublishCDNStream err[%d], errMsg[%s]", err, errMsg);
	QJsonObject data;
	data["errCode"] = err;
	data["errMsg"] = errMsg;
	emit trtcEvent(RTC_EVENT_PUBLISH_CDN, data);
}

void TRTCCloudCore::onStopPublishCDNStream(int err, const char *errMsg)
{
	blog(LOG_INFO, "onStopPublishCDNStream err[%d], errMsg[%s]", err, errMsg);
}


void TRTCCloudCore::onConnectionLost()
{
	blog(LOG_INFO, "onConnectionLost");
}

void TRTCCloudCore::onTryToReconnect()
{
	blog(LOG_INFO, "onTryToReconnect");
}

void TRTCCloudCore::onConnectionRecovery()
{
	blog(LOG_INFO, "onConnectionRecovery");
}

void TRTCCloudCore::connectOtherRoom(QString userId, uint32_t roomId)
{
	QString json = QString("{\"roomId\":%1,\"userId\":\"%2\"}").arg(roomId).arg(userId);
	std::string str = json.toStdString();
	if (m_pCloud) {
		m_pCloud->connectOtherRoom(str.c_str());
	}
}

void TRTCCloudCore::stopCloudMixStream()
{
	if (m_pCloud) {
		m_pCloud->setMixTranscodingConfig(NULL);
	}
}

void TRTCCloudCore::startCloudMixStream(const char *remoteRoomId, int cdnAppID, int bizID)
{
	blog(LOG_INFO, "startCloudMixStream");

	int appId = cdnAppID;
	int bizId = bizID;
	
	TRTCTranscodingConfig config;
	config.mode = (TRTCTranscodingConfigMode)CDataCenter::GetInstance()->m_mixTemplateID;
	config.appId = appId;
	config.bizId = bizId;

	if (config.mode > TRTCTranscodingConfigMode_Manual) {
		if (config.mode == TRTCTranscodingConfigMode_Template_PresetLayout) {
			setPresetLayoutConfig(config, remoteRoomId);
		}

		m_pCloud->setMixTranscodingConfig(&config);
		if (config.mixUsersArray) {
			delete[] config.mixUsersArray;
			config.mixUsersArray = nullptr;
		}
		return;
	}
}

void TRTCCloudCore::setPresetLayoutConfig(TRTCTranscodingConfig &config, const char *remoteRoomId)
{

	int canvasWidth = MIX_CANVAS_WIDTH;
	int canvasHeight = MIX_CANVAS_HEIGHT;
	config.videoWidth = canvasWidth;
	config.videoHeight = canvasHeight;
	config.videoBitrate = 2000;
	config.videoFramerate = MIX_FPS;
	config.videoGOP = 2;
	config.audioSampleRate = 44100;
	config.audioBitrate = 64;
	config.audioChannels = 2;

	config.mixUsersArraySize = 2;

	TRTCMixUser *mixUsersArray = new TRTCMixUser[config.mixUsersArraySize];
	config.mixUsersArray = mixUsersArray;
	int zOrder = 1, index = 0;
	auto setMixUser = [&](const char *_userid, int _index, int _zOrder,
			      int left, int top, int width, int height, const char *roomId) {
		mixUsersArray[_index].roomId = roomId;
		mixUsersArray[_index].userId = _userid;
		mixUsersArray[_index].zOrder = _zOrder;
		{
			mixUsersArray[_index].rect.left = left;
			mixUsersArray[_index].rect.top = top;
			mixUsersArray[_index].rect.right = left + width;
			mixUsersArray[_index].rect.bottom = top + height;
		}
	};
	//本地主路信息
	setMixUser("$PLACE_HOLDER_LOCAL_MAIN$", index, zOrder, 0, 0, canvasWidth / 2, canvasHeight, remoteRoomId);
	index++;
	zOrder++;

	setMixUser("$PLACE_HOLDER_REMOTE$", index, zOrder, canvasWidth / 2, 0, canvasWidth / 2, canvasHeight, remoteRoomId);
}
