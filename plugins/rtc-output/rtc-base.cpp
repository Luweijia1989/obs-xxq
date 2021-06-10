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
	m_qosParams.preference = TRTCVideoQosPreferenceClear;
	m_qosParams.controlMode = TRTCQosControlModeServer;
	m_sceneParams = TRTCAppSceneLIVE;
	m_roleType = TRTCRoleAnchor;

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
					bool isTencentCdn = cloudMixInfo.cdnSupplier == "TENCENT";
					if (isTencentCdn)
						sendEvent(RTC_EVENT_SUCCESS, QJsonObject());
					else
					{
						TRTCPublishCDNParam p;
						p.appId = cloudMixInfo.cdnAppId;
						p.bizId = cloudMixInfo.cdnBizId;
						std::string str = cloudMixInfo.streamUrl.toStdString();
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
	m_mixUsers.clear();

	MixUserInfo self;
	self.isSelf = true;
	self.userId = rtcEnterInfo.userId.toStdString();
	self.roomId = QString::number(rtcEnterInfo.roomId).toStdString();
	self.audioAvailable = true;
	m_mixUsers.append(self);

	m_linkTimeout.start();

	m_hasMixStream = false;
	init();

	//设置连接环境
	std::string cmd = QString("{\"api\": \"setNetEnv\",\"params\" :{\"env\" : 0}}").toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(cmd.c_str());

	std::string cmd2 = QString("{\"api\":\"setSEIPayloadType\",\"params\":{\"payloadType\":5}}").toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(cmd2.c_str());

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setDefaultStreamRecvMode(true, true);

	internalEnterRoom();

	std::string encodeParam =
		QString("{\"api\":\"setVideoEncodeParamEx\",\"params\":{\"codecType\":0,\"softwareCodecParams\":{\"profile\":1,\"enableRealTime\":1},\"videoWidth\":%1,\"videoHeight\":%2,\"videoFps\":%3,\"videoBitrate\":%4,\"streamType\":%5,\"rcMethod\":1}}")
			.arg(videoEncodeInfo.width)
			.arg(videoEncodeInfo.height)
			.arg(videoEncodeInfo.fps)
			.arg(videoEncodeInfo.bitrate)
			.arg(TRTCVideoStreamTypeBig)
			.toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(encodeParam.c_str());

	TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderMirror(false);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setNetworkQosParam(m_qosParams);

	TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalPreview();
	if (rtcEnterInfo.videoOnly)
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

	qDebug() << QString(u8"正在进入[%1]房间, sceneType[%3]").arg(rtcEnterInfo.roomId).arg(u8"视频互动直播");
}

