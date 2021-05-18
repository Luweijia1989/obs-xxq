#include "qiniurtc.h"
#include "QNErrorCode.h"
#include <QDebug>
#include <QJsonDocument>
#define EXTERNAL_TAG "external"

extern "C" QINIU_EXPORT_DLL int GetRoomToken_s(
	const std::string& app_id_,
	const std::string& room_name_,
	const std::string& user_id_,
	const std::string& host_name_,
	const int time_out_,
	std::string& token_);

#define VOLUMEMAX 32767
#define VOLUMEMIN -32768

#ifndef core_min
#define core_min(a, b) ((a) < (b) ? (a) : (b))
#endif

// 获取音频分贝值
static uint32_t ProcessAudioLevel(const int16_t* data, const int32_t& data_size)
{
	uint32_t ret = 0;

	if (data_size > 0) {
		int32_t sum = 0;
		int16_t* pos = (int16_t *)data;
		for (int i = 0; i < data_size; i++) {
			sum += abs(*pos);
			pos++;
		}

		ret = sum * 500.0 / (data_size * VOLUMEMAX);
		ret = core_min(ret, 100);
	}

	return ret;
}

unsigned long long QNRtc::s_startTimeStamp = 0;
QNRtc::QNRtc(QObject *parent)
    : QObject(parent)
{
	init();
	m_speakerTimer.setInterval(3000);
	m_speakerTimer.setSingleShot(false);
	connect(&m_speakerTimer, &QTimer::timeout, this, [=](){
		QJsonObject obj;
		obj["self"] = m_selfSpeak;
		obj["remote"] = m_otherSpeak;
		emit speakerEvent(obj);
		m_selfSpeak = false;
		m_otherSpeak = false;
	});
}

QNRtc::~QNRtc()
{
    uninit();
}

void QNRtc::init()
{
    std::string ver;
    qiniu_v2::QNRoomInterface::GetVersion(ver);
    qDebug() << QString("QN Sdk version: %s").arg(ver.c_str());
    qiniu_v2::QNRoomInterface::SetLogParams(qiniu_v2::LOG_INFO, "rtc_log", "rtc.log");

    m_rtcRoomInterface = qiniu_v2::QNRoomInterface::ObtainRoomInterface();
    m_rtcRoomInterface->SetRoomListener(this);
    m_rtcRoomInterface->EnableStatistics(1000);

    m_rtcVideoInterface = m_rtcRoomInterface->ObtainVideoInterface();
    m_rtcVideoInterface->SetVideoListener(this);
    m_rtcVideoInterface->EnableD3dRender(true);
    m_rtcAudioInterface = m_rtcRoomInterface->ObtainAudioInterface();
    m_rtcAudioInterface->SetAudioListener(this);

    m_loopTimer = new QTimer(this);
    m_loopTimer->setInterval(10);
    m_loopTimer->start();


    // 音频流任务
    int audioRecCount = m_rtcAudioInterface->GetAudioDeviceCount(qiniu_v2::AudioDeviceInfo::adt_record);
    for(int i = 0; i < audioRecCount; ++i)
    {
        qiniu_v2::AudioDeviceInfo audio_info;
        int                       ret = m_rtcAudioInterface->GetAudioDeviceInfo(qiniu_v2::AudioDeviceInfo::adt_record, i, audio_info);
        if(ret == 0)
        {
            if(audio_info.is_default)
            {
                qiniu_v2::AudioDeviceSetting ads;
                ads.device_index = audio_info.device_index;
                ads.device_type  = qiniu_v2::AudioDeviceSetting::wdt_DefaultDevice;
                m_rtcAudioInterface->SetRecordingDevice(ads);
            }
        }
    }

    int audioOutCount = m_rtcAudioInterface->GetAudioDeviceCount(qiniu_v2::AudioDeviceInfo::adt_playout);
    for(int i = 0; i < audioOutCount; ++i)
    {
        qiniu_v2::AudioDeviceInfo audio_info1;
        int                       ret1 = m_rtcAudioInterface->GetAudioDeviceInfo(qiniu_v2::AudioDeviceInfo::adt_playout, i, audio_info1);
        if(ret1 == 0)
        {
            if(audio_info1.is_default)
            {
                qiniu_v2::AudioDeviceSetting ads;
                ads.device_index = audio_info1.device_index;
                ads.device_type  = qiniu_v2::AudioDeviceSetting::wdt_DefaultDevice;
                m_rtcAudioInterface->SetPlayoutDevice(ads);
            }
        }
    }

    connect(m_loopTimer, &QTimer::timeout, this, [=]() {
        // SDK 事件驱动器
        if(m_rtcRoomInterface)
        {
            m_rtcRoomInterface->Loop();
        }
    });
}

