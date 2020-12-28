#include "TRTCCloudCore.h"
#include <mutex>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <assert.h>
#include "util/base.h"
#include "../rtc-define.h"

TRTCCloudCore *TRTCCloudCore::m_instance = nullptr;
static std::mutex engine_mex;
TRTCCloudCore *TRTCCloudCore::GetInstance()
{
	if (m_instance == NULL) {
		engine_mex.lock();
		if (m_instance == NULL) {
			m_instance = new TRTCCloudCore();
		}
		engine_mex.unlock();
	}
	return m_instance;
}
void TRTCCloudCore::Destory()
{
	engine_mex.lock();
	if (m_instance) {
		delete m_instance;
		m_instance = nullptr;
	}
	engine_mex.unlock();
}
TRTCCloudCore::TRTCCloudCore()
{
	trtc_module_ = nullptr;
	if (trtc_module_ != nullptr)
		return;

	HMODULE hmodule = NULL;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			  L"TRTCModule", &hmodule);

	char module_path[MAX_PATH] = {0};
	::GetModuleFileNameA(hmodule, module_path, _countof(module_path));

	std::string module_dir = GetPathNoExt(module_path);
	if (module_dir.length() == 0) {
		blog(LOG_ERROR, "TRTC GetModule Path Error");
		return;
	}

	std::string module_full_path = module_dir + "liteav.dll";

	trtc_module_ = ::LoadLibraryExA(module_full_path.c_str(), NULL,
					LOAD_WITH_ALTERED_SEARCH_PATH);

	if (trtc_module_ == NULL) {
		DWORD dw_ret = GetLastError();
		blog(LOG_ERROR, "TRTC Load liteav.dll Fail ErrorCode[0x%04X]",
		     dw_ret);
		return;
	} else {

		getTRTCShareInstance_ = (GetTRTCShareInstance)::GetProcAddress(
			trtc_module_, "getTRTCShareInstance");

		destroyTRTCShareInstance_ =
			(DestroyTRTCShareInstance)::GetProcAddress(
				trtc_module_, "destroyTRTCShareInstance");

		m_pCloud = getTRTCShareInstance();
		m_pDeviceManager = getTRTCShareInstance()->getDeviceManager();
	}
}

TRTCCloudCore::~TRTCCloudCore()
{
	destroyTRTCShareInstance_();
	m_pCloud = nullptr;
	getTRTCShareInstance_ = nullptr;
	destroyTRTCShareInstance_ = nullptr;

	if (trtc_module_)
		FreeLibrary(trtc_module_);
}

void TRTCCloudCore::Init()
{
	//检查默认选择设备
	m_localUserId = CDataCenter::GetInstance()->getLocalUserID();

	m_pCloud->addCallback(this);
	m_pCloud->setLogCallback(this);
	m_bPreUninit = false;
	//std::string logPath = Wide2UTF8(L"D:/中文/log/");
	//m_pCloud->setLogDirPath(logPath.c_str());
	// m_pCloud->setConsoleEnabled(true);
	m_pCloud->setAudioFrameCallback(this);
}

void TRTCCloudCore::Uninit()
{
	m_pCloud->removeCallback(this);
	m_pCloud->setLogCallback(nullptr);
}

void TRTCCloudCore::PreUninit()
{
	m_bPreUninit = true;
	if (CDataCenter::GetInstance()->m_bStartSystemVoice) {
		CDataCenter::GetInstance()->m_bStartSystemVoice = false;
		TRTCCloudCore::GetInstance()
			->getTRTCCloud()
			->stopSystemAudioLoopback();
	}

	stopCloudMixStream();
	stopCustomCaptureVideo();
	stopCustomCaptureAudio();
	m_pCloud->stopAllRemoteView();
	m_pCloud->stopLocalPreview();
	m_pCloud->muteLocalVideo(true);
	m_pCloud->muteLocalAudio(true);
}

ITRTCCloud *TRTCCloudCore::getTRTCCloud()
{
	return m_pCloud;
}

ITXDeviceManager *TRTCCloudCore::getDeviceManager()
{
	return m_pDeviceManager;
}

