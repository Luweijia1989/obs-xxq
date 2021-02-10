#include "rtc-base.h"
#include "trtc/TRTCCloudCore.h"
#include "qnrtc/qiniurtc.h"
#include "TXLiteAVBase.h"
#include "rtc-output.h"
#include <QDebug>
#include <QTimer>
#include <QUuid>

QByteArray hexstrTobyte(QString str)
{
    QByteArray byte_arr;
    bool ok;
    int len=str.size();
    for(int i=0;i<len;i+=2){
         byte_arr.append(char(str.mid(i,2).toUShort(&ok,16)));
    }
    return byte_arr;
}

TRTC::TRTC()
{
	auto str = QUuid::createUuid().toString(QUuid::Id128);
	m_uuid = hexstrTobyte(str);

	m_seiTimer.setInterval(50);
	m_seiTimer.setSingleShot(false);

	connect(&m_seiTimer, &QTimer::timeout, this, [=](){
		TRTCCloudCore::GetInstance()->getTRTCCloud()->sendSEIMsg((uint8_t *)m_seiData.data(), m_seiData.size(), 1);
	});

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
		else if (type == RTC_EVENT_REMOTE_USER_ENTER)
		{
			onRemoteUserEnter(data["userId"].toString());
		}
		else if (type == RTC_EVENT_REMOTE_USER_LEAVE)
		{
			onRemoteUserLeave(data["userId"].toString());
		}
		else if (type == RTC_EVENT_USER_VIDEO_AVAILABLE)
		{
			onUserVideoAvailable(data["userId"].toString(), data["available"].toBool());
		}
		else if (type == RTC_EVENT_PUBLISH_CDN)
		{
			int err = data["errCode"].toInt();
			data["isNetFail"] = false;
			if (err != 0)
				sendEvent(RTC_EVENT_FAIL, data);
			else
				sendEvent(RTC_EVENT_SUCCESS, QJsonObject());
		}
		else if (type == RTC_EVENT_FIRST_VIDEO)
		{
		}
		else if (type == RTC_EVENT_MIX_STREAM)
		{
			int err = data["errCode"].toInt();
			if (err == 0 && !m_hasMixStream)
			{
				m_hasMixStream = true;
				bool isTencentCdn = link_cdnSupplier == "TENCENT";
				if (isTencentCdn)
				{
					std::string sid = link_streamId.toStdString();
					TRTCCloudCore::GetInstance()->getTRTCCloud()->startPublishing(sid.c_str(), TRTCVideoStreamTypeBig);
				}
				else
				{
					TRTCPublishCDNParam p;
					p.appId = link_cdnAPPID;
					p.bizId = link_cdnBizID;
					std::string str = link_streamUrl.toStdString();
					p.url = str.c_str();
					TRTCCloudCore::GetInstance()->getTRTCCloud()->startPublishCDNStream(p);
				}
			}
		}
		else if (type == RTC_EVENT_USER_VOLUME)
		{
			onSpeakerEvent(data);
		}
	}, Qt::DirectConnection);
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
	m_hasMixStream = false;
	init();

	//设置连接环境
	std::string cmd = QString("{\"api\": \"setNetEnv\",\"params\" :{\"env\" : %1}}").arg(CDataCenter::GetInstance()->m_nLinkTestServer).toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(cmd.c_str());

	std::string cmd2 = QString("{\"api\":\"setSEIPayloadType\",\"params\":{\"payloadType\":5}}").toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(cmd2.c_str());

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setDefaultStreamRecvMode(true, true);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setAudioQuality(TRTCAudioQualityMusic);

	internalEnterRoom();

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderParam(CDataCenter::GetInstance()->m_videoEncParams);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderMirror(false);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setNetworkQosParam(CDataCenter::GetInstance()->m_qosParams);

	TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalPreview();
	if (is_video_link)
	{
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalAudio();
		m_bStartCustomCapture = true;
		TRTCCloudCore::GetInstance()->getTRTCCloud()->enableCustomAudioCapture(true);
		TRTCCloudCore::GetInstance()->getTRTCCloud()->enableCustomVideoCapture(true);
	}
	else
	{
		TRTCCloudCore::GetInstance()->getTRTCCloud()->enableAudioVolumeEvaluation(3000);
		TRTCCloudCore::GetInstance()->getTRTCCloud()->startLocalAudio();
	}

	qDebug() << QString(u8"正在进入[%1]房间, sceneType[%3]").arg(link_rtcRoomId).arg(u8"视频互动直播");
}

