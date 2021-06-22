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
				TRTCCloudCore::GetInstance()->updateCloudMixStream(cloudMixInfo, m_roomUsers);
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
					sendEvent(RTC_EVENT_SUCCESS, QJsonObject());
				}
				else
					sendEvent(RTC_EVENT_FAIL, data);
			}
		}
		else if (type == RTC_EVENT_USER_VOLUME)
		{
			onSpeakerEvent(data);
		}
		else if (type == RTC_EVENT_CMD_MSG)
		{
			QJsonArray arr = data["cmd"].toArray();
			for (int i = 0; i < arr.size(); i++)
			{
				QJsonObject one = arr.at(i).toObject();

				auto userId = one["userId"].toString();
				if (m_roomUsers.contains(userId))
				{
					RoomUser &user = m_roomUsers[userId];
					user.uid = one["uid"].toString();
					user.roomId = one["roomId"].toInt();
					user.isAnchor = one["isAnchor"].toBool();
					user.isCross = one["isCross"].toBool();
				}
			}
			updateRoomUsers();
		}
		else if (type == RTC_EVENT_CONNECT_OTHER_ROOM)
		{
			data["rtcRoomId"] = rtcEnterInfo.roomId;
			data["rtcCrossRoomId"] = m_rtcCrossRoomId;
			sendEvent(RTC_EVENT_CONNECT_OTHER_ROOM, data);
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
	m_roomUsers.insert(rtcEnterInfo.userId, {true, rtcEnterInfo.uid, rtcEnterInfo.userId, -1, rtcEnterInfo.roomId, rtcEnterInfo.userId.toStdString(), QString::number(rtcEnterInfo.roomId).toStdString(), true, false, true, false});

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
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopAllRemoteView();
		TRTCCloudCore::GetInstance()->getTRTCCloud()->enableAudioVolumeEvaluation(3000);
		TRTCCloudCore::GetInstance()->getTRTCCloud()->startLocalAudio(TRTCAudioQualityMusic);
	}

	qDebug() << QString(u8"正在进入[%1]房间, sceneType[%3]").arg(rtcEnterInfo.roomId).arg(u8"视频互动直播");
}

void TRTC::exitRoom()
{
	m_roomUsers.clear();

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

void TRTC::connectOtherRoom(const QString &userId, int roomId, const QString &uid, bool selfDoConnect)
{
	m_rtcCrossRoomId = roomId;
	m_roomUsers.insert(userId, {false, uid, userId, -1, roomId, userId.toStdString(), QString::number(roomId).toStdString(), true, true, false, false});

	if (selfDoConnect)
		TRTCCloudCore::GetInstance()->connectOtherRoom(userId, roomId);
}

void TRTC::disconnectOtherRoom()
{
	TRTCCloudCore::GetInstance()->disconnectOtherRoom();
}

void TRTC::muteRemoteAnchor(bool mute)
{
	for (auto iter = m_roomUsers.begin(); iter != m_roomUsers.end(); iter++)
	{
		RoomUser &user = iter.value();
		if (user.isCross && user.isAnchor)
		{
			user.mute = mute;
			TRTCCloudCore::GetInstance()->getTRTCCloud()->muteRemoteAudio(user.stdUserId.c_str(), mute);
			break;
		}
	}

	if (!cloudMixInfo.usePresetLayout)
		TRTCCloudCore::GetInstance()->updateCloudMixStream(cloudMixInfo, m_roomUsers);
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
			
			bool isTencentCdn = cloudMixInfo.cdnSupplier == "TENCENT";
			if (isTencentCdn)
				TRTCCloudCore::GetInstance()->updateCloudMixStream(cloudMixInfo, m_roomUsers);
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

	if (m_roomUsers.contains(userId))
	{
		RoomUser &user = m_roomUsers[userId];
		user.audioAvailable = available;
	}
	 
	if (!cloudMixInfo.usePresetLayout)
		TRTCCloudCore::GetInstance()->updateCloudMixStream(cloudMixInfo, m_roomUsers);
}

void TRTC::onUserVideoAvailable(QString userId, bool available)
{
	qDebug() << QString(u8"[%1]onVideoAvailable : %2").arg(userId).arg(available);

	if (!m_roomUsers.contains(userId))
		return;

	const RoomUser &user = m_roomUsers[userId];
	if (user.renderView == -1)
		return;

	std::string sid = userId.toStdString();
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

	if (!m_roomUsers.contains(userId))
		m_roomUsers.insert(userId, {false, "", userId, -1, rtcEnterInfo.roomId, userId.toStdString(), QString::number(rtcEnterInfo.roomId).toStdString(), false, false, false, false});

	for (auto iter = m_roomUsers.begin(); iter != m_roomUsers.end(); iter++)
	{
		const RoomUser &user = iter.value();
		if (user.isCross && user.isAnchor)
		{
			QJsonObject data;
			data["userId"] = userId;
			data["errCode"] = 0;
			data["errMsg"] = "";
			data["rtcRoomId"] = rtcEnterInfo.roomId;
			data["rtcCrossRoomId"] = m_rtcCrossRoomId;
			sendEvent(RTC_EVENT_CONNECT_OTHER_ROOM, data);
			break;
		}
	}
}

void TRTC::onRemoteUserLeave(QString userId)
{
	qDebug() << QString(u8"%1离开房间").arg(userId);
	if (m_roomUsers.contains(userId))
		m_roomUsers.remove(userId);

	updateRoomUsers();

	std::string idstr = userId.toStdString();
	if (TRTCCloudCore::GetInstance()->getTRTCCloud())
		TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(idstr.c_str(), TRTCVideoStreamTypeBig);
}

void TRTC::updateRoomUsers()
{
	QJsonArray arr;
	for (auto iter = m_roomUsers.begin(); iter != m_roomUsers.end(); iter++)
	{
		const RoomUser &user = iter.value();
		QJsonObject obj;
		obj["uid"] = user.uid;
		obj["userId"] = user.userId;
		obj["roomId"] = user.roomId;
		obj["isAnchor"] = user.isAnchor;
		obj["isCross"] = user.isCross;
		arr.append(obj);
	}

	QJsonDocument jd(arr);
	auto data = jd.toJson(QJsonDocument::Compact);
	TRTCCloudCore::GetInstance()->getTRTCCloud()->sendCustomCmdMsg(1, (const uint8_t *)data.data(), data.size(), true, true);
}
