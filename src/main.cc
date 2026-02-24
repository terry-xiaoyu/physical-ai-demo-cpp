
#ifdef __Linux__
#include <dlfcn.h>

#endif
#include <iostream>
#include <cctype>
#include "util/util.h"
#include "util/argparser.h"
#include "app_data_manager.h"
#include "rtc_engine_wrapper.h"
#include <signal.h>
#include <atomic>

#define DEFAULT_CONFIG_JSON_FILE "config.json"
std::atomic<bool> g_bExit(false);

void exitAppSignalCallback(int signal) 
{
	if (g_bExit) {
		LOG_WARN("already in exit status, current signal:" << signal);
		return;
	}
	g_bExit = true;
	LOG_WARN("recv exit signal:"<<signal<< ", start destroy rtc engine!");
	RTCVideoEngineWrapper::instance()->destory();
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

	auto nRet = RTCVideoEngineWrapper::instance()->init();
	if (nRet) {
		bytertc::printHelpAndExit();
	}

	nRet = RTCVideoEngineWrapper::instance()->joinRoom();
	if (nRet) {
		bytertc::printHelpAndExit();
	}

	int ch;
	while ((ch = std::getchar()) != EOF) {
		if (std::isprint(ch)) {
			LOG_INFO("Input ESC or q to exit. current input:"<<(char)ch);
		}
		// 按 esc 或者 q 退出
		if (ch == 27 || ch == 113) {
			break;
		}
	}
	RTCVideoEngineWrapper::instance()->destory();
    return 0;
}
