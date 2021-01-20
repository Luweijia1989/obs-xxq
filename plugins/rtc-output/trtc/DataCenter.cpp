#include "DataCenter.h"
#include <mutex>
#include <strstream>
#include <iostream>
#include <iomanip>
//////////////////////////////////////////////////////////////////////////CDataCenter

static std::shared_ptr<CDataCenter> s_pInstance;
static std::mutex engine_mex;
CRITICAL_SECTION g_DataCS;
std::shared_ptr<CDataCenter> CDataCenter::GetInstance()
{
	if (s_pInstance == NULL) {
		engine_mex.lock();
		if (s_pInstance == NULL) {
			s_pInstance = std::make_shared<CDataCenter>();
		}
		engine_mex.unlock();
	}
	return s_pInstance;
}
CDataCenter::CDataCenter()
{
	::InitializeCriticalSection(&g_DataCS); //初始化关键代码段对象

	VideoResBitrateTable &info1 =
		m_videoConfigMap[TRTCVideoResolution_120_120];
	info1.init(150, 40, 200);
	VideoResBitrateTable &info2 =
		m_videoConfigMap[TRTCVideoResolution_160_160];
	info2.init(250, 40, 300);
	VideoResBitrateTable &info3 =
		m_videoConfigMap[TRTCVideoResolution_270_270];
	info3.init(300, 100, 400);
	VideoResBitrateTable &info4 =
		m_videoConfigMap[TRTCVideoResolution_480_480];
	info4.init(500, 200, 1000);
	VideoResBitrateTable &info5 =
		m_videoConfigMap[TRTCVideoResolution_160_120];
	info5.init(150, 40, 200);
	VideoResBitrateTable &info6 =
		m_videoConfigMap[TRTCVideoResolution_240_180];
	info6.init(200, 80, 300);
	VideoResBitrateTable &info7 =
		m_videoConfigMap[TRTCVideoResolution_280_210];
	info7.init(200, 100, 300);
	VideoResBitrateTable &info8 =
		m_videoConfigMap[TRTCVideoResolution_320_240];
	info8.init(400, 100, 400);
	VideoResBitrateTable &info9 =
		m_videoConfigMap[TRTCVideoResolution_400_300];
	info9.init(400, 200, 800);
	VideoResBitrateTable &info10 =
		m_videoConfigMap[TRTCVideoResolution_480_360];
	info10.init(500, 200, 800);
	VideoResBitrateTable &info11 =
		m_videoConfigMap[TRTCVideoResolution_640_480];
	info11.init(700, 250, 1000);
	VideoResBitrateTable &info12 =
		m_videoConfigMap[TRTCVideoResolution_960_720];
	info12.init(1000, 200, 1600);
	VideoResBitrateTable &info13 =
		m_videoConfigMap[TRTCVideoResolution_320_180];
	info13.init(300, 80, 300);
	VideoResBitrateTable &info14 =
		m_videoConfigMap[TRTCVideoResolution_480_270];
	info14.init(400, 200, 800);
	VideoResBitrateTable &info15 =
		m_videoConfigMap[TRTCVideoResolution_640_360];
	info15.init(600, 200, 1000);
	VideoResBitrateTable &info16 =
		m_videoConfigMap[TRTCVideoResolution_960_540];
	info16.init(900, 400, 1600);
	VideoResBitrateTable &info17 =
		m_videoConfigMap[TRTCVideoResolution_1280_720];
	info17.init(1250, 500, 2000);
	VideoResBitrateTable &info18 =
		m_videoConfigMap[TRTCVideoResolution_1920_1080];
	info18.init(2000, 1000, 3000);

	m_sceneParams = TRTCAppSceneVideoCall;
}

CDataCenter::~CDataCenter()
{
	::DeleteCriticalSection(&g_DataCS); //删除关键代码段对象
}

void CDataCenter::CleanRoomInfo()
{
	m_remoteUser.clear();
}

CDataCenter::VideoResBitrateTable
CDataCenter::getVideoConfigInfo(int resolution)
{
	VideoResBitrateTable info;
	if (m_videoConfigMap.find(resolution) != m_videoConfigMap.end())
		info = m_videoConfigMap[resolution];

	if (m_sceneParams == TRTCAppSceneLIVE)
		info.resetLiveSence();
	return info;
}

void CDataCenter::Init()
{
	//音视频参数配置
	m_videoEncParams.videoResolution = TRTCVideoResolution_1280_720;
	m_videoEncParams.videoFps = 20;
	m_videoEncParams.videoBitrate = 1200;
	m_videoEncParams.resMode = TRTCVideoResolutionModePortrait;
	m_qosParams.preference = TRTCVideoQosPreferenceClear;
	m_qosParams.controlMode = TRTCQosControlModeServer;
	m_sceneParams = TRTCAppSceneLIVE;
	m_roleType = TRTCRoleAnchor;
	m_nLinkTestServer = 0; // todo 环境切换
	m_mixTemplateID = TRTCTranscodingConfigMode_Template_PresetLayout;
}

RemoteUserInfo *CDataCenter::FindRemoteUser(std::string userId)
{
	std::map<std::string, RemoteUserInfo>::iterator iter;
	iter = m_remoteUser.find(userId);
	if (iter != m_remoteUser.end()) {
		return &iter->second;
	}
	return nullptr;
}

void CDataCenter::addRemoteUser(std::string userId, bool bClear)
{
	std::map<std::string, RemoteUserInfo>::iterator iter;
	iter = m_remoteUser.find(userId);
	if (iter != m_remoteUser.end()) {
		if (bClear)
			m_remoteUser.erase(iter);
		else
			return;
	}

	RemoteUserInfo info;
	info.user_id = userId;
	m_remoteUser.insert(
		std::pair<std::string, RemoteUserInfo>(userId, info));
}

void CDataCenter::removeRemoteUser(std::string userId)
{
	std::map<std::string, RemoteUserInfo>::iterator
		iter; //定义一个迭代指针iter
	iter = m_remoteUser.find(userId);
	if (iter != m_remoteUser.end()) {
		m_remoteUser.erase(iter);
	}
}

TRTCVideoStreamType CDataCenter::getRemoteVideoStreamType()
{
	return TRTCVideoStreamTypeBig;
}