void TRTC::exitRoom()
{
	if (m_seiTimer.isActive())
		m_seiTimer.stop();
	m_bStartCustomCapture = false;
	TRTCCloudCore::GetInstance()->PreUninit(is_video_link);
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

void TRTC::setRemoteViewHwnd(long window)
{
	m_remoteView = window;
}

void TRTC::sendAudio(struct audio_data *data)
{
	if (!m_bStartCustomCapture || !is_video_link)
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
	if (!m_bStartCustomCapture || !is_video_link)
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

void TRTC::setSei(const QJsonObject &data, int insetType)
{
	if (!is_video_link)
		return;

	m_seiData = QJsonDocument(data).toJson(QJsonDocument::Compact);
	m_seiData = m_uuid + m_seiData;
	if (!m_seiTimer.isActive())
		m_seiTimer.start();
}

void TRTC::setAudioInputDevice(const QString &deviceId)
{
	if (is_video_link)
		return;

	if (deviceId == "disabled")
	{
		setAudioInputMute(true);
		return;
	}

	setAudioInputMute(false);
	std:string strdid = deviceId.toStdString();
	if (last_audio_input_device != deviceId)
	{
		auto devices = TRTCCloudCore::GetInstance()->getTRTCCloud()->getMicDevicesList();
		int count = devices->getCount();
		if (count > 0)
		{
			if (deviceId == "default") {
				TRTCCloudCore::GetInstance()->getTRTCCloud()->setCurrentMicDevice(devices->getDevicePID(0));
			}
			else
			{
				for (int i=0; i<count; i++)
				{
					if (strcmp(strdid.c_str(), devices->getDevicePID(i)) == 0)
					{
						TRTCCloudCore::GetInstance()->getTRTCCloud()->setCurrentMicDevice(devices->getDevicePID(i));
						break;
					}
				}
			}
		}
		devices->release();

		last_audio_input_device = deviceId;
	}
}

void TRTC::setAudioInputMute(bool mute)
{
	if (is_video_link)
		return;

	TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(mute);
}

void TRTC::setAudioInputVolume(int volume)
{
	if (is_video_link)
		return;

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setAudioCaptureVolume(volume);
}

void TRTC::setAudioOutputDevice(const QString &deviceId)
{
	std:string strdid = deviceId.toStdString();
	auto devices = TRTCCloudCore::GetInstance()->getTRTCCloud()->getSpeakerDevicesList();
	int count = devices->getCount();
	if (count > 0)
	{
		if (deviceId == "default") {
			TRTCCloudCore::GetInstance()->getTRTCCloud()->setCurrentSpeakerDevice(devices->getDevicePID(0));
		}
		else
		{
			for (int i = 0; i < count; i++)
			{
				if (strcmp(strdid.c_str(), devices->getDevicePID(i)) == 0)
				{
					TRTCCloudCore::GetInstance()->getTRTCCloud()->setCurrentSpeakerDevice(devices->getDevicePID(i));
					break;
				}
			}
		}
	}
	devices->release();
}

void TRTC::internalEnterRoom()
{
	//进入房间
	TRTCParams params;
	params.sdkAppId = link_rtcAPPID;
	params.roomId = link_rtcRoomId.toInt();
	params.userId = link_std_rtcUid.c_str();
	params.userSig = link_std_rtcRoomToken.c_str();
	std::string privMapEncrypt = "";
	params.privateMapKey = (char*)privMapEncrypt.c_str();
	params.role = CDataCenter::GetInstance()->m_roleType;

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
		qDebug() << QString(u8"进入[%1]房间成功,耗时:%2ms").arg(link_rtcRoomId).arg(result);

		std::string strOtherUid = link_rtc_otherUid.toStdString();
		if (is_video_link)
		{ 
			TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(false);
			TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(false);
			TRTCCloudCore::GetInstance()->getTRTCCloud()->setRemoteViewFillMode(strOtherUid.c_str(), TRTCVideoFillMode_Fill);
			TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteView(strOtherUid.c_str(), (HWND)m_remoteView);
			TRTCCloudCore::GetInstance()->startCloudMixStream(link_std_rtcRoomId.c_str(), link_cdnAPPID, link_cdnBizID);
		}
		else
			sendEvent(RTC_EVENT_SUCCESS, QJsonObject());
	}
	else
	{
		qDebug() << QString(u8"进房失败，ErrorCode:%1").arg(result);
		sendEvent(RTC_EVENT_REJOIN, QJsonObject());
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
			{
				if (is_video_link)
					m_rtc->doLinkMerge(link_streamUrl);
				else
				{
					sendEvent(RTC_EVENT_SUCCESS, QJsonObject());
					m_rtc->startSpeakTimer();
				}
			}
		}
		break;
		case QNRtc::MergeSucess:
			sendEvent(RTC_EVENT_SUCCESS, QJsonObject());
		break;
		case QNRtc::Failture:
		{
			QJsonObject obj;
			obj["errCode"] = erorrCode;
			obj["errMsg"] = errorStr;
			obj["isNetFail"] = true;
			sendEvent(RTC_EVENT_FAIL, obj);
		}
		break;
		case QNRtc::ReConnect:
			sendEvent(RTC_EVENT_RECONNECTING, QJsonObject());
		break;
		case QNRtc::ReJoin:
			sendEvent(RTC_EVENT_REJOIN, QJsonObject());
		break;
		case QNRtc::JoinFailture:
		{
			QJsonObject obj;
			obj["errCode"] = erorrCode;
			obj["errMsg"] = errorStr;
			obj["isNetFail"] = false;
			sendEvent(RTC_EVENT_FAIL, obj);
		}
		break;
		default:
			break;
		}
	});

	connect(m_rtc, &QNRtc::speakerEvent, this, &QINIURTC::onSpeakerEvent);
}

 QINIURTC::~QINIURTC() {}