ITRTCCloudCallback *TRTCCloudCore::GetITRTCCloudCallback()
{
	return this;
}

void TRTCCloudCore::onError(TXLiteAVError errCode, const char *errMsg,
			    void *arg)
{
	blog(LOG_DEBUG, "onError errorCode[%d], errorInfo[%s]", errCode,
	     errMsg);

	QJsonObject data;
	data["code"] = errCode;
	data["msg"] = errMsg;
	emit trtcEvent(RTC_EVENT_ERROR, data);
}

void TRTCCloudCore::onWarning(TXLiteAVWarning warningCode,
			      const char *warningMsg, void *arg)
{
	blog(LOG_DEBUG, "onWarning errorCode[%d], errorInfo[%s]", warningCode,
	     warningMsg);

	QJsonObject data;
	data["code"] = warningCode;
	data["msg"] = warningMsg;
	emit trtcEvent(RTC_EVENT_WARNING, data);
}

void TRTCCloudCore::onEnterRoom(int result)
{
	blog(LOG_INFO, "onEnterRoom elapsed[%d]", result);

	QJsonObject data;
	data["result"] = result;
	emit trtcEvent(RTC_EVENT_ENTERROOM_RESULT, data);
}

void TRTCCloudCore::onExitRoom(int reason)
{
	Q_UNUSED(reason)
	blog(LOG_INFO, "onExitRoom reason[%d]", reason);

	emit trtcEvent(RTC_EVENT_EXITROOM_RESULT, QJsonObject());
}

void TRTCCloudCore::onRemoteUserEnterRoom(const char *userId)
{
	blog(LOG_INFO, "onMemberEnter userId[%s]", userId);

	QJsonObject data;
	data["userId"] = userId;
	emit trtcEvent(RTC_EVENT_REMOTE_USER_ENTER, data);
}

void TRTCCloudCore::onRemoteUserLeaveRoom(const char *userId, int reason)
{
	Q_UNUSED(reason)
	blog(LOG_INFO, "onMemberExit userId[%s]", userId);

	QJsonObject data;
	data["userId"] = userId;
	emit trtcEvent(RTC_EVENT_REMOTE_USER_LEAVE, data);
}

void TRTCCloudCore::onUserAudioAvailable(const char *userId, bool available)
{
	blog(LOG_INFO, "onUserAudioAvailable userId[%s] available[%d]", userId, available);

	QJsonObject data;
	data["userId"] = userId;
	data["available"] = available;
	emit trtcEvent(RTC_EVENT_USER_AUDIO_AVAILABLE, data);
}

void TRTCCloudCore::onSwitchRoom(TXLiteAVError errCode, const char *errMsg)
{
	blog(LOG_INFO, "onSwitchRoom errorCode[%d], errorInfo[%s]", errCode, errMsg);

	QJsonObject data;
	data["errCode"] = errCode;
	data["errMsg"] = errMsg;
	emit trtcEvent(RTC_EVENT_SWITCH_ROOM, data);
}

void TRTCCloudCore::onFirstAudioFrame(const char *userId)
{
	blog(LOG_INFO, "onFirstAudioFrame userId[%s]", userId);
}

void TRTCCloudCore::onUserVoiceVolume(TRTCVolumeInfo *userVolumes,
				      uint32_t userVolumesCount,
				      uint32_t totalVolume)
{
	//更新远端音量信息，暂时不需要
}

void TRTCCloudCore::onNetworkQuality(TRTCQualityInfo localQuality,
				     TRTCQualityInfo *remoteQuality,
				     uint32_t remoteQualityCount)
{
	//网络质量检测回调
}

void TRTCCloudCore::onUserSubStreamAvailable(const char *userId, bool available)
{
	//这是用户屏幕分享的画面，我们的连麦场景不应该有这一条流
	blog(LOG_INFO, "onUserSubStreamAvailable userId[%s] available[%d]", userId, available);
}

void TRTCCloudCore::onUserVideoAvailable(const char *userId, bool available)
{
	blog(LOG_INFO, "onUserVideoAvailable userId[%s] available[%d]\n", userId, available);

	QJsonObject data;
	data["userId"] = userId;
	data["available"] = available;
	emit trtcEvent(RTC_EVENT_USER_VIDEO_AVAILABLE, data);
}