void QNRtc::uninit()
{
    lock_guard< recursive_mutex > lck(m_mutex);
    m_remote_tracks_map.clear();
    m_localTracksList.clear();
    capture_started_ = false;
    // 释放 RTC SDK 资源
    if(m_rtcRoomInterface)
    {
        // 注意在调用 LeaveRoom 之前，用户需确保sdk的回调接口已处理并返回,
        // 否则 LeaveRoom 容易阻塞。
        leaveRoom();
        qiniu_v2::QNRoomInterface::DestroyRoomInterface(m_rtcRoomInterface);
        m_rtcRoomInterface = nullptr;
    }

    if(m_loopTimer && m_loopTimer->isActive())
    {
        m_loopTimer->stop();
    }
}

void QNRtc::entreRoom(const QString &token)
{
    if(token.isEmpty())
    {
        return;
    }
    m_roomToken = token;
    m_rtcRoomInterface->SetDnsServerUrl("223.5.5.5");
    int ret = m_rtcRoomInterface->JoinRoom(m_roomToken.toStdString());
    qDebug() << "token:" << token << endl;
}

void QNRtc::leaveRoom()
{
    if (!m_inRoom)
	return;
    if(m_rtcRoomInterface)
    {
        m_reConenct = false;
	StopPublish();
        m_rtcRoomInterface->LeaveRoom();
    }
    m_inRoom = false;
}

void QNRtc::setCropInfo(int posX, int posY, int width, int height)
{
    if(m_rtcVideoInterface)
    {
        m_rtcVideoInterface->SetCropAndScale(qiniu_v2::tst_ExternalYUV, qiniu_v2::p_Crop, true, posX, posY, width, height);
    }
}

void QNRtc::setRenderWidget(HWND handle)
{
    m_hwnd = handle;
}

void QNRtc::startLink(const QString &token)
{
    s_startTimeStamp = 0;
    entreRoom(token);
}

void QNRtc::stopLink()
{
    lock_guard< recursive_mutex > lck(m_mutex);
    m_remote_tracks_map.clear();
    m_localTracksList.clear();
    m_createdMergeTrack = false;
    capture_started_ = false;
    StopPublish();
    leaveRoom();
    m_reConenct = false;
    if(m_rtcRoomInterface)
        m_rtcRoomInterface->StopMergeStream();
}

void QNRtc::setUserId(const QString &userId)
{
    m_userId = userId;
    qDebug() << "userId:" << userId << endl;
}

void QNRtc::PushExternalVideoData(const uint8_t *data, unsigned long long reference_time)
{
    if(!m_rtcVideoInterface || !capture_started_ || !m_isVideoLink)
        return;

    if (s_startTimeStamp == 0) {
	    s_startTimeStamp = reference_time;
    }
    unsigned long long  timeStamp = (reference_time - s_startTimeStamp);
    if (timeStamp >= m_expectedTimestamp) {
	    int len = m_vedioFormat.width * m_vedioFormat.height * 3 / 2;
	    m_rtcVideoInterface->InputVideoFrame(m_externalVedioTrackId, data, len, m_vedioFormat.width, m_vedioFormat.height,
		    timeStamp / 1000, qiniu_v2::VideoCaptureType::kI420, qiniu_v2::kVideoRotation_0);
	    m_expectedTimestamp += m_pushInterval;
    }
}

void QNRtc::PushExternalAudioData(const uint8_t *data, int frames)
{
    if(!m_rtcAudioInterface || !capture_started_ || !m_isVideoLink)
        return;
    m_rtcAudioInterface->InputAudioFrame(data, frames * 2 * 2, 16, 48000, 2, frames);
}

