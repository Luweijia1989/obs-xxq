#include "rtc-base.h"
#include "trtc/TRTCCloudCore.h"
#include "rtc-define.h"
#include <QDebug>

TRTC::TRTC()
{
	connect(TRTCCloudCore::GetInstance(), &TRTCCloudCore::trtcEvent, this, [=](int type, QJsonObject data){
		if (type == RTC_EVENT_ENTERROOM_RESULT)
		{
			int result = data["result"].toInt();
			onEnterRoom(result);
		}
		else if (type == RTC_EVENT_EXITROOM_RESULT)
		{
			onExitRoom();
		}
		else if (type == RTC_EVENT_USER_AUDIO_AVAILABLE)
		{
			onUserAudioAvailable(data["userId"].toString(), data["available"].toBool());
		}
		else if (type == RTC_EVENT_SWITCH_ROOM)
		{
			onSwitchRoom(data["errCode"].toInt(), data["errMsg"].toString());
		}

		emit onEvent(type, data);
	});
}

void TRTC::init() {}

void TRTC::enterRoom() {}

void TRTC::switchRoom(int id)
{
	TRTCSwitchRoomConfig config;
	CDataCenter::GetInstance()->getLocalUserInfo()._roomId = id;
	config.roomId = id;
	config.strRoomId = "";
	config.userSig = CDataCenter::GetInstance()->getLocalUserInfo()._userSig.c_str();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->switchRoom(config);
}

void TRTC::onEnterRoom(int result)
{
	if (result >= 0)
	{
		CDataCenter::GetInstance()->m_bIsEnteredRoom = true;

		TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(false);
		TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(false);

		LocalUserInfo& info = CDataCenter::GetInstance()->getLocalUserInfo();
		info._bEnterRoom = true;

		qDebug() << QString(u8"进入[%1]房间成功,耗时:%2ms").arg(info._roomId).arg(result);
		if (CDataCenter::GetInstance()->m_bCDNMixTranscoding)
			TRTCCloudCore::GetInstance()->startCloudMixStream();
	}
	else
	{
		LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
		qDebug() << QString(u8"进房失败，ErrorCode:%1").arg(result);
	}
}

void TRTC::onExitRoom()
{
	qDebug() << "onExitRoom===";
	CDataCenter::GetInstance()->CleanRoomInfo();
	TRTCCloudCore::GetInstance()->Uninit();
	CDataCenter::GetInstance()->m_bIsEnteredRoom = false;
}

void TRTC::onUserAudioAvailable(QString userId, bool available)
{
	qDebug() << QString("[%1]onAudioAvailable : %2").arg(userId).arg(available);

	RemoteUserInfo* remoteInfo = CDataCenter::GetInstance()->FindRemoteUser(userId.toStdString());
	if (remoteInfo != nullptr && remoteInfo->user_id != "")
	{
		remoteInfo->available_audio = available;
		remoteInfo->subscribe_audio = available;
	}
}

void TRTC::onUserVideoAvailable(QString userId, bool available)
{
	RemoteUserInfo* remoteInfo = CDataCenter::GetInstance()->FindRemoteUser(userId);
	if (available) {
		CDataCenter::GetInstance()->addRemoteUser(userId, false);
		if (remoteInfo != nullptr && remoteInfo->user_id != "")
		{
			remoteInfo->available_main_video = true;
			remoteInfo->subscribe_main_video = true;
			if (m_pVideoViewLayout->IsRemoteViewShow(UTF82Wide(userId), TRTCVideoStreamTypeBig));
			{
				TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteView(
					userId.c_str(), CDataCenter::GetInstance()->getRemoteVideoStreamType(), NULL);
			}
			TRTCCloudCore::GetInstance()->getTRTCCloud()->setRemoteVideoRenderCallback(userId.c_str(), TRTCVideoPixelFormat_BGRA32, \
				TRTCVideoBufferType_Buffer, (ITRTCVideoRenderCallback*)getShareViewMgrInstance());
		}
	}
	else {
		if (TRTCCloudCore::GetInstance()->getTRTCCloud())
			TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(userId.c_str(), TRTCVideoStreamTypeBig);
		//m_pVideoViewLayout->deleteVideoView(UTF82Wide(userId), TRTCVideoStreamTypeBig);
		if (remoteInfo != nullptr && remoteInfo->user_id != "")
		{
			remoteInfo->available_main_video = false;
			remoteInfo->subscribe_main_video = false;
		}
	}

	m_pUserListController->UpdateUserInfo(*remoteInfo);

	if (CDataCenter::GetInstance()->m_mixTemplateID <= TRTCTranscodingConfigMode_Manual) TRTCCloudCore::GetInstance()->updateMixTranCodeInfo();
	m_pVideoViewLayout->muteVideo(UTF82Wide(userId), TRTCVideoStreamTypeBig, !available);
	LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
	CDuiString strFormat;
	strFormat.Format(L"%s[%s]onVideoAvailable : %d", Log::_GetDateTimeString().c_str(), UTF82Wide(userId).c_str(), available);
	TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData(), true);
}

void TRTC::onSwitchRoom(int errCode, QString errMsg)
{
	qDebug() << QString("onSwitchRoom errorCode[%1], errorInfo[%2]").arg(errCode).arg(errMsg);

	if (errCode == ERR_NULL)
	{
		LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
		qDebug() << QString("switch room success, room id: %1, user id: %2").arg(info._roomId).arg(info._userId);
	}
}

QINIURTC::QINIURTC(){}

void QINIURTC::init() {}

void QINIURTC::enterRoom() {}
