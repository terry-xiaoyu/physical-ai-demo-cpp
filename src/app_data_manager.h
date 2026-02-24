#ifndef APP_DATA_MANAGER_H
#define APP_DATA_MANAGER_H

#include<memory>
#include<string>

struct StuVideoCaptureConfig
{
	int width = 1280;
	int height = 720;
	int fps = 30;
};

struct StuVideoEncoderConfig
{
	int width = 1280;
	int height = 720;
	int fps = 30;
	int max_bitrate = 3000;
};

struct StuMqttConfig
{
	std::string broker_url;
	std::string client_id;
	std::string agent_id;
	std::string username;
	std::string password;
};

struct StuRtcSessionInfo
{
	std::string app_id;
	std::string room_id;
	std::string token;
	std::string user_id;
	std::string target_user_id;
	bool valid = false;
};

struct StuAppData
{
	// MQTT configuration (from config file)
	StuMqttConfig mqtt_config;

	// RTC session info (obtained dynamically via MQTT)
	StuRtcSessionInfo rtc_session;

	int rtc_env = 0;
	bool enable_video;
	bool enable_external_audio;
	bool enable_external_video;
	std::string audio_file;
	std::string video_file;
	std::string save_audio_path;
	std::shared_ptr<StuVideoCaptureConfig> video_capture_config;
	std::shared_ptr<StuVideoEncoderConfig> video_encoder_config;
	int video_device_index = 0;
};

class AppDataManager {
public:
	AppDataManager();
	~AppDataManager();
	bool parse(const std::string &json_str);
	bool load(const std::string &json_file);
	std::shared_ptr<StuAppData> getAppData() { return m_appData; }

	void setRtcSessionInfo(const StuRtcSessionInfo& info);
public:
	static AppDataManager *instance();
private:
	std::shared_ptr<StuAppData> m_appData;
};

#endif // APP_DATA_MANAGER_H