void QINIURTC::init() {}

void QINIURTC::enterRoom()
{
	m_rtc->setIsVideoLink(is_video_link);
	m_rtc->SetVideoInfo(videoInfo().audioBitrate, videoInfo().videoBitrate, videoInfo().fps, videoInfo().width, videoInfo().height);
	m_rtc->setCropInfo(cropInfo().x(), 0, cropInfo().width(), cropInfo().height());
	m_rtc->setUserId(link_rtc_uid);
	m_rtc->startLink(link_rtcRoomToken);
}

void QINIURTC::exitRoom()
{
	m_rtc->stopLink();
	m_joinSucess = false;
	m_subscibeSucess = false;
	m_publishSucess = false;
}

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

void QINIURTC::setSei(const QJsonObject &data, int insetType)
{
	m_rtc->setSei(data, insetType);
}

void QINIURTC::setAudioInputDevice(const QString &deviceId)
{
	if (is_video_link)
		return;
	m_rtc->setMicDevice(deviceId);
}

void QINIURTC::setAudioInputMute(bool mute)
{
	if (is_video_link)
		return;
	m_rtc->setMicMute(mute);
}

void QINIURTC::setAudioInputVolume(int volume)
{
	if (is_video_link)
		return;
	m_rtc->setMicVolume(volume);
}

void QINIURTC::setAudioOutputDevice(const QString &deviceId)
{
	m_rtc->setPlayoutDevice(deviceId);
}