void QNRtc::StartPublish()
{
    // TODO: Add your control notification handler code here
    if(!m_rtcRoomInterface)
    {
        return;
    }

    if(capture_started_ == true)
    {
        return;
    }

    auto audio_track = qiniu_v2::QNTrackInfo::CreateAudioTrackInfo(
        "microphone",
        m_audiobitrate * 1000,
        false);
    qiniu_v2::TrackInfoList track_list;
    track_list.emplace_back(audio_track);

    if (m_isVideoLink)
    {
	    // 视频流
	    auto video_track_ptr = qiniu_v2::QNTrackInfo::CreateVideoTrackInfo(
		"",
		EXTERNAL_TAG,
		nullptr,
		m_vedioFormat.width,
		m_vedioFormat.height,
		20,
		1000 * 1000,
		qiniu_v2::tst_ExternalYUV,
		false,
		false);
	    track_list.emplace_back(video_track_ptr);
    }

    auto ret2 = m_rtcRoomInterface->PublishTracks(track_list);
    qiniu_v2::QNTrackInfo::ReleaseList(track_list);
}

void QNRtc::StopPublish()
{
    if(!m_rtcRoomInterface || m_localTracksList.size() == 0)
    {
        return;
    }
    list< string > track_list;
    for(auto &&itor : m_localTracksList)
    {
        track_list.emplace_back(itor->GetTrackId());
    }
    m_rtcRoomInterface->UnPublishTracks(track_list);
    qiniu_v2::QNTrackInfo::ReleaseList(m_localTracksList);
    m_localTracksList.clear();
}

void QNRtc::setIsAdmin(bool admin)
{
    m_isAdmin = admin;
}

void QNRtc::setSei(const QJsonObject &data, int insetType)
{
    if(m_rtcVideoInterface && m_isVideoLink)
    {
        lock_guard< recursive_mutex > lck(m_mutex);
	QJsonDocument jd(data);
	QByteArray jsonData = jd.toJson(QJsonDocument::Compact);
        list< string >                track_list;
        for(auto &&itor : m_localTracksList)
        {
            if(itor->GetKind().compare("video") == 0)
                track_list.emplace_back(itor->GetTrackId());
        }

	QString content = jsonData.data();
	m_rtcVideoInterface->SetLocalVideoSei(track_list, content.toStdString(), insetType);
    }
}

void QNRtc::doLinkMerge(QString url)
{
	m_pushUrl = url;
	lock_guard<recursive_mutex> lck(m_mutex);
	m_startMergeTrack = true;
	int size1 = m_remote_tracks_map.size();
	int size2 = m_localTracksList.size();
	if (size1 == 2 && size2 == 2) {
		// 开启合流
		CreateCustomMerge();
		m_startMergeTrack = false;
	}
}

void QNRtc::CreateCustomMerge()
{
    if(m_rtcRoomInterface == nullptr)
        return;
    MergeConfig merge_config;

    qiniu_v2::MergeJob job_desc;
    job_desc.job_id       = "merge_" + m_userId.toStdString();
    m_jobId               = job_desc.job_id;
    job_desc.publish_url  = m_pushUrl.toStdString();
    job_desc.width        = merge_config.job_width;
    job_desc.height       = merge_config.job_height;
    job_desc.fps          = merge_config.job_fps;
    job_desc.bitrate      = merge_config.job_bitrate;
    job_desc.min_bitrate  = merge_config.job_min_bitrate;
    job_desc.max_bitrate  = merge_config.job_max_bitrate;
    job_desc.stretch_mode = qiniu_v2::MergeStretchMode::ASPECT_FILL;

    qiniu_v2::MergeLayer     background;
    qiniu_v2::MergeLayerList watermarks;

    m_customMergeId = QString::fromStdString(job_desc.job_id);
    int rtn         = m_rtcRoomInterface->CreateMergeJob(job_desc, background, watermarks);
}

void QNRtc::SetVideoInfo(int a, int v, int fps, int w, int h)
{
	m_audiobitrate = a;
	m_videobitrate = v;
	m_pushInterval = 1.0 / 20 * 1000000000;
	m_vedioFormat.width = w;
	m_vedioFormat.height = h;
}

void QNRtc::setMicMute(bool mute)
{
	m_mute = mute;
	m_rtcRoomInterface->MuteAudio(mute);
}

