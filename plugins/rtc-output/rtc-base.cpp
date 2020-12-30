#include "rtc-base.h"
#include "trtc/TRTCCloudCore.h"
#include "trtc/GenerateTestUserSig.h"
#include "qnrtc/qiniurtc.h"
#include "TXLiteAVBase.h"
#include "rtc-define.h"
#include <QDebug>
#include <QTimer>

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
		else if (type == RTC_EVENT_REMOTE_USER_ENTER)
		{
			onRemoteUserEnter(data["userId"].toString());
		}
		else if (type == RTC_EVENT_REMOTE_USER_LEAVE)
		{
			onRemoteUserLeave(data["userId"].toString());
		}
		else if (type == RTC_EVENT_CONNECT_OTHER_ROOM)
		{
			onConnectOtherRoom(data["userId"].toString(), data["errCode"].toInt(), data["errMsg"].toString());
		}
		else if (type == RTC_EVENT_DISCONNECT_OTHER_ROOM)
		{
			onDisconnectOtherRoom(data["errCode"].toInt(), data["errMsg"].toString());
		}
		else if (type == RTC_EVENT_USER_VIDEO_AVAILABLE)
		{
			onUserVideoAvailable(data["userId"].toString(), data["available"].toBool());
		}

		emit onEvent(type, data);
	});
}

 TRTC::~TRTC()
 {
	 delete m_yuvBuffer;
	 TRTCCloudCore::Destory();
 }

void TRTC::init()
{
	TRTCCloudCore::GetInstance()->Init();
}

void TRTC::enterRoom()
{
	QString userId = linkInfo().value("userId").toString();
	int roomId = linkInfo().value("roomId").toInt();
	QString userSig = QString::fromStdString(GenerateTestUserSig::instance().genTestUserSig(userId.toStdString()));
	//QString userSig = linkInfo().value("userSig").toString();
	CDataCenter::GetInstance()->setLocalUserInfo(userId.toStdString(), roomId, userSig.toStdString());
	init();

	//设置连接环境
	std::string cmd = QString("{\"api\": \"setNetEnv\",\"params\" :{\"env\" : %1}}").arg(CDataCenter::GetInstance()->m_nLinkTestServer).toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(cmd.c_str());

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setDefaultStreamRecvMode(true, true);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setAudioQuality(TRTCAudioQualityMusic);

	internalEnterRoom();

	//进入房间
	LocalUserInfo& info = CDataCenter::GetInstance()->getLocalUserInfo();


	TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderParam(CDataCenter::GetInstance()->m_videoEncParams);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderMirror(false);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setNetworkQosParam(CDataCenter::GetInstance()->m_qosParams);

	TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalPreview();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalAudio();
	m_bStartCustomCapture = true;
	TRTCCloudCore::GetInstance()->getTRTCCloud()->enableCustomAudioCapture(true);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->enableCustomVideoCapture(true);

	qDebug() << QString(u8"正在进入[%1]房间,customstreamId[%2] sceneType[%3]").arg(info._roomId).arg(QString::fromStdString(CDataCenter::GetInstance()->m_strCustomStreamId)).arg(u8"视频互动直播");
}

