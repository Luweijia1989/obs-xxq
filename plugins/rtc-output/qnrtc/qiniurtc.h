#pragma once
#include "windows.h"
#include <QObject>
#include <QString>
#include <string>
#include <list>
#include "QNVideoInterface.h"
#include "QNAudioInterface.h"
#include "QNRoomInterface.h"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <map>
#include <QTimer>
#include <QPointer>
#include <thread>
#include <QFile>
using namespace std;

struct VideoFormat
{
    int width;
    int height;
};

struct AudioFormat
{
    int sampleRate;
    int bitsPerSample;
    int numberOfChannels;
};

struct MergeConfig
{
    std::string publish_url = "";

    int32_t job_width  = 375 * 3;
    int32_t job_height = 282 * 3;
    int32_t job_fps    = 25;

    int32_t job_bitrate         = 2500 * 1000;
    int32_t job_max_bitrate     = 2500 * 1000;
    int32_t job_min_bitrate     = 2500 * 1000;
    int32_t job_stretch_mode    = 1;
    int32_t local_video_x       = 0;
    int32_t local_video_y       = 0;
    int32_t local_video_width   = 375 * 3 / 2;
    int32_t local_video_height  = 282 * 3;
    int32_t remote_video_x      = 563;
    int32_t remote_video_y      = 0;
    int32_t remote_video_width  = 375 * 3 / 2;
    int32_t remote_video_height = 282 * 3;
};

