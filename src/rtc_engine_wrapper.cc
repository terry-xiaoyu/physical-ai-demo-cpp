#include "rtc_engine_wrapper.h"
#include "app_data_manager.h"
#include "util/util.h"
#include <functional>
#include <memory>
#include "rtc/bytertc_advance.h"

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELs 2

RTCVideoEngineWrapper::RTCVideoEngineWrapper(){
	m_threadLoop = std::make_unique<ThreadLoop>();
}

RTCVideoEngineWrapper::~RTCVideoEngineWrapper() {
	if (m_pVideoEngine != nullptr) {
		destory();
	}
}

int RTCVideoEngineWrapper::RTCVideoEngineWrapper::init()
{
	auto chVersion = bytertc::IRTCEngine::getSDKVersion();
	std::string strVersion = chVersion ? chVersion : "";
	LOG_INFO("sdk version:" << strVersion);
	auto appDataIns = AppDataManager::instance()->getAppData();

	// Check if RTC session info is available (obtained via MQTT)
	if (!appDataIns->rtc_session.valid) {
		LOG_ERROR("RTC session info not available! Please start voice chat via MQTT first.");
		return -1;
	}

	std::string param;
	if (appDataIns->rtc_env == 2) {
		param = "{\n"
			"    \"rtc.video_encoder\":{\n"
			"        \"codec_name\":\"auto\",\n"
			"        \"codec_mode\":\"hardware\"\n"
			"    },\n"
			"    \"config_hosts\":[\n"
			"        \"rtc-test.bytedance.com\"\n"
			"    ],\n"
			"    \"access_hosts\":[\n"
			"        \"rtc-access-test.bytedance.com\"\n"
			"    ]\n"
			"}";
	}
	else {
		param = "{\n"
			"    \"rtc.video_encoder\":{\n"
			"        \"codec_name\":\"auto\",\n"
			"        \"codec_mode\":\"hardware\"\n"
			"    }\n"
			"}";
	}
	bytertc::EngineConfig config;
	config.app_id = appDataIns->rtc_session.app_id.c_str();
	config.parameters = param.c_str();

	m_pVideoEngine = bytertc::IRTCEngine::createRTCEngine(config, this);
	if (m_pVideoEngine == nullptr) {
		LOG_INFO("create rtc video failed!");
		return -1;
	}

	initVideoDevice();

	auto nRet = initAudioConfig();
	if (nRet) {
		return nRet;
	}

	nRet = initVideoConfig();
	if (nRet) {
		return nRet;
	}
	if ((appDataIns->enable_external_audio)
		|| (appDataIns->enable_video && appDataIns->enable_external_video)) {
		m_threadLoop->do_loop();
	}
	return 0;
}

int RTCVideoEngineWrapper::joinRoom()
{
	if (m_pVideoEngine == nullptr) {
		LOG_WARN("rtc video engine is null");
		return -1;
	}

	auto appDataIns = AppDataManager::instance()->getAppData();

	// Check if RTC session info is available
	if (!appDataIns->rtc_session.valid) {
		LOG_ERROR("RTC session info not available!");
		return -1;
	}

	const auto& rtcSession = appDataIns->rtc_session;

	if (m_pRtcRoom == nullptr) {
		m_pRtcRoom = m_pVideoEngine->createRTCRoom(rtcSession.room_id.c_str());
		m_pRtcRoom->setRTCRoomEventHandler(this);
	}
	if (m_pRtcRoom == nullptr) {
		LOG_WARN("create rtc room failed!");
		return -2;
	}

	bytertc::UserInfo user;
	// use target_user_id as the uid of the current user, rtcSession.user_id is the user id of the AI agent, which is not used in RTC SDK.
	user.uid = rtcSession.target_user_id.c_str();
	bytertc::RTCRoomConfig roomConfig;
	roomConfig.stream_id = nullptr;
	roomConfig.is_auto_subscribe_audio = true;
	roomConfig.is_auto_subscribe_video = true;

	LOG_INFO("Joining room - appId: " << rtcSession.app_id
			 << " roomId: " << rtcSession.room_id
			 << " userId: " << user.uid
			 << " token: " << rtcSession.token);

	int nRet = m_pRtcRoom->joinRoom(rtcSession.token.c_str(), user, false, roomConfig);
	if (nRet != 0) {
		LOG_WARN("join rtc room failed!" << nRet);
		return nRet;
	}
	m_pRtcRoom->publishStreamVideo(true);
	int ret = m_pRtcRoom->publishStreamAudio(true);
	LOG_INFO("publishStreamAudio returned: " << ret);
	return nRet;
}