void TRTCCloudCore::onStatistics(const TRTCStatistics &statis)
{
	//更新云端混流的结构信息
	
}

void TRTCCloudCore::onLog(const char *log, TRTCLogLevel level,
			  const char *module)
{
	//LINFO(L"onStatus userId[%s], paramCount[%d] \n", UTF82Wide(userId).c_str());

	int type = (int)level;
	if (type <= -1000 && type > -1999 && log && module) {
		int streamType = -1000 - type;
		blog(LOG_DEBUG, "Dashboard log data, stream type: %1, msg: %s", streamType, log);
	} else if (type == -2000 && log && module) {
		int streamType = 0;
		blog(LOG_DEBUG, "SDK Event data, stream type: %1, msg: %s", streamType, log);
	}
}

void TRTCCloudCore::onConnectOtherRoom(const char *userId,
				       TXLiteAVError errCode,
				       const char *errMsg)
{
	LINFO(L"onConnectOtherRoom\n");
	for (auto &itr : m_mapSDKMsgFilter) {
		if (itr.first == WM_USER_CMD_PKConnectStatus &&
		    itr.second != nullptr) {
			std::string *str = new std::string(errMsg);
			::PostMessage(itr.second, WM_USER_CMD_PKConnectStatus,
				      errCode, (LPARAM)str);
		}
	}
}

void TRTCCloudCore::onDisconnectOtherRoom(TXLiteAVError errCode,
					  const char *errMsg)
{
	LINFO(L"onConnectOtherRoom\n");
	for (auto &itr : m_mapSDKMsgFilter) {
		if (itr.first == WM_USER_CMD_PKDisConnectStatus &&
		    itr.second != nullptr) {
			std::string *str = new std::string(errMsg);
			::PostMessage(itr.second,
				      WM_USER_CMD_PKDisConnectStatus, errCode,
				      (LPARAM)str);
		}
	}
}

void TRTCCloudCore::onSetMixTranscodingConfig(int errCode, const char *errMsg)
{
	blog(LOG_INFO, "onSetMixTranscodingConfig errCode[%d], errMsg[%s]", errCode, errMsg);
}

void TRTCCloudCore::onStartPublishing(int err, const char *errMsg)
{
	blog(LOG_INFO, "onStartPublishing err[%d], errMsg[%s]", err, errMsg);
}

void TRTCCloudCore::onStopPublishing(int err, const char *errMsg)
{
	blog(LOG_INFO, "onStartPublishing err[%d], errMsg[%s]", err, errMsg);
}

void TRTCCloudCore::onConnectionLost()
{
	blog(LOG_INFO, "onConnectionLost");
}

void TRTCCloudCore::onTryToReconnect()
{
	blog(LOG_INFO, "onTryToReconnect");
}

void TRTCCloudCore::onConnectionRecovery()
{
	blog(LOG_INFO, "onConnectionRecovery");
}

void TRTCCloudCore::connectOtherRoom(QString userId, uint32_t roomId)
{
	QString json = QString("{\"roomId\":%1,\"userId\":\"%2\"}").arg(roomId).arg(userId);
	if (m_pCloud) {
		m_pCloud->connectOtherRoom(json.toStdString().c_str());
	}
}

void TRTCCloudCore::startCloudMixStream()
{
	m_bStartCloudMixStream = true;

	updateMixTranCodeInfo();
}

void TRTCCloudCore::stopCloudMixStream()
{
	m_bStartCloudMixStream = false;
	if (m_pCloud) {
		m_pCloud->setMixTranscodingConfig(NULL);
	}
}

