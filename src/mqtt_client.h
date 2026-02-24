#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace mqtt {
    class async_client;
    class connect_options;
}

struct VoiceChatInfo {
    std::string app_id;
    std::string room_id;
    std::string token;
    std::string user_id;
    std::string target_user_id;
    bool valid = false;
};

struct MqttConfig {
    std::string broker_url;
    std::string client_id;
    std::string agent_id;
    std::string username;
    std::string password;
};

class MqttClient {
public:
    MqttClient();
    ~MqttClient();

    bool connect(const MqttConfig& config);
    void disconnect();
    bool isConnected() const;

    bool initializeSession();
    bool startVoiceChat(const std::string& task_id, VoiceChatInfo& info);
    bool destroySession();

    void setMessageCallback(std::function<void(const std::string&, const std::string&)> callback);

private:
    void onMessageArrived(const std::string& topic, const std::string& payload);
    bool sendRequest(const std::string& method, const std::string& params,
                     const std::string& request_id, std::string& response, int timeout_ms = 10000);
    void publishMessage(const std::string& topic, const std::string& payload);
    std::string generateRequestId();

private:
    std::unique_ptr<mqtt::async_client> m_client;
    MqttConfig m_config;
    std::atomic<bool> m_connected{false};

    std::mutex m_responseMutex;
    std::condition_variable m_responseCv;
    std::string m_pendingRequestId;
    std::string m_pendingResponse;
    bool m_responseReceived = false;

    std::function<void(const std::string&, const std::string&)> m_messageCallback;

    static std::atomic<int> s_requestCounter;
};

#endif // MQTT_CLIENT_H