int RTCVideoEngineWrapper::destory()
{
	LOG_INFO("");
	std::lock_guard<std::mutex> locker(m_exitMutex);
	m_threadLoop->cancel_loop();

	// Close audio output file
	{
		std::lock_guard<std::mutex> audioLocker(m_audioFileMutex);
		if (m_audioOutputFile.is_open()) {
			m_audioOutputFile.close();
			LOG_INFO("Audio output file closed");
		}
		m_bSaveAudioEnabled = false;
	}

	if (m_pVideoEngine == nullptr) {
		LOG_WARN("rtc video engine is null");
		return -1;
	}

	// Unregister audio frame observer
	m_pVideoEngine->registerAudioFrameObserver(nullptr);

	if (m_pRtcRoom) {
		m_pRtcRoom->leaveRoom();
		m_pRtcRoom->destroy();
		m_pRtcRoom = nullptr;
	}
	bytertc::IRTCEngine::destroyRTCEngine();
	m_pVideoEngine = nullptr;
	return 0;
}


int RTCVideoEngineWrapper::initVideoDevice() 
{
	LOG_INFO("");
	auto appDataIns = AppDataManager::instance()->getAppData();
	auto releaseFunc = [](bytertc::IVideoDeviceCollection*col) {
		if (col) {
			col->release();
		}
	};
	auto videoDevManger = m_pVideoEngine->getVideoDeviceManager();
	std::unique_ptr<bytertc::IVideoDeviceCollection, decltype(releaseFunc)>
		captureDevs(videoDevManger->enumerateVideoCaptureDevices(), releaseFunc);
	auto nCount = captureDevs->getCount();
	bool bConfigDeviceValid = false;
	std::string configDeviceId;
	std::string configDeviceName;
	LOG_INFO("video device count=" << nCount);
	for (int i = 0; i < nCount; i++) {
		char deviceId[bytertc::MAX_DEVICE_ID_LENGTH] = { 0 };
		char deviceName[bytertc::MAX_DEVICE_ID_LENGTH] = { 0 };
		auto nRet = captureDevs->getDevice(i, deviceName, deviceId);
		if (nRet != 0) {
			LOG_WARN("enum video device failed! index :" << i);
			return nRet;
		}
		LOG_INFO("enum video device: index:" << i <<" id:["<<deviceId<<"] name:[" <<deviceName<<"]");
		if (appDataIns->video_device_index == i) {
			bConfigDeviceValid = true;
			configDeviceId = deviceId;
			configDeviceName = deviceName;
		}
	}

	if (bConfigDeviceValid) {

		int nRet = videoDevManger->setVideoCaptureDevice(configDeviceId.c_str());
		if (nRet == 0) {
			LOG_INFO("set video capture device success! index:" << appDataIns->video_device_index << "id:[" << configDeviceId << "] name: [" << configDeviceName << "]");
		}
		else {
			LOG_WARN("set video capture device failed! index:" << appDataIns->video_device_index << "id:[" << configDeviceId << "] name: [" << configDeviceName << "]");
		}
		return nRet;
	}
	else {
		LOG_WARN("video device index is invalid! index:" << appDataIns->video_device_index);
	}
	return 0;
}