void TRTCCloudCore::updateMixTranCodeInfo()
{
	if (m_bStartCloudMixStream == false)
		return;

	int appId = GenerateTestUserSig::APPID;
	int bizId = GenerateTestUserSig::BIZID;

	if (appId == 0 || bizId == 0) {
		blog(LOG_ERROR, u8"混流功能不可使用，请在TRTCGetUserIDAndUserSig.h->TXCloudAccountInfo填写混流的账号信息");
		return;
	}

	RemoteUserInfoList &remoteMetaInfo =
		CDataCenter::GetInstance()->m_remoteUser;
	LocalUserInfo &localMetaInfo =
		CDataCenter::GetInstance()->getLocalUserInfo();

	bool bAudioSenceStyle = false;
	if (CDataCenter::GetInstance()->m_sceneParams ==
		    TRTCAppSceneAudioCall ||
	    CDataCenter::GetInstance()->m_sceneParams ==
		    TRTCAppSceneVoiceChatRoom)
		bAudioSenceStyle = true;

	//没有主流，直接停止混流。
	if (!localMetaInfo.publish_audio && !localMetaInfo.publish_main_video) {
		m_pCloud->setMixTranscodingConfig(NULL);
		return;
	}

	TRTCTranscodingConfig config;
	config.mode = (TRTCTranscodingConfigMode)CDataCenter::GetInstance()
			      ->m_mixTemplateID;
	config.appId = appId;
	config.bizId = bizId;

	if (config.mode > TRTCTranscodingConfigMode_Manual) {
		if (config.mode ==
		    TRTCTranscodingConfigMode_Template_PresetLayout) {
			setPresetLayoutConfig(config);
		}

		m_pCloud->setMixTranscodingConfig(&config);
		if (config.mixUsersArray) {
			delete[] config.mixUsersArray;
			config.mixUsersArray = nullptr;
		}
		return;
	}
}

void TRTCCloudCore::getMixVideoPos(int index, int &left, int &top, int &right,
				   int &bottom)
{
	left = 20, top = 40;
	if (index == 1) {
		left = 240 / 4 * 3 + 240 * 2;
		top = 240 / 3 * 1;
	}
	if (index == 2) {
		left = 240 / 4 * 3 + 240 * 2;
		top = 240 / 3 * 2 + 240 * 1;
	}
	if (index == 3) {
		left = 240 / 4 * 2 + 240 * 1;
		top = 240 / 3 * 1;
	}
	if (index == 4) {
		left = 240 / 4 * 2 + 240 * 1;
		top = 240 / 3 * 2 + 240 * 1;
	}
	if (index == 5) {
		left = 240 / 4 * 1;
		top = 240 / 3 * 1;
	}
	if (index == 6) {
		left = 240 / 4 * 1;
		top = 240 / 3 * 2 + 240 * 1;
	}
	right = 240 + left;
	bottom = 240 + top;
}

void TRTCCloudCore::startCustomCaptureAudio(std::wstring filePat,
					    int samplerate, int channel)
{
	m_audioFilePath = filePat;
	_audio_file_length = 0;
	_audio_samplerate = samplerate;
	_audio_channel = channel;
	ifstream ifs(m_audioFilePath, ifstream::binary);
	if (!ifs)
		return;
	streampos pos = ifs.tellg();
	ifs.seekg(0, ios::end);
	_audio_file_length = ifs.tellg();
	ifs.close();

	m_bStartCustomCaptureAudio = true;
	m_pCloud->stopLocalAudio();
	if (m_pCloud)
		m_pCloud->enableCustomAudioCapture(true);

	if (custom_audio_thread_ == nullptr) {
		auto task2 = [=]() {
			while (m_bStartCustomCaptureAudio) {
				sendCustomAudioFrame();
				Sleep(20);
			}
		};
		custom_audio_thread_ = new std::thread(task2);
	}
}

void TRTCCloudCore::stopCustomCaptureAudio()
{
	m_audioFilePath = L"";
	m_bStartCustomCaptureAudio = false;
	_offset_audioread = 0;
	if (_audio_buffer != nullptr) {
		delete _audio_buffer;
		_audio_buffer = nullptr;
	}

	if (custom_audio_thread_) {
		custom_audio_thread_->join();
		delete custom_audio_thread_;
		custom_audio_thread_ = nullptr;
	}

	if (m_pCloud)
		m_pCloud->enableCustomAudioCapture(false);

	LocalUserInfo &_loginInfo = CDataCenter::GetInstance()->m_localInfo;
	if (_loginInfo.publish_audio && m_bPreUninit == false)
		m_pCloud->startLocalAudio(
			(TRTCAudioQuality)CDataCenter::GetInstance()
				->audio_quality_);
}