void QNRtc::setMicVolume(int v)
{
	std::string uid = m_userId.toStdString();
	m_rtcAudioInterface->SetAudioVolume(uid, (float)v / 100.f);
}

void QNRtc::setMicDevice(const QString &deviceId)
{
	if (deviceId == "disabled")
	{
		setMicMute(true);
		return;
	}
	setMicMute(false);
	bool set2default = deviceId == "default";
	int count = m_rtcAudioInterface->GetAudioDeviceCount(qiniu_v2::AudioDeviceInfo::adt_record);
	qiniu_v2::AudioDeviceSetting setting;
	for (int i=0; i<count; i++)
	{
		qiniu_v2::AudioDeviceInfo info;
		m_rtcAudioInterface->GetAudioDeviceInfo(qiniu_v2::AudioDeviceInfo::adt_record, i, info);
		if (info.is_default && set2default)
		{
			setting.device_index = info.device_index;
			m_rtcAudioInterface->SetRecordingDevice(setting);
			break;
		}
		else
		{
			if (deviceId == info.device_id)
			{
				setting.device_index = info.device_index;
				m_rtcAudioInterface->SetRecordingDevice(setting);
				break;
			}
		}
	}
}

void QNRtc::setPlayoutDevice(const QString &deviceId)
{
	bool set2default = deviceId == "default";
	int count = m_rtcAudioInterface->GetAudioDeviceCount(qiniu_v2::AudioDeviceInfo::adt_playout);
	qiniu_v2::AudioDeviceSetting setting;
	for (int i = 0; i < count; i++)
	{
		qiniu_v2::AudioDeviceInfo info;
		m_rtcAudioInterface->GetAudioDeviceInfo(qiniu_v2::AudioDeviceInfo::adt_playout, i, info);
		if (info.is_default && set2default)
		{
			setting.device_index = info.device_index;
			m_rtcAudioInterface->SetPlayoutDevice(setting);
			break;
		}
		else
		{
			if (deviceId == info.device_id)
			{
				setting.device_index = info.device_index;
				m_rtcAudioInterface->SetPlayoutDevice(setting);
				break;
			}
		}
	}
}

void QNRtc::setIsVideoLink(bool b)
{
	m_isVideoLink = b;
}

void QNRtc::startSpeakTimer()
{
	m_speakerTimer.start();
}

////////////////////////SDK回调///////////////////////////////////////////////
void QNRtc::OnJoinResult(
    int                            error_code_,
    const std::string &            error_str_,
    const qiniu_v2::UserInfoList & user_vec_,
    const qiniu_v2::TrackInfoList &stream_vec_,
    bool                           reconnect_flag_)
{
    if(0 == error_code_)
    {
        qDebug() << "OnJoinResult: reconenct:" << (reconnect_flag_ ? 1 : 0) << endl;
        lock_guard< recursive_mutex > lck(m_mutex);
        m_localTracksList.clear();
        m_remote_tracks_map.clear();
        qiniu_v2::TrackInfoList sub_tracks_list;
        m_reConenct = reconnect_flag_;
        for(auto &&itor : stream_vec_)
        {
            if(itor->GetUserId().compare(m_userId.toStdString().c_str()) == 0)
            {
                continue;
            }
            auto    tmp_track_ptr = qiniu_v2::QNTrackInfo::Copy(itor);
            QString trackId       = QString::fromStdString(tmp_track_ptr->GetTrackId());
            qDebug() << "OnJoinResult m_remote_tracks add:id:" << trackId << endl;
            m_remote_tracks_map[tmp_track_ptr->GetTrackId()] = tmp_track_ptr;
            if(tmp_track_ptr->GetKind().compare("video") == 0)
            {
                tmp_track_ptr->SetRenderHwnd(m_hwnd);
            }
            sub_tracks_list.emplace_back(tmp_track_ptr);
            itor->Release();
        }

        if(m_remote_tracks_map.size() > 2)
        {
            qDebug() << "OnJoinResult Remote Size Error" << endl;
            emit linkStateResult(Failture);
            return;
        }

        if(!sub_tracks_list.empty())
        {
            int rtn = m_rtcRoomInterface->SubscribeTracks(sub_tracks_list);
            qDebug() << "OnJoinResult SubscribeTracks:Result:" << rtn << endl;
        }

        if(!reconnect_flag_)
        {
	    m_inRoom = true;
            StartPublish();
            emit linkStateResult(JoinSucess);
        }
        else
        {
            emit linkStateResult(ReConnect);
        }
    }
    else
    {
        QString errorStr = error_str_.c_str();
        switch(error_code_)
        {
            case Err_Token_Error:
                emit linkStateResult(ReJoin, error_code_, errorStr);
                break;
            case Err_Token_Expired:
                emit linkStateResult(ReJoin, error_code_, errorStr);
                break;
            case Err_Room_Closed:
                emit linkStateResult(JoinFailture, error_code_, errorStr);
                break;
            case Err_Room_Full:
                emit linkStateResult(JoinFailture, error_code_, errorStr);
                break;
            case Err_User_Already_Exist:
                emit linkStateResult(JoinFailture, error_code_, errorStr);
                break;
            case Err_ReconnToken_Error:
                emit linkStateResult(ReJoin, error_code_, errorStr);
                break;
            case Err_Room_Not_Exist:
                emit linkStateResult(JoinFailture, error_code_, errorStr);
                break;
            case Err_Invalid_Parameter:
                emit linkStateResult(JoinFailture, error_code_, errorStr);
                break;
            default:
                break;
        }
        qDebug() << "OnJoinResult failture code:" << error_code_ << "errorStr:" << errorStr << endl;
    }
}