int RTCVideoEngineWrapper::initAudioConfig()
{
	LOG_INFO("");
	auto appDataIns = AppDataManager::instance()->getAppData();

	// Setup audio save if path is specified
	if (!appDataIns->save_audio_path.empty()) {
		m_audioOutputFile.open(appDataIns->save_audio_path, std::ios::binary);
		if (m_audioOutputFile.is_open()) {
			m_bSaveAudioEnabled = true;
			LOG_INFO("Audio save enabled, Output File: " << appDataIns->save_audio_path);

			// Enable playback audio callback (remote audio mixed)
			// bytertc::AudioFormat format;
			// format.sample_rate = bytertc::kAudioSampleRate48000;
			// format.channel = bytertc::kAudioChannelStereo;
			// int ret = m_pVideoEngine->enableAudioFrameCallback(bytertc::AudioFrameCallbackMethod::kPlayback, format);
			// LOG_INFO("enableAudioFrameCallback(kPlayback) returned: " << ret);

			// Also enable remote user audio callback as alternative
			bytertc::AudioFormat remoteFormat;
			remoteFormat.sample_rate = bytertc::kAudioSampleRateAuto;
			remoteFormat.channel = bytertc::kAudioChannelAuto;
			int ret = m_pVideoEngine->enableAudioFrameCallback(bytertc::AudioFrameCallbackMethod::kRemoteUser, remoteFormat);
			LOG_INFO("enableAudioFrameCallback(kRemoteUser) returned: " << ret);
		
			// Register audio frame observer first
			int ret1 = m_pVideoEngine->registerAudioFrameObserver(this);
			LOG_INFO("registerAudioFrameObserver returned: " << ret1);
		} else {
			LOG_ERROR("Failed to open audio output file: " << appDataIns->save_audio_path);
		}
	}

	if (appDataIns->enable_external_audio) {
		LOG_INFO("start external audio capture");
		m_vecAudioPCMData = bytertc::readFile(appDataIns->audio_file.c_str());
		if (m_vecAudioPCMData.size() == 0) {
			LOG_INFO("audio file invalid! file path:"<<appDataIns->audio_file);
			return -1;
		}
		int n10msAudioFrameSize = AUDIO_SAMPLE_RATE * 0.01 * AUDIO_CHANNELs * sizeof(int16_t);

		m_nTotalAudioFrames = m_vecAudioPCMData.size() / n10msAudioFrameSize;
		m_nCurrentAudioFrameIndex = 0;
		m_pVideoEngine->setAudioSourceType(bytertc::AudioSourceType::kAudioSourceTypeExternal);
		m_threadLoop->addTimer(10, std::bind(&RTCVideoEngineWrapper::pushExternalAudioFrame, this),false);
	}
	return 0;
}

int RTCVideoEngineWrapper::initVideoConfig()
{
	LOG_INFO("");
	auto appDataIns = AppDataManager::instance()->getAppData();
	if (!appDataIns->enable_video) {
		LOG_INFO("enable video: " << appDataIns->enable_video);
		return 0;
	}

	if (appDataIns->enable_external_video) {
		LOG_INFO("start custom video capture");
		m_vecVideoYUVData = bytertc::readFile(appDataIns->video_file.c_str());
		if (m_vecVideoYUVData.size() == 0) {
			LOG_INFO("video file invalid! file path:" << appDataIns->video_file);
			return -1;
		}
		auto fileName = bytertc::getFileName(appDataIns->video_file);
		auto splitParts = bytertc::splitString(fileName, "X");
		if (splitParts.size() < 4) {
			LOG_INFO("yuv video file name is not correct!. name:" << fileName);
			return -2;
		}
		
		m_customCaptureConfig = std::make_unique<StuVideoCaptureConfig>();
		m_customCaptureConfig->width = bytertc::safeStrToInt(splitParts[0]);
		m_customCaptureConfig->height =  bytertc::safeStrToInt(splitParts[1]);
		m_customCaptureConfig->fps =  bytertc::safeStrToInt(splitParts[2]);
		if (m_customCaptureConfig->width <= 0 ||
			m_customCaptureConfig->height <= 0
			|| m_customCaptureConfig->fps <= 0) {
			LOG_INFO("custom video capture config is not correct! width:" << m_customCaptureConfig->width
			<<" height:"<<m_customCaptureConfig->width << " fps: "<<m_customCaptureConfig->fps);
			return -3;
		}
		m_nCurrentVideoFrameIndex = 0;
		int nFrameSize = m_customCaptureConfig->width*m_customCaptureConfig->height * 3 / 2;
		m_nTotalVideoFrames = m_vecVideoYUVData.size() / nFrameSize;

		m_pVideoEngine->setVideoSourceType(bytertc::VideoSourceType::kVideoSourceTypeExternal);
		m_threadLoop->addTimer(1000.0f/m_customCaptureConfig->fps, std::bind(&RTCVideoEngineWrapper::pushExternalVideoFrame, this), false);
	}
	else {
		if (appDataIns->video_capture_config) {
			bytertc::VideoCaptureConfig captureConfig;
			captureConfig.capture_preference = bytertc::VideoCaptureConfig::CapturePreference::kManual;
			captureConfig.width = appDataIns->video_capture_config->width;
			captureConfig.height = appDataIns->video_capture_config->height;
			captureConfig.frame_rate = appDataIns->video_capture_config->fps;
			LOG_INFO("set video capture config width:" << captureConfig.width << " height:" << captureConfig.height << " fps:" << captureConfig.frame_rate);
			m_pVideoEngine->setVideoCaptureConfig(captureConfig);
		}
		else {
			LOG_WARN("video capture config is null!");
		}

		LOG_INFO("start inner video capture");
		m_pVideoEngine->startVideoCapture();
	}

	if (appDataIns->video_encoder_config) 
	{
		bytertc::VideoEncoderConfig encoderConfig;
		encoderConfig.width = appDataIns->video_encoder_config->width;
		encoderConfig.height = appDataIns->video_encoder_config->height;
		encoderConfig.frame_rate = appDataIns->video_encoder_config->fps;
		encoderConfig.max_bitrate = appDataIns->video_encoder_config->max_bitrate;
		LOG_INFO("set video encoder width:" << encoderConfig.width << " height:" << encoderConfig.height
			<< " fps:" << encoderConfig.frame_rate << " max bitrate:" << encoderConfig.max_bitrate);
		m_pVideoEngine->setVideoEncoderConfig(encoderConfig);
	}
	else {
		LOG_WARN("video encoder config is null!");
	}

	return 0;
}