void TRTCCloudCore::startCustomCaptureVideo(std::wstring filePat, int width,
					    int height)
{
	m_videoFilePath = filePat;
	_video_file_length = 0;
	_video_width = width;
	_video_height = height;
	ifstream ifs(m_videoFilePath, ifstream::binary);
	if (!ifs)
		return;
	streampos pos = ifs.tellg();
	ifs.seekg(0, ios::end);
	_video_file_length = ifs.tellg();
	ifs.close();

	m_bStartCustomCaptureVideo = true;
	m_pCloud->stopLocalPreview();

	if (m_pCloud)
		m_pCloud->enableCustomVideoCapture(true);

	if (custom_video_thread_ == nullptr) {
		auto task2 = [=]() {
			while (m_bStartCustomCaptureVideo) {
				sendCustomVideoFrame();
				Sleep(66);
			}
		};
		custom_video_thread_ = new std::thread(task2);
	}
}

void TRTCCloudCore::stopCustomCaptureVideo()
{
	m_videoFilePath = L"";
	m_bStartCustomCaptureVideo = false;
	_offset_videoread = 0;
	if (_video_buffer != nullptr) {
		delete _video_buffer;
		_video_buffer = nullptr;
	}
	if (m_pCloud)
		m_pCloud->enableCustomVideoCapture(false);
	if (m_bStartLocalPreview && m_bPreUninit == false)
		m_pCloud->startLocalPreview(NULL);

	if (custom_video_thread_) {
		custom_video_thread_->join();
		delete custom_video_thread_;
		custom_video_thread_ = nullptr;
	}
}

void TRTCCloudCore::sendCustomAudioFrame()
{
	if (!m_bStartCustomCaptureAudio)
		return;
	if (m_pCloud) {
		ifstream ifs(m_audioFilePath, ifstream::binary);
		if (!ifs)
			return;

		uint32_t bufferSize = (960 * _audio_samplerate / 48000) *
				      (_audio_channel * 16 / 8);
		if (_audio_buffer == nullptr)
			_audio_buffer = (char *)malloc(bufferSize + 2);

		if (_offset_audioread + bufferSize > _audio_file_length)
			_offset_audioread = 0;

		ifs.seekg(_offset_audioread);
		ifs.read(_audio_buffer, bufferSize);
		_offset_audioread += bufferSize;

		TRTCAudioFrame frame;
		frame.audioFormat = LiteAVAudioFrameFormatPCM;
		frame.length = bufferSize;
		frame.data = _audio_buffer;
		frame.sampleRate = _audio_samplerate;
		frame.channel = _audio_channel;
		m_pCloud->sendCustomAudioData(&frame);
	}
}

void TRTCCloudCore::sendCustomVideoFrame()
{
	if (!m_bStartCustomCaptureVideo)
		return;
	if (m_pCloud) {
		ifstream ifs(m_videoFilePath, ifstream::binary);
		if (!ifs)
			return;

		uint32_t bufferSize = _video_width * _video_height * 3 / 2;
		if (_video_buffer == nullptr)
			_video_buffer = (char *)malloc(bufferSize + 2);

		if (_offset_videoread + bufferSize > _video_file_length)
			_offset_videoread = 0;

		ifs.seekg(_offset_videoread);
		ifs.read(_video_buffer, bufferSize);
		_offset_videoread += bufferSize;

		TRTCVideoFrame frame;
		frame.videoFormat = LiteAVVideoPixelFormat_I420;
		frame.length = bufferSize;
		frame.data = _video_buffer;
		frame.width = _video_width;
		frame.height = _video_height;
		m_pCloud->sendCustomVideoData(&frame);
	}
}