void QNRtc::OnLeave(int error_code_, const std::string &error_str_, const std::string &user_id_)
{
}

void QNRtc::OnRoomStateChange(qiniu_v2::RoomState state_)
{
}

void QNRtc::OnPublishTracksResult(
    int                            error_code_,
    const std::string &            error_str_,
    const qiniu_v2::TrackInfoList &track_info_list_)
{
    if(error_code_ != 0)
    {
        QString errorStr = error_str_.c_str();
        emit    linkStateResult(Failture, error_code_, errorStr);
        qDebug() << "OnPublishTracksResult failture code:" << error_code_ << endl;
        return;
    }

    lock_guard< recursive_mutex > lck(m_mutex);
    wchar_t                       buff[1024] = { 0 };
    for(auto &&itor : track_info_list_)
    {
        if(itor->GetTrackId().empty())
        {
            // 此 Track 发布失败
            qDebug() << "OnPublishTracks empty" << endl;
            continue;
        }
        qDebug() << "OnPublishTracksResult trackId:" << QString::fromStdString(itor->GetTrackId()) << endl;
        m_localTracksList.emplace_back(qiniu_v2::QNTrackInfo::Copy(itor));
        m_rtcAudioInterface->EnableAudioFakeInput(m_isVideoLink);
        if(itor->GetSourceType() == qiniu_v2::tst_ExternalYUV)
        {
            capture_started_       = true;
            m_externalVedioTrackId = itor->GetTrackId();
        }
    }

    if(!m_reConenct)
        emit linkStateResult(PublishSucess);

    if(m_reConenct || m_createdMergeTrack)
    {
        MergeConfig                mergeConfig;
        qiniu_v2::MergeOptInfoList add_tracks_list;
        for(auto &&itor : track_info_list_)
        {
            qiniu_v2::MergeOptInfo merge_opt;
            merge_opt.track_id = itor->GetTrackId();
            merge_opt.is_video = true;
            if(itor->GetKind().compare("audio") == 0)
            {
                merge_opt.is_video = false;
                add_tracks_list.emplace_back(merge_opt);
                continue;
            }

            qDebug() << "localTracksList video:" << endl;
            merge_opt.pos_x          = mergeConfig.local_video_x;
            merge_opt.pos_y          = mergeConfig.local_video_y;
            merge_opt.pos_z          = 0;
            merge_opt.width          = mergeConfig.local_video_width;
            merge_opt.height         = mergeConfig.local_video_height;
            merge_opt.is_support_sei = true;
            add_tracks_list.emplace_back(merge_opt);
        }
        list< string > remove_tracks_list;
        m_rtcRoomInterface->SetMergeStreamlayouts(add_tracks_list, remove_tracks_list, m_jobId);
    }

    // 释放 SDK 内部资源
    for(auto &&itor : track_info_list_)
    {
        itor->Release();
    }
}