void TRTC::exitRoom()
{
	m_bStartCustomCapture = false;
	TRTCCloudCore::GetInstance()->PreUninit(rtcEnterInfo.videoOnly);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->exitRoom();

	if (!m_bIsEnteredRoom)
	{
		onExitRoom();
		return;
	}

	QTimer timer;
	connect(&timer, &QTimer::timeout, &m_exitRoomLoop, &QEventLoop::quit);
	timer.start(1000);
	m_exitRoomLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

void TRTC::sendAudio(struct audio_data *data)
{
	if (!m_bStartCustomCapture || !rtcEnterInfo.videoOnly)
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
	if (!m_bStartCustomCapture || !rtcEnterInfo.videoOnly)
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
	if (!rtcEnterInfo.videoOnly)
		return;

	m_seiData = QJsonDocument(data).toJson(QJsonDocument::Compact);
	m_seiData = m_uuid + m_seiData;
	TRTCCloudCore::GetInstance()->getTRTCCloud()->sendSEIMsg((uint8_t *)m_seiData.data(), m_seiData.size(), insetType);
}

void TRTC::setAudioInputDevice(const QString &deviceId)
{
	if (rtcEnterInfo.videoOnly)
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
	if (rtcEnterInfo.videoOnly)
		return;

	TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(mute);
}

void TRTC::setAudioInputVolume(int volume)
{
	if (rtcEnterInfo.videoOnly)
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

void TRTC::connectOtherRoom(const QString &userId, int roomId)
{
	MixUserInfo remoteAnchor;
	remoteAnchor.userId = userId.toStdString();
	remoteAnchor.roomId = QString::number(roomId).toStdString();
	remoteAnchor.audioAvailable = false;
	m_mixUsers.append(remoteAnchor);

	m_remoteAnchorInfo.first = userId;
	m_remoteAnchorInfo.second = roomId;
	TRTCCloudCore::GetInstance()->connectOtherRoom(userId, roomId);
}

void TRTC::disconnectOtherRoom()
{
	TRTCCloudCore::GetInstance()->disconnectOtherRoom();
}

void TRTC::muteRemoteAnchor(bool mute)
{
	for (auto iter = m_mixUsers.begin(); iter != m_mixUsers.end(); iter++)
	{
		MixUserInfo &user = *iter;
		if (QString::fromStdString(user.userId) != m_remoteAnchorInfo.first)
			continue;

		user.mute = mute;
	}

	TRTCCloudCore::GetInstance()->updateCloudMixStream(cloudMixInfo, m_mixUsers);
	std::string ud = m_remoteAnchorInfo.first.toStdString();
	TRTCCloudCore::GetInstance()->getTRTCCloud()->muteRemoteAudio(ud.c_str(), mute);
}

void TRTC::internalEnterRoom()
{
	//进入房间
	TRTCParams params;
	params.sdkAppId = rtcEnterInfo.appId;
	params.roomId = rtcEnterInfo.roomId;
	std::string sid = rtcEnterInfo.userId.toStdString();
	std::string ssig = rtcEnterInfo.userSig.toStdString();
	params.userId = sid.c_str();
	params.userSig = ssig.c_str();
	std::string privMapEncrypt = "";
	params.privateMapKey = (char*)privMapEncrypt.c_str();
	params.role = m_roleType;
	std::string std_streamId = cloudMixInfo.streamId.toStdString();
	if (rtcEnterInfo.videoOnly && cloudMixInfo.cdnSupplier == "TENCENT")
		params.streamId = std_streamId.c_str();

	// TRTCCloudCore::GetInstance()->getTRTCCloud()->setEncodedDataProcessingListener();
	char api_str[128] = { 0 };
	sprintf_s(api_str, 128, "{\"api\":\"setEncodedDataProcessingListener\", \"params\": {\"listener\":%llu}}", (uint64_t)0);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI(api_str);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->enterRoom(params, m_sceneParams);
}

void TRTC::onEnterRoom(int result)
{
	if (result >= 0)
	{
		m_bIsEnteredRoom = true;
		qDebug() << QString(u8"进入[%1]房间成功,耗时:%2ms").arg(rtcEnterInfo.roomId).arg(result);

		m_bStartCustomCapture = true;
		if (rtcEnterInfo.videoOnly)
		{ 
			TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(false);
			TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(false);
			TRTCCloudCore::GetInstance()->updateCloudMixStream(cloudMixInfo, m_mixUsers);
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
	TRTCCloudCore::GetInstance()->Uninit();
	m_bIsEnteredRoom = false;
	m_exitRoomLoop.quit();
}

void TRTC::onUserAudioAvailable(QString userId, bool available)
{
	qDebug() << QString("[%1]onAudioAvailable : %2").arg(userId).arg(available);

	auto iter = m_mixUsers.begin();
	for (; iter != m_mixUsers.end(); iter++)
	{
		MixUserInfo &user = *iter;
		if (QString::fromStdString(user.userId) == userId)
		{
			user.audioAvailable = available;
			break;
		}
	}

	if (iter == m_mixUsers.end())
	{
		MixUserInfo user;
		user.audioAvailable = available;
		user.userId = userId.toStdString();
		user.roomId = QString::number(rtcEnterInfo.roomId).toStdString();
		m_mixUsers.append(user);
	}
	 
	if (!cloudMixInfo.usePresetLayout)
		TRTCCloudCore::GetInstance()->updateCloudMixStream(cloudMixInfo, m_mixUsers);
}

void TRTC::onUserVideoAvailable(QString userId, bool available)
{
	qDebug() << QString(u8"[%1]onVideoAvailable : %2").arg(userId).arg(available);

	if (!remoteUsers.contains(userId))
		return;

	std::string sid = userId.toStdString();
	const RemoteUser &user = remoteUsers[userId];
	TRTCRenderParams renderParam;
	renderParam.fillMode = TRTCVideoFillMode_Fill;
	renderParam.mirrorType = TRTCVideoMirrorType_Disable;
	renderParam.rotation = TRTCVideoRotation0;
	TRTCCloudCore::GetInstance()->getTRTCCloud()->setRemoteRenderParams(sid.c_str(), TRTCVideoStreamTypeBig, renderParam);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteView(sid.c_str(), TRTCVideoStreamTypeBig, (HWND)user.renderView);
}

void TRTC::onRemoteUserEnter(QString userId)
{
	qDebug() << QString(u8"%1进入房间").arg(userId);
}

void TRTC::onRemoteUserLeave(QString userId)
{
	qDebug() << QString(u8"%1离开房间").arg(userId);
	std::string idstr = userId.toStdString();
	if (TRTCCloudCore::GetInstance()->getTRTCCloud())
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(idstr.c_str(), TRTCVideoStreamTypeBig);
}