void TRTCCloudCore::setPresetLayoutConfig(TRTCTranscodingConfig &config)
{

	int canvasWidth = 1280;
	int canvasHeight = 720;
	if (CDataCenter::GetInstance()->m_videoEncParams.resMode ==
	    TRTCVideoResolutionModePortrait) {
		canvasHeight = 1280;
		canvasWidth = 720;
	}

	config.videoWidth = canvasWidth;
	config.videoHeight = canvasHeight;
	config.videoBitrate = 1500;
	config.videoFramerate = 15;
	config.videoGOP = 1;
	config.audioSampleRate = 48000;
	config.audioBitrate = 64;
	config.audioChannels = 1;

	config.mixUsersArraySize = 8;

	TRTCMixUser *mixUsersArray = new TRTCMixUser[config.mixUsersArraySize];
	config.mixUsersArray = mixUsersArray;
	int zOrder = 1, index = 0;
	auto setMixUser = [&](const char *_userid, int _index, int _zOrder,
			      int left, int top, int width, int height) {
		mixUsersArray[_index].roomId = nullptr;
		mixUsersArray[_index].userId = _userid;
		mixUsersArray[_index].zOrder = _zOrder;
		{
			mixUsersArray[_index].rect.left = left;
			mixUsersArray[_index].rect.top = top;
			mixUsersArray[_index].rect.right = left + width;
			mixUsersArray[_index].rect.bottom = top + height;
		}
	};
	//本地主路信息
	setMixUser("$PLACE_HOLDER_LOCAL_MAIN$", index, zOrder, 0, 0,
		   canvasWidth, canvasHeight);
	index++;
	zOrder++;

	setMixUser("$PLACE_HOLDER_LOCAL_SUB$", index, zOrder, 0, 0, canvasWidth,
		   canvasHeight);
	index++;
	zOrder++;

	if (canvasWidth < canvasHeight) {
		//竖屏排布
		int subWidth = canvasWidth / 5 / 2 * 2;
		int subHeight = canvasHeight / 5 / 2 * 2;
		int xOffSet = (canvasWidth - (3 * subWidth)) / 4;
		int yOffSet = (canvasHeight - (4 * subHeight)) / 5;
		for (int u = 0; u < 6; ++u, index++, zOrder++) {

			if (u < 3) {
				// 前三个小画面靠左往右
				setMixUser("$PLACE_HOLDER_REMOTE$", index,
					   zOrder,
					   xOffSet * (1 + u) + subWidth * u,
					   canvasHeight - yOffSet - subHeight,
					   subWidth, subHeight);
			} else if (u < 6) {
				// 后三个小画面靠左从下往上铺
				setMixUser("$PLACE_HOLDER_REMOTE$", index,
					   zOrder,
					   canvasWidth - xOffSet - subWidth,
					   canvasHeight - (u - 1) * yOffSet -
						   (u - 1) * subHeight,
					   subWidth, subHeight);
			} else {
				// 最多只叠加六个小画面
			}
		}
	} else {
		//横屏排布
		int subWidth = canvasWidth / 5 / 2 * 2;
		int subHeight = canvasHeight / 5 / 2 * 2;
		int xOffSet = 10;
		int yOffSet = (canvasHeight - (3 * subHeight)) / 4;

		for (int u = 0; u < 6; ++u, index++, zOrder++) {
			if (u < 3) {
				// 前三个小画面靠右从下往上铺
				setMixUser("$PLACE_HOLDER_REMOTE$", index,
					   zOrder,
					   canvasWidth - xOffSet - subWidth,
					   canvasHeight - (u + 1) * yOffSet -
						   (u + 1) * subHeight,
					   subWidth, subHeight);
			} else if (u < 6) {
				// 后三个小画面靠左从下往上铺
				setMixUser("$PLACE_HOLDER_REMOTE$", index,
					   zOrder, xOffSet,
					   canvasHeight - (u - 2) * yOffSet -
						   (u - 2) * subHeight,
					   subWidth, subHeight);
			} else {
				// 最多只叠加六个小画面
			}
		}
	}
}

void TRTCCloudCore::startGreenScreen(const std::string &path) {}

void TRTCCloudCore::stopGreenScreen() {}
std::string TRTCCloudCore::GetPathNoExt(std::string path)
{
	std::string str_ret;
	if (path.length() > 0) {
		size_t uPos = 0;
		for (size_t u = 0; u < path.size() - 1; u++) {
			if (path.c_str()[u] == '\\')
				uPos = u;
		}

		str_ret = path.substr(0, uPos + 1);
	}
	return str_ret;
}