void RTCVideoEngineWrapper::pushExternalVideoFrame() 
{
	int nFrameSize = m_customCaptureConfig->width*m_customCaptureConfig->height * 3 / 2;
	auto buffer = m_vecVideoYUVData.data() + nFrameSize * m_nCurrentVideoFrameIndex;

	bytertc::VideoFrameData builder;
	builder.width = m_customCaptureConfig->width;
	builder.height = m_customCaptureConfig->height;
	int square = builder.width*builder.height;
	builder.plane_data[0] = buffer;
	builder.plane_data[1] = buffer + square;
	builder.plane_data[2] = buffer + square * 5 / 4;
	builder.plane_stride[0] = builder.width;
	builder.plane_stride[1] = builder.width >> 1;
	builder.plane_stride[2] = builder.width >> 1;
	//builder.memory_deleter = nullptr;
	builder.pixel_format = bytertc::kVideoPixelFormatI420;
	builder.rotation = bytertc::kVideoRotation0;
	builder.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	//builder.color_space = bytertc::kColorSpaceYCbCrBT601LimitedRange;

	auto curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	//std::string cur_time_str = std::to_string(curTime);
	//uint32_t seiSize = (cur_time_str.length() + 1) * sizeof(uint8_t);
	//
	//std::unique_ptr<uint8_t[]> seiData(new uint8_t[seiSize]);
	//memcpy(seiData.get(), cur_time_str.c_str(), cur_time_str.length());
	//builder.extra_data = seiData.get();
	//builder.extra_data_size = seiSize;

	//bytertc::IVideoFrame* pFrame = bytertc::IRTCEngne::BuildVideoFrame(builder);
	int nRet = m_pVideoEngine->pushExternalVideoFrame(builder);
	if (nRet) {
		LOG_ERROR("push external video frame error! ret: " << nRet);
	}

	++m_nCurrentVideoFrameIndex;
	m_nCurrentVideoFrameIndex %= m_nTotalVideoFrames;
}