void QNRtc::OnSubscribeTracksResult(
    int                            error_code_,
    const std::string &            error_str_,
    const qiniu_v2::TrackInfoList &track_info_list_)
{
    if(error_code_ == 0)
    {
        for(auto &&itor : track_info_list_)
        {
            if(itor->IsConnected())
            {
                if(itor->GetKind().compare("video") == 0)
                {
                    if(m_rtcVideoInterface)
                    {
                        m_rtcVideoInterface->SetStretchMode(itor->GetTrackId(), qiniu_v2::ASPECT_FILL);
                    }
                }
                m_remoteTracksList.emplace_back(itor);
                qDebug() << "OnSubscribeTracksResult:tracId:" << QString::fromStdString(itor->GetTrackId()) << endl;
            }
        }

        int size = track_info_list_.size();
        qDebug() << "OnSubscribeTracksResult:size:" << size << endl;
        if(m_reConenct || m_createdMergeTrack)
        {
            MergeConfig                mergeConfig;
            qiniu_v2::MergeOptInfoList add_tracks_list;
            for(auto &&itor : track_info_list_)
            {
                qiniu_v2::MergeOptInfo merge_opt;
                merge_opt.track_id = itor->GetTrackId();
                merge_opt.is_video = true;
                if(itor->GetKind().compare("audio") == 0)
                {
                    merge_opt.is_video = false;
                    add_tracks_list.emplace_back(merge_opt);
                    continue;
                }

                qDebug() << "localTracksList video:" << endl;
                merge_opt.pos_x          = mergeConfig.remote_video_x;
                merge_opt.pos_y          = mergeConfig.remote_video_y;
                merge_opt.pos_z          = 0;
                merge_opt.width          = mergeConfig.remote_video_width;
                merge_opt.height         = mergeConfig.remote_video_height;
                merge_opt.is_support_sei = false;
                add_tracks_list.emplace_back(merge_opt);
            }
            list< string > remove_tracks_list;
            m_rtcRoomInterface->SetMergeStreamlayouts(add_tracks_list, remove_tracks_list, m_jobId);
        }


        for(auto &&itor : track_info_list_)
        {
            itor->Release();
        }
        emit linkStateResult(SubscribeSucess);
    }
    else
    {
        qDebug() << "OnSubscribeTracksResult failture code:" << error_code_ << endl;
        QString errorStr = error_str_.c_str();
        emit    linkStateResult(Failture, error_code_, errorStr);
    }
}

void QNRtc::OnRemoteAddTracks(
    const qiniu_v2::TrackInfoList &track_list_)
{
    lock_guard< recursive_mutex > lck(m_mutex);
    qiniu_v2::TrackInfoList       sub_tracks_list;
    m_remote_tracks_map.clear();
    for(auto &&itor : track_list_)
    {
        auto    tmp_track_ptr = qiniu_v2::QNTrackInfo::Copy(itor);
        QString trackId       = QString::fromStdString(tmp_track_ptr->GetTrackId());
        qDebug() << "OnRemoteAddTracks m_remote_tracks add:id:" << trackId << endl;
        if(tmp_track_ptr->GetKind().compare("video") == 0)
        {
            tmp_track_ptr->SetRenderHwnd(m_hwnd);
        }
        m_remote_tracks_map.insert_or_assign(tmp_track_ptr->GetTrackId(), tmp_track_ptr);
        sub_tracks_list.emplace_back(tmp_track_ptr);
    }
    m_rtcRoomInterface->SubscribeTracks(sub_tracks_list);
    for(auto &&itor : track_list_)
    {
        itor->Release();
    }

    if(m_startMergeTrack && m_remote_tracks_map.size() == 2 && m_localTracksList.size() == 2)
    {
        // 开启合流
        CreateCustomMerge();
        m_startMergeTrack = false;
    }
}

void QNRtc::OnRemoteDeleteTracks(
    const std::list< string > &track_list_)
{
    lock_guard< recursive_mutex > lck(m_mutex);
    // 释放本地资源
    if(m_remote_tracks_map.empty())
    {
        return;
    }

    for(auto &&itor : track_list_)
    {
        if(m_remote_tracks_map.empty())
        {
            break;
        }
        auto tmp_ptr = m_remote_tracks_map.begin();
        while(tmp_ptr != m_remote_tracks_map.end())
        {
            if(tmp_ptr->second->GetTrackId().compare(itor) == 0)
            {
                m_remote_tracks_map.erase(tmp_ptr);
                break;
            }
            ++tmp_ptr;
        }
    }
}