class QNRtc : public QObject,
                                         qiniu_v2::QNRoomInterface::QNRoomListener,
                                         qiniu_v2::QNAudioInterface::QNAudioListener,
                                         qiniu_v2::QNVideoInterface::QNVideoListener
{
    Q_OBJECT
public:
    enum ResultCode
    {
        JoinSucess,
        PublishSucess,
        SubscribeSucess,
        MergeSucess,
        ReConnect,
        Failture,
        JoinFailture,
        ReJoin
    };

    Q_ENUM(ResultCode)
    QNRtc(QObject *parent = nullptr);
    ~QNRtc();

    void entreRoom(const QString &token);
    void init();
    void uninit();
    void StartPublish();
    void StopPublish();
    void PushExternalVideoData(const uint8_t *data, unsigned long long reference_time);
    void PushExternalAudioData(const uint8_t *data, int frames);
    void CreateCustomMerge();
    void SetVideoInfo(int a, int v, int fps);
public slots:
    void leaveRoom();
    void setCropInfo(int posX, int posY, int width, int height);
    void setRenderWidget(HWND handle);
    void startLink(const QString &token);
    void stopLink();
    void setUserId(const QString &userId);
    void setPushUrl(const QString &pushUrl);
    void setIsAdmin(bool admin = false);
    void setSei(const QJsonObject &data, int insetType);
    void doLinkMerge();
signals:
    void linkStateResult(ResultCode code, int erorrCode = 0, QString errorStr = "");

private:
    // 下面为 SDK 回调接口实现
    virtual void OnJoinResult(
        int                            error_code_,
        const std::string &            error_str_,
        const qiniu_v2::UserInfoList & user_vec_,
        const qiniu_v2::TrackInfoList &stream_vec_,
        bool                           reconnect_flag_);

    virtual void OnLeave(int error_code_, const std::string &error_str_, const std::string &user_id_);

    virtual void OnRoomStateChange(qiniu_v2::RoomState state_);

    virtual void OnPublishTracksResult(
        int                            error_code_,
        const std::string &            error_str_,
        const qiniu_v2::TrackInfoList &track_info_list_);

    virtual void OnSubscribeTracksResult(
        int                            error_code_,
        const std::string &            error_str_,
        const qiniu_v2::TrackInfoList &track_info_list_);

    virtual void OnRemoteAddTracks(
        const qiniu_v2::TrackInfoList &track_list_);

    virtual void OnRemoteDeleteTracks(
        const std::list< std::string > &track_list_);

    virtual void OnRemoteUserJoin(
        const std::string &user_id_,
        const std::string &user_data_);

    virtual void OnRemoteUserLeave(
        const std::string &user_id_,
        int                error_code_);

    virtual void OnKickoutResult(
        const std::string &kicked_out_user_id_,
        int                error_code_,
        const std::string &error_str_);

    virtual void OnRemoteTrackMuted(
        const std::string &track_id_,
        bool               mute_flag_);

    virtual void OnStatisticsUpdated(
        const qiniu_v2::StatisticsReport &statistics_);

    virtual void OnReceiveMessage(
        const qiniu_v2::CustomMessageList &custom_message_);

    virtual void OnRemoteUserReconnecting(const std::string &remote_user_id_);

    virtual void OnRemoteUserReconnected(const std::string &remote_user_id_);

    virtual void OnUnPublishTracksResult(
        const qiniu_v2::TrackInfoList &track_list_);

    virtual void OnRemoteStatisticsUpdated(
        const qiniu_v2::StatisticsReportList &statistics_list_);

    virtual void OnCreateMergeResult(
        const std::string &job_id_,
        int                error_code_,
        const std::string &error_str_);

    virtual void OnStopMergeResult(
        const std::string &job_id_,
        const std::string &job_iid_,
        int                error_code_,
        const std::string &error_str_);

    virtual void OnSetSubscribeTracksProfileResult(
        int                            error_code_,
        const string &                 error_str_,
        const qiniu_v2::TrackInfoList &track_list_);

    virtual void OnCreateForwardResult(
        const std::string &job_id_,
        int                error_code_,
        const std::string &error_str_);

    virtual void OnStopForwardResult(
        const std::string &job_id_,
        const std::string &job_iid_,
        int                error_code_,
        const std::string &error_str_);

    // 音频数据回调，本地和远端的都通过此接口
    virtual void OnAudioPCMFrame(
        const unsigned char *audio_data_,
        int                  bits_per_sample_,
        int                  sample_rate_,
        size_t               number_of_channels_,
        size_t               number_of_frames_,
        const std::string &  user_id_);

    // 音频设备插拔事件通知
    virtual void OnAudioDeviceStateChanged(
        qiniu_v2::AudioDeviceState device_state_,
        const std::string &        device_guid_);

    virtual int OnPutExtraData(
        unsigned char *    extra_data_,
        int                extra_data_max_size_,
        const std::string &track_id_);

    virtual int OnSetMaxEncryptSize(
        int                frame_size_,
        const std::string &track_id_);

    virtual int OnEncrypt(const unsigned char *frame_,
                          int                  frame_size_,
                          unsigned char *      encrypted_frame_,
                          const std::string &  track_id_);

    virtual void OnGetExtraData(
        const unsigned char *extra_data_,
        int                  extra_data_size_,
        const std::string &  track_id_);

    virtual int OnSetMaxDecryptSize(
        int                encrypted_frame_size_,
        const std::string &track_id_);

    virtual int OnDecrypt(
        const unsigned char *encrypted_frame_,
        int                  encrypted_size_,
        unsigned char *      frame_,
        const std::string &  track_id_);

    virtual void OnVideoDeviceStateChanged(
        qiniu_v2::VideoDeviceState device_state_,
        const std::string &        device_name_);
    virtual void OnVideoFrame(
        const unsigned char *      raw_data_,
        int                        data_len_,
        int                        width_,
        int                        height_,
        qiniu_v2::VideoCaptureType video_type_,
        const std::string &        track_id_,
        const std::string &        user_id_);
    virtual void OnVideoFramePreview(
        const unsigned char *      raw_data_,
        int                        data_len_,
        int                        width_,
        int                        height_,
        qiniu_v2::VideoCaptureType video_type_);

protected:
    int				m_audiobitrate;
    int				m_videobitrate;
    int				m_fps;
    bool                        m_reConenct       = false;
    bool                        m_startMergeTrack = false;
    string                      m_jobId;
    QString                     m_customMergeId;
    bool                        m_isAdmin           = false;
    HWND                        m_hwnd              = nullptr;
    qiniu_v2::QNRoomInterface * m_rtcRoomInterface  = nullptr;
    qiniu_v2::QNVideoInterface *m_rtcVideoInterface = nullptr;
    qiniu_v2::QNAudioInterface *m_rtcAudioInterface = nullptr;
    bool                        m_containAdminFlag  = false;
    qiniu_v2::TrackInfoList     m_localTracksList;
    QString                     m_roomToken;
    QString                     m_userId;
    QString                     m_pushUrl;
    QTimer*                     m_loopTimer = nullptr;

    VideoFormat                            m_vedioFormat;
    std::string                            m_externalVedioTrackId;
    qiniu_v2::TrackInfoList                m_remoteTracksList;
    map< string, qiniu_v2::QNTrackInfo * > m_remote_tracks_map;
    bool                                   m_createdMergeTrack = false;
    /////////////////////////////////////////////////////////////////
    bool                      capture_started_ = false;
    bool		      m_inRoom = false;
    recursive_mutex           m_mutex;
    static unsigned long long s_startTimeStamp;
};