void RTCVideoEngineWrapper::pushExternalAudioFrame()
{
	if (!m_bRoomAudioStateReady) {
		// 房间音频状态未准备好，暂不发送外部音频帧
		return;
	}

	auto appDataIns = AppDataManager::instance()->getAppData();

	// 检查是否在暂停状态
	if (m_bAudioLoopPaused) {
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_audioLoopPauseStartTime).count();
		if (elapsed < appDataIns->audio_loop_interval_seconds) {
			return;  // 还在暂停期间，不发送
		}
		// 暂停结束，恢复发送
		m_bAudioLoopPaused = false;
		LOG_INFO("Audio loop pause ended, resuming...");
	}

	bytertc::AudioFrameBuilder builder;
	int n10msAudioFrameSize = AUDIO_SAMPLE_RATE * 0.01 * AUDIO_CHANNELs * sizeof(int16_t);
	builder.data = m_vecAudioPCMData.data() + n10msAudioFrameSize * m_nCurrentAudioFrameIndex;
	builder.data_size = n10msAudioFrameSize;
	builder.sample_rate = static_cast<bytertc::AudioSampleRate>(AUDIO_SAMPLE_RATE);
	builder.channel = static_cast<bytertc::AudioChannel>(AUDIO_CHANNELs);
	builder.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	auto frame = bytertc::buildAudioFrame(builder);
	int nRet = m_pVideoEngine->pushExternalAudioFrame(frame);
	if (nRet) {
		LOG_ERROR("push external audio frame error! ret: " << nRet);
	}

	++m_nCurrentAudioFrameIndex;
	if (m_nCurrentAudioFrameIndex >= m_nTotalAudioFrames) {
		m_nCurrentAudioFrameIndex = 0;
		// 开始暂停
		m_bAudioLoopPaused = true;
		m_audioLoopPauseStartTime = std::chrono::steady_clock::now();
		LOG_INFO("Audio loop completed, pausing for " << appDataIns->audio_loop_interval_seconds << " seconds...");
	}
}

void RTCVideoEngineWrapper::onRoomStateChanged(const char * room_id, const char * uid, int state, const char * extra_info)
{
	std::string roomId = room_id ? room_id : "";
	std::string userId = uid ? uid : "";
	std::string extraInfo = extra_info ? extra_info : "";
	LOG_INFO("[callback] roomid: " << room_id << " userid: " << userId << " state: " << state << " extra_info: " << extraInfo);
}

void RTCVideoEngineWrapper::onWarning(int warn)
{
	LOG_INFO("[callback] warn:" << warn);
}

void RTCVideoEngineWrapper::onError(int err)
{
	LOG_INFO("[callback] err:" << err);
}

void RTCVideoEngineWrapper::onLeaveRoom(const bytertc::RtcRoomStats & stats)
{
	LOG_INFO("[callback]");
	m_bRoomAudioStateReady = false;
}