void QNRtc::OnRemoteUserJoin(
    const std::string &user_id_,
    const std::string &user_data_)
{
}

void QNRtc::OnRemoteUserLeave(
    const std::string &user_id_,
    int                error_code_)
{
}

void QNRtc::OnKickoutResult(
    const std::string &kicked_out_user_id_,
    int                error_code_,
    const std::string &error_str_)
{
}

void QNRtc::OnRemoteTrackMuted(
    const std::string &track_id_,
    bool               mute_flag_)
{
}

void QNRtc::OnStatisticsUpdated(
    const qiniu_v2::StatisticsReport &statistics_)
{
}

void QNRtc::OnReceiveMessage(
    const qiniu_v2::CustomMessageList &custom_message_)
{
}

void QNRtc::OnRemoteUserReconnecting(const std::string &remote_user_id_)
{
}

void QNRtc::OnRemoteUserReconnected(const std::string &remote_user_id_)
{
}

void QNRtc::OnUnPublishTracksResult(
    const qiniu_v2::TrackInfoList &track_list_)
{
    // 释放 SDK 内部资源
    for(auto &&itor : track_list_)
    {
        itor->Release();
    }
}

void QNRtc::OnRemoteStatisticsUpdated(
    const qiniu_v2::StatisticsReportList &statistics_list_)
{
}


void QNRtc::OnSetSubscribeTracksProfileResult(
    int                            error_code_,
    const std::string &            error_str_,
    const qiniu_v2::TrackInfoList &track_list_)
{
    for(auto &&itor : track_list_)
        itor->Release();
}

void QNRtc::OnCreateForwardResult(
    const std::string &job_id_,
    int                error_code_,
    const std::string &error_str_)
{
}

void QNRtc::OnStopForwardResult(
    const std::string &job_id_,
    const std::string &job_iid_,
    int                error_code_,
    const std::string &error_str_)
{
}

// 音频数据回调，本地和远端的都通过此接口
void QNRtc::OnAudioPCMFrame(
    const unsigned char *audio_data_,
    int                  bits_per_sample_,
    int                  sample_rate_,
    size_t               number_of_channels_,
    size_t               number_of_frames_,
    const std::string &  user_id_)
{
	if (bits_per_sample_ / 8 == sizeof(int16_t) && user_id_.length() > 0) {
		// ASSERT(bits_per_sample_ / 8 == sizeof(int16_t));
		// 可以借助以下代码计算音量的实时分贝值
		auto level = ProcessAudioLevel((int16_t*)audio_data_, bits_per_sample_ / 8 * number_of_channels_ * number_of_frames_ / sizeof(int16_t));
		bool self = QString::fromStdString(user_id_) == m_userId;
		if (self && m_mute)
			level = 0;
		if (level > 0)
		{
			if (self)
				m_selfSpeak = true;
			else
				m_otherSpeak = true;
		}
	}
}

// 音频设备插拔事件通知
void QNRtc::OnAudioDeviceStateChanged(
    qiniu_v2::AudioDeviceState device_state_,
    const std::string &        device_guid_)
{
}

int QNRtc::OnPutExtraData(
    unsigned char *    extra_data_,
    int                extra_data_max_size_,
    const std::string &track_id_)
{
    return 0;
}

int QNRtc::OnSetMaxEncryptSize(
    int                frame_size_,
    const std::string &track_id_)
{
    return 0;
}

int QNRtc::OnEncrypt(const unsigned char *frame_,
                                                int                  frame_size_,
                                                unsigned char *      encrypted_frame_,
                                                const std::string &  track_id_)
{
    return 0;
}

void QNRtc::OnGetExtraData(
    const unsigned char *extra_data_,
    int                  extra_data_size_,
    const std::string &  track_id_)
{
}

int QNRtc::OnSetMaxDecryptSize(
    int                encrypted_frame_size_,
    const std::string &track_id_)
{
    return 0;
}

