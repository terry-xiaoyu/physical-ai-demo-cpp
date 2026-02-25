#include "mqtt_client.h"
#include "util/util.h"
#include "json11.hpp"
#include <mqtt/async_client.h>
#include <chrono>
#include <sstream>

std::atomic<int> MqttClient::s_requestCounter{0};

class MqttCallback : public virtual mqtt::callback {
public:
    MqttCallback(MqttClient* client) : m_client(client) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
        if (m_client && m_messageHandler) {
            m_messageHandler(msg->get_topic(), msg->get_payload_str());
        }
    }

    void connected(const std::string& cause) override {
        LOG_INFO("MQTT connected: " << cause);
    }

    void connection_lost(const std::string& cause) override {
        LOG_WARN("MQTT connection lost: " << cause);
    }

    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
        m_messageHandler = handler;
    }

private:
    MqttClient* m_client;
    std::function<void(const std::string&, const std::string&)> m_messageHandler;
};

MqttClient::MqttClient() {}

MqttClient::~MqttClient() {
    disconnect();
}

bool MqttClient::connect(const MqttConfig& config) {
    m_config = config;

    try {
        m_client = std::make_unique<mqtt::async_client>(config.broker_url, config.client_id);

        auto callback = new MqttCallback(this);
        callback->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            this->onMessageArrived(topic, payload);
        });
        m_client->set_callback(*callback);

        mqtt::connect_options connOpts;
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);

        if (!config.username.empty()) {
            connOpts.set_user_name(config.username);
            connOpts.set_password(config.password);
        }

        LOG_INFO("Connecting to MQTT broker: " << config.broker_url);
        auto tok = m_client->connect(connOpts);
        tok->wait();

        std::string subscribeTopic = "$agent-client/" + config.client_id + "/#";
        LOG_INFO("Subscribing to: " << subscribeTopic);
        m_client->subscribe(subscribeTopic, 1)->wait();

        m_connected = true;
        LOG_INFO("MQTT client connected and subscribed successfully");
        return true;
    } catch (const mqtt::exception& e) {
        LOG_ERROR("MQTT connection failed: " << e.what());
        return false;
    }
}

void MqttClient::disconnect() {
    if (m_client && m_connected) {
        try {
            destroySession();
            m_client->disconnect()->wait();
            m_connected = false;
            LOG_INFO("MQTT client disconnected");
        } catch (const mqtt::exception& e) {
            LOG_ERROR("MQTT disconnect failed: " << e.what());
        }
    }
}

bool MqttClient::isConnected() const {
    return m_connected && m_client && m_client->is_connected();
}

bool MqttClient::initializeSession() {
    std::string response;
    std::string requestId = generateRequestId();

    json11::Json params = json11::Json::object{};

    if (!sendRequest("initializeSession", params.dump(), requestId, response)) {
        LOG_ERROR("Failed to initialize session");
        return false;
    }

    std::string err;
    auto responseJson = json11::Json::parse(response, err);
    if (!err.empty()) {
        LOG_ERROR("Failed to parse initializeSession response: " << err);
        return false;
    }

    auto result = responseJson["result"];
    if (result["status"].string_value() == "sessionInitialized") {
        LOG_INFO("Session initialized successfully");
        return true;
    }

    LOG_ERROR("initializeSession failed: " << response);
    return false;
}