void TRTC::exitRoom()
{
	LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();

	m_bStartCustomCapture = false;
	TRTCCloudCore::GetInstance()->PreUninit();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->exitRoom();

	if (!CDataCenter::GetInstance()->m_bIsEnteredRoom)
	{
		onExitRoom();
		return;
	}

	QTimer timer;
	connect(&timer, &QTimer::timeout, &m_exitRoomLoop, &QEventLoop::quit);
	timer.start(1000);
	m_exitRoomLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

void TRTC::switchRoom(int roomId)
{
	TRTCSwitchRoomConfig config;
	CDataCenter::GetInstance()->getLocalUserInfo()._roomId = roomId;
	config.roomId = roomId;
	config.strRoomId = "";
	config.userSig = CDataCenter::GetInstance()->getLocalUserInfo()._userSig.c_str();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->switchRoom(config);
}

void TRTC::connectOtherRoom(QString userId, int roomId)
{
	m_pkRoomId = roomId;
	TRTCCloudCore::GetInstance()->connectOtherRoom(userId, roomId);
}

void TRTC::disconnectOtherRoom()
{
	TRTCCloudCore::GetInstance()->getTRTCCloud()->disconnectOtherRoom();
}

void TRTC::setRemoteViewHwnd(long window)
{
	m_remoteView = window;
}

void TRTC::sendAudio(struct audio_data *data)
{
	if (!m_bStartCustomCapture)
		return;

	TRTCAudioFrame frame;
	frame.audioFormat = LiteAVAudioFrameFormatPCM;
	frame.length = data->frames * 2 * 2;
	frame.data = (char *)data->data[0];
	frame.sampleRate = 48000;
	frame.channel = 2;
	frame.timestamp = data->timestamp / 1000000;
	TRTCCloudCore::GetInstance()->getTRTCCloud()->sendCustomAudioData(&frame);
}

static void cropYUV(struct video_data *data, char *dst, int px, int py, int w, int h)
{
	int copyIndex = 0;
	for (int y = py; y < h + py /*pFrame->height*/; y++) {
		memcpy(dst + copyIndex, data->data[0] + y * data->linesize[0] + px, w);
		copyIndex += w;
	}
	//Y wide
	// Y Y Y Y Y |[width]
	// Y Y Y Y Y
	// [heigth] high

	//U component width
	//U U U U | [w/2]
	//U U U U |
	//[h/2] high

	//u
	void *ptr = NULL;
	int linesize = w / 2;

	int uy = py / 2;

	//u
	for (int u = 0; u < h / 2; u++) {
		ptr = data->data[1] + (u + uy) * data->linesize[1] + px / 2;
		memcpy(dst + copyIndex, ptr, linesize);
		copyIndex += linesize;
	}

	//v
	for (int v = 0; v < h / 2; v++) {
		ptr = data->data[2] + (v + uy) * data->linesize[2] + px / 2;
		memcpy(dst + copyIndex, ptr, linesize);
		copyIndex += linesize;
	}
}


void TRTC::sendVideo(struct video_data *data)
{
	if (!m_bStartCustomCapture)
		return;

	if (!m_yuvBuffer)
		m_yuvBuffer = (char *)malloc(cropInfo().width() * cropInfo().height() * 3 / 2);
	cropYUV(data, m_yuvBuffer, cropInfo().x(), cropInfo().y(), cropInfo().width(), cropInfo().height());

	TRTCVideoFrame frame;
	frame.videoFormat = LiteAVVideoPixelFormat_I420;
	frame.length = cropInfo().width() * cropInfo().height() * 3 / 2;
	frame.data = m_yuvBuffer;
	frame.width = cropInfo().width();
	frame.height = cropInfo().height();
	frame.timestamp = data->timestamp / 1000000;
	TRTCCloudCore::GetInstance()->getTRTCCloud()->sendCustomVideoData(&frame);
}

void TRTC::internalEnterRoom()
{
	//进入房间
	LocalUserInfo& info = CDataCenter::GetInstance()->getLocalUserInfo();
	TRTCParams params;
	params.sdkAppId = GenerateTestUserSig::SDKAPPID;
	params.roomId = info._roomId;//std::to_string(info._roomId).c_str();
	std::string userid = info._userId.c_str();
	params.userId = (char*)userid.c_str();
	std::string userSig = info._userSig.c_str();
	params.userSig = (char*)userSig.c_str();
	std::string privMapEncrypt = "";
	params.privateMapKey = (char*)privMapEncrypt.c_str();
	params.role = CDataCenter::GetInstance()->m_roleType;

	// 默认旁路streamId = sdkappid_roomid_userid_main
	if (CDataCenter::GetInstance()->m_strCustomStreamId.empty())
		CDataCenter::GetInstance()->m_strCustomStreamId = QString("%1_%2_%3_main").arg(GenerateTestUserSig::SDKAPPID).arg(info._roomId).arg(QString::fromStdString(info._userId)).toStdString();
	params.streamId = CDataCenter::GetInstance()->m_strCustomStreamId.c_str();

	// TRTCCloudCore::GetInstance()->getTRTCCloud()->setEncodedDataProcessingListener();
	char api_str[128] = { 0 };
	sprintf_s(api_str, 128, "{\"api\":\"setEncodedDataProcessingListener\", \"params\": {\"listener\":%llu}}", (uint64_t)0);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(api_str);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->enterRoom(params, CDataCenter::GetInstance()->m_sceneParams);
}

void TRTC::onEnterRoom(int result)
{
	if (result >= 0)
	{
		CDataCenter::GetInstance()->m_bIsEnteredRoom = true;

		TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(false);
		TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(false);

		LocalUserInfo& info = CDataCenter::GetInstance()->getLocalUserInfo();

		qDebug() << QString(u8"进入[%1]房间成功,耗时:%2ms").arg(info._roomId).arg(result);
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
	m_exitRoomLoop.quit();
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
	qDebug() << QString(u8"[%1]onVideoAvailable : %2").arg(userId).arg(available);

	std::string idstr = userId.toStdString();
	RemoteUserInfo* remoteInfo = CDataCenter::GetInstance()->FindRemoteUser(idstr);
	if (available) {
		CDataCenter::GetInstance()->addRemoteUser(userId.toStdString(), false);
		if (remoteInfo != nullptr && remoteInfo->user_id != "")
		{
			remoteInfo->available_main_video = true;
			remoteInfo->subscribe_main_video = true;
			TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteView(idstr.c_str(), (HWND)m_remoteView);
		}
	}
	else {
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(idstr.c_str());
		if (remoteInfo != nullptr && remoteInfo->user_id != "")
		{
			remoteInfo->available_main_video = false;
			remoteInfo->subscribe_main_video = false;
		}
	}
}

void TRTC::onSwitchRoom(int errCode, QString errMsg)
{
	qDebug() << QString("onSwitchRoom errorCode[%1], errorInfo[%2]").arg(errCode).arg(errMsg);

	if (errCode == ERR_NULL)
	{
		LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
		qDebug() << QString("switch room success, room id: %1, user id: %2").arg(info._roomId).arg(QString::fromStdString(info._userId));
	}
}

void TRTC::onRemoteUserEnter(QString userId)
{
	CDataCenter::GetInstance()->addRemoteUser(userId.toStdString());
}

void TRTC::onRemoteUserLeave(QString userId)
{
	qDebug() << QString(u8"%1离开房间").arg(userId);
	std::string idstr = userId.toStdString();
	if (TRTCCloudCore::GetInstance()->getTRTCCloud())
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(idstr.c_str());

	CDataCenter::GetInstance()->removeRemoteUser(idstr);
}

void TRTC::onConnectOtherRoom(QString userId, int errCode, QString errMsg)
{
	qDebug() << (errCode == 0 ? QString(u8"连麦成功:[room:%1, user:%2]").arg(m_pkRoomId).arg(userId) : QString(u8"连麦失败[userId:%1, roomId:%2, errCode:%3, msg:%4]").arg(userId).arg(m_pkRoomId).arg(errCode).arg(errMsg));
}

void TRTC::onDisconnectOtherRoom(int errCode, QString errMsg)
{
	qDebug() << (errCode == 0 ? QString(u8"取消连麦成功[msg:%s]").arg(errMsg) : QString(u8"取消连麦失败:[errCode:%d, msg:%s]").arg(errCode).arg(errMsg));
}

QINIURTC::QINIURTC()
{
	m_rtc = new QNRtc(this);
	connect(
		m_rtc, &QNRtc::linkStateResult, this, [=](QNRtc::ResultCode code, int erorrCode, QString errorStr) {
		switch (code)
		{
		case QNRtc::JoinSucess:
		case QNRtc::SubscribeSucess:
		case QNRtc::PublishSucess:
		{
			if (code == QNRtc::JoinSucess)
			{
				m_joinSucess = true;
			}
			else if (code == QNRtc::SubscribeSucess)
			{
				m_subscibeSucess = true;
			}
			else if (code == QNRtc::PublishSucess)
			{
				m_publishSucess = true;
			}

			if (m_joinSucess && m_subscibeSucess && m_publishSucess)
				m_rtc->doLinkMerge();
		}
		break;
		case QNRtc::MergeSucess:
			emit onEvent(RTC_EVENT_SUCCESS, QJsonObject());
		break;
		case QNRtc::Failture:
		{
			QJsonObject obj;
			obj["errCode"] = erorrCode;
			obj["errMsg"] = errorStr;
			obj["isNetFail"] = true;
			emit onEvent(RTC_EVENT_FAIL, obj);
		}
		break;
		case QNRtc::ReConnect:
			emit onEvent(RTC_EVENT_RECONNECTING, QJsonObject());
		break;
		case QNRtc::ReJoin:
			emit onEvent(RTC_EVENT_REJOIN, QJsonObject());
		break;
		case QNRtc::JoinFailture:
		{
			QJsonObject obj;
			obj["errCode"] = erorrCode;
			obj["errMsg"] = errorStr;
			obj["isNetFail"] = false;
			emit onEvent(RTC_EVENT_FAIL, obj);
		}
		break;
		default:
			break;
		}
	});
}

 QINIURTC::~QINIURTC() {}

void QINIURTC::init() {}

void QINIURTC::enterRoom()
{
	m_rtc->SetVideoInfo(videoInfo().audioBitrate, videoInfo().videoBitrate, videoInfo().fps);
	m_rtc->setCropInfo(cropInfo().x(), 0, 724, 1080);
	m_rtc->setUserId(linkInfo().value("userId").toString());
	m_rtc->setPushUrl(linkInfo().value("pushUrl").toString());
	m_rtc->startLink(linkInfo().value("token").toString());
}

void QINIURTC::exitRoom()
{
	m_rtc->stopLink();
	bool m_joinSucess = false;
	bool m_subscibeSucess = false;
	bool m_publishSucess = false;
}

void QINIURTC::switchRoom(int roomId) {}

void QINIURTC::connectOtherRoom(QString userId, int roomId) {}

void QINIURTC::disconnectOtherRoom() {}

void QINIURTC::setRemoteViewHwnd(long window)
{
	m_rtc->setRenderWidget((HWND)window);
}

void QINIURTC::sendAudio(struct audio_data *data)
{
	m_rtc->PushExternalAudioData(data->data[0], data->frames);
}

void QINIURTC::sendVideo(struct video_data *data)
{
	m_rtc->PushExternalVideoData(data->data[0], data->timestamp);
}
