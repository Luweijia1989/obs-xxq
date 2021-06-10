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
	sentBytes = 0;
	m_pCloud->addCallback(this);
	m_pCloud->setLogCallback(this);
	m_pCloud->setAudioFrameCallback(this);
}

void TRTCCloudCore::Uninit()
{
	m_pCloud->removeCallback(this);
	m_pCloud->setLogCallback(nullptr);
}

void TRTCCloudCore::PreUninit(bool isVideoLink)
{
	if (isVideoLink)
	{
		m_pCloud->stopPublishing();
		m_pCloud->stopPublishCDNStream();
		stopCloudMixStream();
		m_pCloud->enableCustomAudioCapture(false);
		m_pCloud->enableCustomVideoCapture(false);
		m_pCloud->stopAllRemoteView();
		m_pCloud->muteLocalVideo(true);
		m_pCloud->muteLocalAudio(true);
	}
	else
		m_pCloud->stopLocalAudio();
}

ITRTCCloud *TRTCCloudCore::getTRTCCloud()
{
	return m_pCloud;
}

trtc::ITXDeviceManager *TRTCCloudCore::deviceManager()
{
	return m_pCloud->getDeviceManager();
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
	blog(LOG_INFO, "onFirstVideoFrame userId[%s]", userId);
	emit trtcEvent(RTC_EVENT_FIRST_VIDEO, QJsonObject());
}

void TRTCCloudCore::onUserVoiceVolume(TRTCVolumeInfo *userVolumes,
				      uint32_t userVolumesCount,
				      uint32_t totalVolume)
{
	QJsonObject obj;
	QJsonArray ret;
	for (int i=0; i<userVolumesCount; i++)
	{
		TRTCVolumeInfo info = userVolumes[i];
		if (info.volume > 0)
			ret.append(QString(info.userId));
	}
	obj["users"] = ret;
	emit trtcEvent(RTC_EVENT_USER_VOLUME, obj);
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
	sentBytes = statis.sentBytes;
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


void TRTCCloudCore::onConnectOtherRoom(const char *userId,
				       TXLiteAVError errCode,
				       const char *errMsg)
{
	blog(LOG_INFO, "onConnectOtherRoom err[%d], errMsg[%s], userId[%s], remoteRoomId[%ld]", errCode, errMsg, userId, m_remoteRoomId);
}

void TRTCCloudCore::onDisconnectOtherRoom(TXLiteAVError errCode,
					  const char *errMsg)
{
	
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
		m_remoteRoomId = roomId;
		m_pCloud->connectOtherRoom(str.c_str());
	}
}

void TRTCCloudCore::disconnectOtherRoom()
{
	if (m_pCloud) {
		m_remoteRoomId = -1;
		m_pCloud->disconnectOtherRoom();
	}
}

void TRTCCloudCore::stopCloudMixStream()
{
	if (m_pCloud) {
		m_pCloud->setMixTranscodingConfig(NULL);
	}
}

void TRTCCloudCore::updateCloudMixStream(const RTCBase::CloudMixInfo &mixInfo, const QList<MixUserInfo> &mixUsers)
{
	blog(LOG_INFO, "updateCloudMixStream");

	m_config.mode = mixInfo.usePresetLayout ? TRTCTranscodingConfigMode_Template_PresetLayout : TRTCTranscodingConfigMode_Manual;
	m_config.appId = mixInfo.cdnAppId;
	m_config.bizId = mixInfo.cdnBizId;
	m_config.videoWidth = mixInfo.mixWidth;
	m_config.videoHeight = mixInfo.mixHeight;
	m_config.videoBitrate = mixInfo.mixVideoBitRate;
	m_config.videoFramerate = mixInfo.mixFps;
	m_config.videoGOP = 2;
	m_config.audioSampleRate = 48000;
	m_config.audioBitrate = 128;
	m_config.audioChannels = 2;

	if (mixInfo.usePresetLayout)
		setPresetLayoutConfig(m_config, mixInfo);
	else
		setManualLayoutConfig(mixInfo.mixWidth, mixInfo.mixHeight, m_config, mixUsers);
}