bool MqttClient::startVoiceChat(VoiceChatInfo& info) {
    std::string response;
    std::string requestId = generateRequestId();

    json11::Json params = json11::Json::object{};

    if (!sendRequest("startVoiceChat", params.dump(), requestId, response)) {
        LOG_ERROR("Failed to start voice chat");
        return false;
    }

    std::string err;
    auto responseJson = json11::Json::parse(response, err);
    if (!err.empty()) {
        LOG_ERROR("Failed to parse startVoiceChat response: " << err);
        return false;
    }

    auto result = responseJson["result"];
    if (result.is_null()) {
        LOG_ERROR("startVoiceChat failed, no result in response: " << response);
        return false;
    }

    info.app_id = result["appId"].string_value();
    info.room_id = result["roomId"].string_value();
    info.token = result["token"].string_value();
    info.user_id = result["userId"].string_value();
    info.target_user_id = result["targetUserId"].string_value();
    info.valid = !info.app_id.empty() && !info.room_id.empty() &&
                 !info.token.empty() && !info.user_id.empty();

    if (info.valid) {
        LOG_INFO("Voice chat started - appId: " << info.app_id
                 << " roomId: " << info.room_id
                 << " userId: " << info.user_id
                 << " targetUserId: " << info.target_user_id);
    } else {
        LOG_ERROR("Invalid voice chat info received");
    }

    return info.valid;
}

bool MqttClient::destroySession() {
    if (!isConnected()) {
        return true;
    }

    json11::Json request = json11::Json::object{
        {"jsonrpc", "2.0"},
        {"method", "destroySession"},
        {"params", json11::Json::object{}}
    };

    std::string topic = "$agent/" + m_config.agent_id + "/" + m_config.client_id;
    publishMessage(topic, request.dump());
    LOG_INFO("Session destroy message sent");
    return true;
}

void MqttClient::setMessageCallback(std::function<void(const std::string&, const std::string&)> callback) {
    m_messageCallback = callback;
}

void MqttClient::onMessageArrived(const std::string& topic, const std::string& payload) {
    LOG_INFO("MQTT message received - topic: " << topic << " payload: " << payload);

    std::string err;
    auto json = json11::Json::parse(payload, err);
    if (!err.empty()) {
        LOG_WARN("Failed to parse MQTT message: " << err);
        if (m_messageCallback) {
            m_messageCallback(topic, payload);
        }
        return;
    }

    std::string id = json["id"].string_value();

    {
        std::lock_guard<std::mutex> lock(m_responseMutex);
        if (!id.empty() && id == m_pendingRequestId) {
            m_pendingResponse = payload;
            m_responseReceived = true;
            m_responseCv.notify_one();
            return;
        }
    }

    if (m_messageCallback) {
        m_messageCallback(topic, payload);
    }
}

bool MqttClient::sendRequest(const std::string& method, const std::string& params,
                              const std::string& request_id, std::string& response, int timeout_ms) {
    if (!isConnected()) {
        LOG_ERROR("MQTT client not connected");
        return false;
    }

    std::string paramsErr;
    auto paramsJson = json11::Json::parse(params, paramsErr);

    json11::Json request = json11::Json::object{
        {"jsonrpc", "2.0"},
        {"id", request_id},
        {"method", method},
        {"params", paramsJson}
    };

    std::string topic = "$agent/" + m_config.agent_id + "/" + m_config.client_id;

    {
        std::lock_guard<std::mutex> lock(m_responseMutex);
        m_pendingRequestId = request_id;
        m_pendingResponse.clear();
        m_responseReceived = false;
    }

    publishMessage(topic, request.dump());

    {
        std::unique_lock<std::mutex> lock(m_responseMutex);
        if (m_responseCv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                   [this] { return m_responseReceived; })) {
            response = m_pendingResponse;
            m_pendingRequestId.clear();
            return true;
        }
    }

    LOG_ERROR("Request timeout for method: " << method);
    m_pendingRequestId.clear();
    return false;
}

void MqttClient::publishMessage(const std::string& topic, const std::string& payload) {
    try {
        LOG_INFO("Publishing to " << topic << ": " << payload);
        m_client->publish(topic, payload.c_str(), payload.length(), 1, false)->wait();
    } catch (const mqtt::exception& e) {
        LOG_ERROR("Failed to publish message: " << e.what());
    }
}

std::string MqttClient::generateRequestId() {
    std::ostringstream oss;
    oss << "req_" << ++s_requestCounter;
    return oss.str();
}
