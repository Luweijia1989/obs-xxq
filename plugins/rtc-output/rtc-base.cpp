#include "rtc-base.h"
#include "trtc/TRTCCloudCore.h"
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
	m_linkTimeout.setSingleShot(true);
	m_linkTimeout.setInterval(30000);
	connect(&m_linkTimeout, &QTimer::timeout, this, [=](){
		QJsonObject obj;
		obj["errCode"] = -1;
		obj["errMsg"] = "timeout";
		sendEvent(RTC_EVENT_FAIL, obj);
	});
	auto str = QUuid::createUuid().toString(QUuid::Id128);
	m_uuid = hexstrTobyte(str);

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
			if (!m_hasMixStream)
			{
				if (err == 0)
				{
					m_hasMixStream = true;
					bool isTencentCdn = link_cdnSupplier == "TENCENT";
					if (isTencentCdn)
						sendEvent(RTC_EVENT_SUCCESS, QJsonObject());
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
				else
					sendEvent(RTC_EVENT_FAIL, data);
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
	m_linkTimeout.start();

	m_hasMixStream = false;
	init();

	//设置连接环境
	std::string cmd = QString("{\"api\": \"setNetEnv\",\"params\" :{\"env\" : %1}}").arg(CDataCenter::GetInstance()->m_nLinkTestServer).toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(cmd.c_str());

	std::string cmd2 = QString("{\"api\":\"setSEIPayloadType\",\"params\":{\"payloadType\":5}}").toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(cmd2.c_str());

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setDefaultStreamRecvMode(true, true);

	internalEnterRoom();

	if (videoInfo().mode == 0) // 0->单主播 1->主播加观众
	{
		CDataCenter::GetInstance()->m_videoEncParams.videoBitrate = videoInfo().videoBitrate;
		CDataCenter::GetInstance()->m_videoEncParams.videoFps = videoInfo().fps;
		CDataCenter::GetInstance()->m_videoEncParams.minVideoBitrate = videoInfo().videoBitrate;
		if (videoInfo().width == 1920)
			CDataCenter::GetInstance()->m_videoEncParams.videoResolution = TRTCVideoResolution_1920_1080;
		else if (videoInfo().width == 1280)
			CDataCenter::GetInstance()->m_videoEncParams.videoResolution = TRTCVideoResolution_1280_720;
		CDataCenter::GetInstance()->m_videoEncParams.resMode = TRTCVideoResolutionModeLandscape;
	}
	else if (videoInfo().mode == 1)
	{
		//默认的就是主播加观众的设置，不用调整了。
	}

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderParam(CDataCenter::GetInstance()->m_videoEncParams);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderMirror(false);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setNetworkQosParam(CDataCenter::GetInstance()->m_qosParams);

	TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalPreview();
	if (is_video_link)
	{
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalAudio();
		TRTCCloudCore::GetInstance()->getTRTCCloud()->enableCustomAudioCapture(true);
		TRTCCloudCore::GetInstance()->getTRTCCloud()->enableCustomVideoCapture(true);
	}
	else
	{
		TRTCCloudCore::GetInstance()->getTRTCCloud()->enableAudioVolumeEvaluation(3000);
		TRTCCloudCore::GetInstance()->getTRTCCloud()->startLocalAudio(TRTCAudioQualityMusic);
	}

	qDebug() << QString(u8"正在进入[%1]房间, sceneType[%3]").arg(link_rtcRoomId).arg(u8"视频互动直播");
}

void TRTC::exitRoom()
{
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
	TRTCCloudCore::GetInstance()->getTRTCCloud()->sendSEIMsg((uint8_t *)m_seiData.data(), m_seiData.size(), insetType);
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
	std::string strdid = deviceId.toStdString();
	if (last_audio_input_device != deviceId)
	{
		auto devices = TRTCCloudCore::GetInstance()->deviceManager()->getDevicesList(TXMediaDeviceTypeMic);
		if (!devices)
			return;
		int count = devices->getCount();
		if (count > 0)
		{
			if (deviceId == "default") {
				TRTCCloudCore::GetInstance()->deviceManager()->setCurrentDevice(TXMediaDeviceTypeMic, devices->getDevicePID(0));
			}
			else
			{
				for (int i=0; i<count; i++)
				{
					if (strcmp(strdid.c_str(), devices->getDevicePID(i)) == 0)
					{
						TRTCCloudCore::GetInstance()->deviceManager()->setCurrentDevice(TXMediaDeviceTypeMic, devices->getDevicePID(i));
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
	std::string strdid = deviceId.toStdString();
	auto devices = TRTCCloudCore::GetInstance()->deviceManager()->getDevicesList(TXMediaDeviceTypeSpeaker);
	if (!devices)
		return;

	int count = devices->getCount();
	if (count > 0)
	{
		if (deviceId == "default") {
			TRTCCloudCore::GetInstance()->deviceManager()->setCurrentDevice(TXMediaDeviceTypeSpeaker, devices->getDevicePID(0));
		}
		else
		{
			for (int i = 0; i < count; i++)
			{
				if (strcmp(strdid.c_str(), devices->getDevicePID(i)) == 0)
				{
					TRTCCloudCore::GetInstance()->deviceManager()->setCurrentDevice(TXMediaDeviceTypeSpeaker, devices->getDevicePID(i));
					break;
				}
			}
		}
	}
	devices->release();
}

void TRTC::stopTimeoutTimer()
{
	QMetaObject::invokeMethod(&m_linkTimeout, "stop", Qt::QueuedConnection);
}

uint64_t TRTC::getTotalBytes()
{
	return TRTCCloudCore::GetInstance()->getSentBytes();
}

void TRTC::startRecord(const QString &path)
{
	trtc::TRTCLocalRecordingParams p;
	std::string pp = path.toStdString();
	p.filePath = pp.c_str();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->startLocalRecording(p);
}

void TRTC::stopRecord()
{
	TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalRecording();
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
	std::string std_streamId = link_streamId.toStdString();
	if (is_video_link && link_cdnSupplier == "TENCENT")
		params.streamId = std_streamId.c_str();

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

		m_bStartCustomCapture = true;
		std::string strOtherUid = link_rtc_otherUid.toStdString();
		if (is_video_link)
		{ 
			TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(false);
			TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(false);
			MixInfo info;
			info.onlyAnchorVideo = videoInfo().mode == 0;
			if (videoInfo().mode == 0)
			{
				info.width = videoInfo().width;
				info.height = videoInfo().height;
				info.vbitrate = videoInfo().videoBitrate;
				info.fps = videoInfo().fps;
				info.mixerCount = 2;
			}
			else
			{
				info.width = 750;
				info.height = 564;
				info.vbitrate = 850;
				info.fps = 20;
				info.mixerCount = 2;

				TRTCRenderParams renderParam;
				renderParam.fillMode = TRTCVideoFillMode_Fill;
				renderParam.mirrorType = TRTCVideoMirrorType_Disable;
				renderParam.rotation = TRTCVideoRotation0;
				TRTCCloudCore::GetInstance()->getTRTCCloud()->setRemoteRenderParams(strOtherUid.c_str(), TRTCVideoStreamTypeBig, renderParam);
				TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteView(strOtherUid.c_str(), TRTCVideoStreamTypeBig, (HWND)m_remoteView);
			}
			TRTCCloudCore::GetInstance()->startCloudMixStream(link_std_rtcRoomId.c_str(), link_cdnAPPID, link_cdnBizID, info);
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
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(idstr.c_str(), TRTCVideoStreamTypeBig);
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
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(idstr.c_str(), TRTCVideoStreamTypeBig);

	CDataCenter::GetInstance()->removeRemoteUser(idstr);
}
