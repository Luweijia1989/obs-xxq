#pragma once
#include <map>
#include <queue>
#include <string>
#include <memory>
#include "TRTCCloudDef.h"
#include "ITRTCCloud.h"

class CConfigMgr;

#define TRTCAudioQualityUnSelect 0
enum LivePlayerSourceType
{
    TRTC_RTC,
    TRTC_CDN
};

typedef struct RemoteUserInfo
{
    std::string user_id = "";
    uint32_t room_id = 0;
    std::string str_room_id = "";
    bool available_main_video = false;
    bool available_sub_video = false;
    bool available_audio = false;
    bool subscribe_audio = false;
    bool subscribe_main_video = false;
    bool subscribe_sub_video = false;
}RemoteUserInfo;

typedef struct _tagLocalUserInfo {
public:
    _tagLocalUserInfo() {};
    std::string _userId = "test_trtc_01";
    std::string _pwd = "12345678";
    int _roomId = 1222222;
    std::string strRoomId = "";
    std::string _userSig;
}LocalUserInfo;

typedef struct _tagPKUserInfo
{
    std::string _userId = "";
    uint32_t _roomId = 0;
    bool bEnterRoom = false;
}PKUserInfo;

typedef std::map<std::string, RemoteUserInfo> RemoteUserInfoList;

class CDataCenter
{
public:
    typedef struct _tagVideoResBitrateTable
    {
    public:
        int defaultBitrate = 800;
        int minBitrate = 1200;
        int maxBitrate = 200;
    public:
        void init(int bitrate, int minBit, int maxBit)
        {
            defaultBitrate = bitrate;
            minBitrate = minBit;
            maxBitrate = maxBit;
        }
        void resetLiveSence()
        {
            defaultBitrate = defaultBitrate * 15 / 10;
            minBitrate = minBitrate * 15 / 10;
            maxBitrate = maxBitrate * 15 / 10;
        }
    }VideoResBitrateTable;
public:
    static std::shared_ptr<CDataCenter> GetInstance();
    CDataCenter();
    ~CDataCenter();
    void CleanRoomInfo();
    void Init();    //初始化SDK的local配置信息
public:
    LocalUserInfo& getLocalUserInfo();
    void setLocalUserInfo(std::string userId, int roomId, std::string userSig);
    std::string getLocalUserID() { return m_localInfo._userId; };
    VideoResBitrateTable getVideoConfigInfo(int resolution);
    TRTCVideoStreamType getRemoteVideoStreamType();

public:

    TRTCVideoEncParam m_videoEncParams;
    TRTCNetworkQosParam m_qosParams;
    TRTCAppScene m_sceneParams = TRTCAppSceneVideoCall;
    TRTCRoleType m_roleType = TRTCRoleAnchor;

    /*
    enum {
        Env_PROD = 0,   // 正式环境
        Env_DEV = 1,    // 开发测试环境
        Env_UAT = 2,    // 体验环境
    };
    */
    int m_nLinkTestServer = 0; //是否连接测试环境。
    int m_mixTemplateID = 0;
    std::string m_strCustomStreamId;

    std::map<int, VideoResBitrateTable> m_videoConfigMap;
    //是否在room中
    bool m_bIsEnteredRoom = false;
public: 
    //远端用户信息
    RemoteUserInfoList m_remoteUser;
    void addRemoteUser(std::string userId, bool bClear = true);
    void removeRemoteUser(std::string userId);
    RemoteUserInfo* FindRemoteUser(std::string userId);
public:
    LocalUserInfo m_localInfo;

    std::vector<PKUserInfo> m_vecPKUserList;
};