void RTCVideoEngineWrapper::onRoomStats(const bytertc::RtcRoomStats & stats)
{
	//LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onUserJoined(const bytertc::UserInfo & userInfo)
{
	std::string userId = userInfo.uid ? userInfo.uid : "";
	LOG_INFO("[callback] userid: " << userId );
}

void RTCVideoEngineWrapper::onUserLeave(const char * uid, bytertc::UserOfflineReason reason)
{
	std::string userId = uid ? uid : "";
	LOG_INFO("[callback] userid: " << userId << " reason: " <<reason);
}

void RTCVideoEngineWrapper::onUserPublishStreamVideo(const char* stream_id, const bytertc::StreamInfo& stream_info, bool is_publish) {
	std::string streamId = stream_id ? stream_id : "";
	std::string roomId = stream_info.room_id ? stream_info.room_id : "";
	std::string userId = stream_info.user_id ? stream_info.user_id : "";
	LOG_INFO("[callback] streamId: " << streamId << "userid:" << userId << "room_id:" << roomId << " is_publish: " << is_publish);

}

void RTCVideoEngineWrapper::onUserPublishStreamAudio(const char* stream_id, const bytertc::StreamInfo& stream_info, bool is_publish) {
    std::string streamId = stream_id ? stream_id : "";
	std::string roomId = stream_info.room_id ? stream_info.room_id : "";
	std::string userId = stream_info.user_id ? stream_info.user_id : "";
	LOG_INFO("[callback] streamId: " << streamId << "userid:" << userId << "room_id:" << roomId << " is_publish: " << is_publish);
}

void RTCVideoEngineWrapper::onStreamRemove(const bytertc::MediaStreamInfo & bs, bytertc::StreamRemoveReason reason)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onStreamAdd(const bytertc::MediaStreamInfo & stream)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onStreamPublishSuccess(const char * user_id, bool is_screen)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onRoomMessageReceived(const char * uid, const char * message)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onRoomBinaryMessageReceived(const char * uid, int size, const uint8_t * message)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onUserMessageReceived(const char * uid, const char * message)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onUserBinaryMessageReceived(const char * uid, int size, const uint8_t * message)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onRoomMessageSendResult(int64_t msgid, int error)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onUserMessageSendResult(int64_t message_id, int error)
{
	LOG_INFO("[callback]");
}

void RTCVideoEngineWrapper::onConnectionStateChanged(bytertc::ConnectionState state)
{
	LOG_INFO("[callback] state: "<<state);
}

void RTCVideoEngineWrapper::onPerformanceAlarms(const char* stream_id, const bytertc::StreamInfo& stream_info, bytertc::PerformanceAlarmMode mode,
            bytertc::PerformanceAlarmReason reason, const bytertc::SourceWantedData& data)
{
	LOG_INFO("[callback]");
}


void RTCVideoEngineWrapper::onAudioDeviceStateChanged(const char* device_id, bytertc::RTCAudioDeviceType device_type,
            bytertc::MediaDeviceState device_state, bytertc::MediaDeviceError device_error)
{
	LOG_INFO("[callback] device_id: "<< device_id << " device_type: "<< device_type << "  device_state:" << device_state << ", device_error:" << device_error);
}

void RTCVideoEngineWrapper::onVideoDeviceStateChanged(const char* device_id, bytertc::RTCVideoDeviceType device_type,
            bytertc::MediaDeviceState device_state, bytertc::MediaDeviceError device_error)
{
	LOG_INFO("[callback] device_id: "<< device_id << " device_type: "<< device_type << "  device_state:" << device_state << ", device_error:" << device_error);
}

void RTCVideoEngineWrapper::onRemoteAudioStateChanged(const char* stream_id, const bytertc::StreamInfo& stream_info, bytertc::RemoteAudioState state, bytertc::RemoteAudioStateChangeReason reason)
{
	std::string streamId = stream_id ? stream_id : "";
	std::string roomId = stream_info.room_id ? stream_info.room_id : "";
	std::string userId = stream_info.user_id ? stream_info.user_id : "";
	LOG_INFO("[callback] streamId: "<<streamId << " roomId: "<< roomId << " userId: "<< userId<<" index: "<<stream_info.stream_index << " state: "<<state <<" reason: " << reason);
	if (state == bytertc::RemoteAudioState::kRemoteAudioStateStarting || state == bytertc::RemoteAudioState::kRemoteAudioStateDecoding) {
		m_bRoomAudioStateReady = true;
		LOG_INFO("Remote Audio ready, audio push enabled");
	} else if (state == bytertc::RemoteAudioState::kRemoteAudioStateStopped || state == bytertc::RemoteAudioState::kRemoteAudioStateFailed) {
		m_bRoomAudioStateReady = false;
		LOG_INFO("Remote Audio stopped or failed, audio push disabled");
	}
}

void RTCVideoEngineWrapper::onFirstLocalVideoFrameCaptured(bytertc::IVideoSource* video_source, const bytertc::VideoFrameInfo& info)
{
	LOG_INFO("[callback] width: "<<info.width <<" height: "<<info.height );
}

void RTCVideoEngineWrapper::onLocalVideoSizeChanged(bytertc::IVideoSource* video_source, const bytertc::VideoFrameInfo& info)
{
	LOG_INFO("[callback] width: "<<info.width <<" height: "<<info.height );
}

void RTCVideoEngineWrapper::onRemoteVideoSizeChanged(const char* stream_id, const bytertc::StreamInfo& stream_info, const bytertc::VideoFrameInfo& info)
{
	std::string streamId = stream_id ? stream_id : "";
	std::string roomId = stream_info.room_id ? stream_info.room_id : "";
	std::string userId = stream_info.user_id ? stream_info.user_id : "";
	LOG_INFO("[callback] streamId: "<<streamId << " roomId: "<< roomId << " userId: "<< userId << " index: " << stream_info.stream_index << " width: " << info.width << " height: " <<  info.height);
}

void RTCVideoEngineWrapper::onFirstRemoteVideoFrameRendered(const char* stream_id, const bytertc::StreamInfo& stream_info, const bytertc::VideoFrameInfo& info)
{
	std::string streamId = stream_id ? stream_id : "";
	std::string roomId = stream_info.room_id ? stream_info.room_id : "";
	std::string userId = stream_info.user_id ? stream_info.user_id : "";
	LOG_INFO("[callback] streamId: "<<streamId << " roomId: "<< roomId << " userId: "<< userId << " index: " << stream_info.stream_index << " width: " << info.width << " height: " << info.height);
}

void RTCVideoEngineWrapper::onFirstRemoteVideoFrameDecoded(const char* stream_id, const bytertc::StreamInfo& stream_info, const bytertc::VideoFrameInfo& info)
{
	std::string streamId = stream_id ? stream_id : "";
	std::string roomId = stream_info.room_id ? stream_info.room_id : "";
	std::string userId = stream_info.user_id ? stream_info.user_id : "";
	LOG_INFO("[callback] streamId: "<<streamId << " roomId: "<< roomId << " userId: "<< userId << " index: " << stream_info.stream_index << " width: " << info.width << " height: " << info.height);
}


RTCVideoEngineWrapper *RTCVideoEngineWrapper::instance()
{
	static RTCVideoEngineWrapper ins;
	return &ins;
}

// IAudioFrameObserver implementations
void RTCVideoEngineWrapper::onRecordAudioFrameOriginal(const bytertc::IAudioFrame& audio_frame)
{
	// Not used
}

void RTCVideoEngineWrapper::onRecordAudioFrame(const bytertc::IAudioFrame& audio_frame)
{
	// Local microphone audio - not saving
}

void RTCVideoEngineWrapper::onPlaybackAudioFrame(const bytertc::IAudioFrame& audio_frame)
{
	LOG_INFO("[callback] sample_rate: " << audio_frame.sampleRate() << " channel: " << audio_frame.channel() << " data_size: " << audio_frame.dataSize());
	// Remote audio mixed - save to file
	if (!m_bSaveAudioEnabled) {
		return;
	}

	std::lock_guard<std::mutex> locker(m_audioFileMutex);
	if (m_audioOutputFile.is_open()) {
		m_audioOutputFile.write(reinterpret_cast<const char*>(audio_frame.data()), audio_frame.dataSize());
	}
}

void RTCVideoEngineWrapper::onRemoteUserAudioFrame(const char* stream_id, const bytertc::StreamInfo& stream_info, const bytertc::IAudioFrame& audio_frame)
{
	std::string streamId = stream_id ? stream_id : "";
	std::string userId = stream_info.user_id ? stream_info.user_id : "";
	LOG_INFO("[callback] stream_id: " << streamId << " user_id: " << userId
		<< " sample_rate: " << audio_frame.sampleRate()
		<< " channel: " << audio_frame.channel()
		<< " data_size: " << audio_frame.dataSize());

	// Save remote user audio to file
	if (!m_bSaveAudioEnabled) {
		return;
	}

	std::lock_guard<std::mutex> locker(m_audioFileMutex);
	if (m_audioOutputFile.is_open()) {
		m_audioOutputFile.write(reinterpret_cast<const char*>(audio_frame.data()), audio_frame.dataSize());
	}
}

void RTCVideoEngineWrapper::onMixedAudioFrame(const bytertc::IAudioFrame& audio_frame)
{
	// Mixed local and remote audio - not used
	LOG_INFO("[callback] sample_rate: " << audio_frame.sampleRate() << " channel: " << audio_frame.channel() << " data_size: " << audio_frame.dataSize());
}

void RTCVideoEngineWrapper::onCaptureMixedAudioFrame(const bytertc::IAudioFrame& audio_frame)
{
	// Local capture mixed audio - not used
	LOG_INFO("[callback] sample_rate: " << audio_frame.sampleRate() << " channel: " << audio_frame.channel() << " data_size: " << audio_frame.dataSize());
}

