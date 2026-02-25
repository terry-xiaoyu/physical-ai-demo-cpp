#ifdef __Linux__
#include <dlfcn.h>

#endif
#include <iostream>
#include <cctype>
#include <chrono>
#include "util/util.h"
#include "util/argparser.h"
#include "app_data_manager.h"
#include "rtc_engine_wrapper.h"
#include "mqtt_client.h"
#include <signal.h>
#include <atomic>

#define DEFAULT_CONFIG_JSON_FILE "config.json"
std::atomic<bool> g_bExit(false);
MqttClient* g_mqttClient = nullptr;

void exitAppSignalCallback(int signal)
{
	if (g_bExit) {
		LOG_WARN("already in exit status, current signal:" << signal);
		return;
	}
	g_bExit = true;
	LOG_WARN("recv exit signal:"<<signal<< ", start destroy rtc engine!");
	RTCVideoEngineWrapper::instance()->destory();
	if (g_mqttClient) {
		g_mqttClient->disconnect();
	}
	LOG_INFO("end destroy rtc engine");
	exit(0);
}

void registerSignals()
{
	 signal(SIGINT, &exitAppSignalCallback);
	 signal(SIGABRT, &exitAppSignalCallback);
	 signal(SIGILL, &exitAppSignalCallback);
	 signal(SIGTERM, &exitAppSignalCallback);
#ifndef WIN32
	 signal(SIGTSTP, &exitAppSignalCallback);
	 signal(SIGQUIT, &exitAppSignalCallback);
	 signal(SIGSTOP, &exitAppSignalCallback);
#endif
}

int main(int argc, char* argv[]) {

	registerSignals();
	std::string configJsonFilePath;
	std::string audioFilePath;
	std::string saveAudioPath;

	bytertc::argparser::set_parser("config_file", [&](std::string&& config_file) {
		configJsonFilePath = config_file;
	});
	bytertc::argparser::set_parser("audio_file", [&](std::string&& audio_file) {
		audioFilePath = audio_file;
	});
	bytertc::argparser::set_parser("save_audio", [&](std::string&& save_path) {
		saveAudioPath = save_path;
	});

	bytertc::setCurrentDir(bytertc::getExePath());
	if (argc > 1 && !bytertc::argparser::parse(argc, argv)) {
		bytertc::printHelpAndExit();
	}

	if (configJsonFilePath.empty()) {
		configJsonFilePath = DEFAULT_CONFIG_JSON_FILE;
	}

	auto appDataIns = AppDataManager::instance();
	if (!appDataIns->load(configJsonFilePath)) {
		bytertc::printHelpAndExit();
	}

	// Override audio file path from command line
	if (!audioFilePath.empty()) {
		appDataIns->getAppData()->audio_file = audioFilePath;
		appDataIns->getAppData()->enable_external_audio = true;
		LOG_INFO("Using audio file from command line: " << audioFilePath);
	}

	// Set save audio path
	if (!saveAudioPath.empty()) {
		appDataIns->getAppData()->save_audio_path = saveAudioPath;
		LOG_INFO("Will save received audio to: " << saveAudioPath);
	}

	// Step 1: Connect to MQTT broker
	MqttClient mqttClient;
	g_mqttClient = &mqttClient;

	const auto& mqttConfig = appDataIns->getAppData()->mqtt_config;
	MqttConfig config;
	config.broker_url = mqttConfig.broker_url;
	config.client_id = mqttConfig.client_id;
	config.agent_id = mqttConfig.agent_id;
	config.username = mqttConfig.username;
	config.password = mqttConfig.password;

	LOG_INFO("Connecting to MQTT broker...");
	if (!mqttClient.connect(config)) {
		LOG_ERROR("Failed to connect to MQTT broker!");
		return -1;
	}

	// Step 2: Initialize session with agent
	LOG_INFO("Initializing session with agent...");
	if (!mqttClient.initializeSession()) {
		LOG_ERROR("Failed to initialize session with agent!");
		mqttClient.disconnect();
		return -1;
	}

	// Step 3: Start voice chat to get RTC session info
	LOG_INFO("Starting voice chat to get RTC session info...");

	VoiceChatInfo voiceChatInfo;
	if (!mqttClient.startVoiceChat(voiceChatInfo)) {
		LOG_ERROR("Failed to start voice chat!");
		mqttClient.disconnect();
		return -1;
	}

	// Step 4: Set RTC session info to AppDataManager
	StuRtcSessionInfo rtcSession;
	rtcSession.app_id = voiceChatInfo.app_id;
	rtcSession.room_id = voiceChatInfo.room_id;
	rtcSession.token = voiceChatInfo.token;
	rtcSession.user_id = voiceChatInfo.user_id;
	rtcSession.target_user_id = voiceChatInfo.target_user_id;
	rtcSession.valid = true;
	appDataIns->setRtcSessionInfo(rtcSession);

	// Step 5: Initialize and join RTC room
	auto nRet = RTCVideoEngineWrapper::instance()->init();
	if (nRet) {
		LOG_ERROR("Failed to initialize RTC engine!");
		mqttClient.disconnect();
		bytertc::printHelpAndExit();
	}

	nRet = RTCVideoEngineWrapper::instance()->joinRoom();
	if (nRet) {
		LOG_ERROR("Failed to join RTC room!");
		mqttClient.disconnect();
		bytertc::printHelpAndExit();
	}

	LOG_INFO("Successfully joined RTC room. Press ESC or 'q' to exit.");

	int ch;
	while ((ch = std::getchar()) != EOF) {
		if (std::isprint(ch)) {
			LOG_INFO("Input ESC or q to exit. current input:"<<(char)ch);
		}
		// Press ESC or 'q' to exit
		if (ch == 27 || ch == 113) {
			break;
		}
	}

	RTCVideoEngineWrapper::instance()->destory();
	mqttClient.disconnect();
	g_mqttClient = nullptr;
	return 0;
}