int QNRtc::OnDecrypt(
    const unsigned char *encrypted_frame_,
    int                  encrypted_size_,
    unsigned char *      frame_,
    const std::string &  track_id_)
{

    return 0;
}

void QNRtc::OnVideoDeviceStateChanged(
    qiniu_v2::VideoDeviceState device_state_,
    const std::string &        device_name_)
{
}

void QNRtc::OnVideoFrame(
    const unsigned char *      raw_data_,
    int                        data_len_,
    int                        width_,
    int                        height_,
    qiniu_v2::VideoCaptureType video_type_,
    const std::string &        track_id_,
    const std::string &        user_id_)
{

}

void QNRtc::OnVideoFramePreview(
    const unsigned char *      raw_data_,
    int                        data_len_,
    int                        width_,
    int                        height_,
    qiniu_v2::VideoCaptureType video_type_)
{
}

void QNRtc::OnCreateMergeResult(const std::string &job_id_, int error_code_,
                                                           const std::string &error_str_)
{
    if(error_code_ == 0 && job_id_ == m_jobId)
    {
        lock_guard< recursive_mutex > lck(m_mutex);
        m_createdMergeTrack = true;
        MergeConfig                mergeConfig;
        qiniu_v2::MergeOptInfoList add_tracks_list;
        list< string >             remove_tracks_list;
        int                        num(-1);

        int size = m_localTracksList.size();
        qDebug() << "localTracksList size:" << size << endl;
        for(auto &&itor : m_localTracksList)
        {
            qiniu_v2::MergeOptInfo merge_opt;
            merge_opt.track_id = itor->GetTrackId();
            merge_opt.is_video = true;
            if(itor->GetKind().compare("audio") == 0)
            {
                merge_opt.is_video = false;
                add_tracks_list.emplace_back(merge_opt);
                continue;
            }

            qDebug() << "localTracksList video:" << endl;
            merge_opt.pos_x          = mergeConfig.local_video_x;
            merge_opt.pos_y          = mergeConfig.local_video_y;
            merge_opt.pos_z          = 0;
            merge_opt.width          = mergeConfig.local_video_width;
            merge_opt.height         = mergeConfig.local_video_height;
            merge_opt.is_support_sei = true;
            add_tracks_list.emplace_back(merge_opt);
        }
        for(auto &&itor : m_remote_tracks_map)
        {
            qiniu_v2::MergeOptInfo merge_opt;
            merge_opt.track_id = itor.second->GetTrackId();
            merge_opt.is_video = true;
            if(itor.second->GetKind().compare("audio") == 0)
            {
                merge_opt.is_video = false;
                add_tracks_list.emplace_back(merge_opt);
                continue;
            }
            qDebug() << "RemoteList video:" << endl;
            merge_opt.pos_x          = mergeConfig.remote_video_x;
            merge_opt.pos_y          = mergeConfig.remote_video_y;
            merge_opt.pos_z          = 0;
            merge_opt.width          = mergeConfig.remote_video_width;
            merge_opt.height         = mergeConfig.remote_video_height;
            merge_opt.stretchMode    = qiniu_v2::ASPECT_FILL;
            merge_opt.is_support_sei = false;
            add_tracks_list.emplace_back(merge_opt);
        }
        size = m_remote_tracks_map.size();
        qDebug() << "RemoteList size:" << size << endl;
        int ret = m_rtcRoomInterface->SetMergeStreamlayouts(add_tracks_list, remove_tracks_list, m_jobId);
        qDebug() << "RemoteList result:" << ret << endl;
        if(m_reConenct == false)
            emit linkStateResult(MergeSucess);
    }
    else
    {
        if(error_code_ != 0)
        {
            qDebug() << "createMergeResult Failture:code" << error_code_ << endl;
            QString errStr = error_str_.c_str();
            emit    linkStateResult(Failture, error_code_, errStr);
        }
    }
    m_startMergeTrack = false;
}


void QNRtc::OnStopMergeResult(
    const std::string &job_id_,
    const std::string &job_iid_,
    int                error_code_,
    const std::string &error_str_)
{
    if(error_code_ == 0)
        qDebug() << "stopMerge Sucess" << endl;
    else
    {
        QString errStr = error_str_.c_str();
        qDebug() << "stopMerge Failture:code" << error_code_ << "errStr" << errStr << endl;
    }
}