void TRTCCloudCore::setManualLayoutConfig(int width, int height, TRTCTranscodingConfig &config, const QList<MixUserInfo> &mixUsers)
{
	QVector<TRTCMixUser> mixUserArray;
	int zOrder = 1;
	for (auto iter=mixUsers.begin(); iter!=mixUsers.end(); iter++)
	{
		const MixUserInfo &user = *iter;
		if (!user.audioAvailable || user.mute)
			continue;

		TRTCMixUser one;
		one.roomId = user.roomId.c_str();
		one.userId = user.userId.c_str();
		one.zOrder = zOrder++;
		if (user.isSelf)
		{
			one.inputType = TRTCMixInputTypeAudioVideo;
			one.streamType = TRTCVideoStreamTypeBig;
			one.rect.left = 0;
			one.rect.top = 0;
			one.rect.right = width;
			one.rect.bottom = height;
		}
		else
		{
			one.inputType = TRTCMixInputTypePureAudio;
		}
		mixUserArray.append(one);
	}

	m_config.mixUsersArraySize = mixUserArray.count();
	m_config.mixUsersArray = mixUserArray.data();
	m_pCloud->setMixTranscodingConfig(&m_config);
	m_config.mixUsersArray = nullptr;
}

void TRTCCloudCore::setPresetLayoutConfig(TRTCTranscodingConfig &config, const RTCBase::CloudMixInfo &mixInfo)
{
	int canvasWidth = mixInfo.mixWidth;
	int canvasHeight = mixInfo.mixHeight;

	config.mixUsersArraySize = mixInfo.mixUsers;

	TRTCMixUser *mixUsersArray = new TRTCMixUser[config.mixUsersArraySize];
	config.mixUsersArray = mixUsersArray;
	int zOrder = 1, index = 0;
	auto setMixUser = [&](bool onlyAudio, const char *_userid, int _index, int _zOrder,
			      int left, int top, int width, int height, const char *roomId) {
		mixUsersArray[_index].roomId = roomId;
		mixUsersArray[_index].userId = _userid;
		mixUsersArray[_index].zOrder = _zOrder;
		if (onlyAudio)
			mixUsersArray[_index].inputType = TRTCMixInputTypePureAudio;
		else
			mixUsersArray[_index].inputType = TRTCMixInputTypeAudioVideo;

		{
			mixUsersArray[_index].rect.left = left;
			mixUsersArray[_index].rect.top = top;
			mixUsersArray[_index].rect.right = left + width;
			mixUsersArray[_index].rect.bottom = top + height;
		}
	};
	//本地主路信息

	if (mixInfo.onlyMixAnchorVideo)
	{
		for (int i = 0; i < mixInfo.mixUsers; i++)
		{
			if (i == 0)
				setMixUser(false, "$PLACE_HOLDER_LOCAL_MAIN$", index, zOrder, 0, 0, canvasWidth, canvasHeight, nullptr);
			else
				setMixUser(true, "$PLACE_HOLDER_REMOTE$", index, zOrder, 0, 0, 0, 0, nullptr);
			index++;
			zOrder++;
		}
	}
	else
	{
		for (int i = 0; i < mixInfo.mixUsers; i++)
		{
			setMixUser(false, i == 0 ? "$PLACE_HOLDER_LOCAL_MAIN$" : "$PLACE_HOLDER_REMOTE$", index, zOrder, canvasWidth / mixInfo.mixUsers * i, 0, canvasWidth / mixInfo.mixUsers, canvasHeight, nullptr);
			index++;
			zOrder++;
		}
	}

	m_pCloud->setMixTranscodingConfig(&m_config);

	if (m_config.mixUsersArray) {
		delete[] m_config.mixUsersArray;
		m_config.mixUsersArray = nullptr;
	}
}
