#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <mutex>

// Paho MQTT C++ headers
#include <mqtt/async_client.h>

// MCP SDK headers
#include <mcp_mqtt.h>

using namespace mcp_mqtt;

std::atomic<bool> g_running{true};

void signalHandler(int) {
    std::cout << "\nShutting down..." << std::endl;
    g_running = false;
}

/**
 * Example IMqttClient implementation using Paho MQTT C++
 */
class PahoMqttAdapter : public IMqttClient, public mqtt::callback {
public:
    PahoMqttAdapter(const std::string& brokerAddress, const std::string& clientId)
        : clientId_(clientId) {
        mqtt::create_options createOpts(MQTTVERSION_5);
        client_ = std::make_unique<mqtt::async_client>(brokerAddress, clientId, createOpts);
        client_->set_callback(*this);
    }

    ~PahoMqttAdapter() override {
        if (client_->is_connected()) {
            client_->disconnect()->wait();
        }
    }

    bool connect(const std::string& willTopic = "") {
        try {
            auto connBuilder = mqtt::connect_options_builder()
                .mqtt_version(MQTTVERSION_5)
                .clean_start(true)
                .keep_alive_interval(std::chrono::seconds(60));

            if (!willTopic.empty()) {
                mqtt::message willMsg(willTopic, "", 1, true);
                connBuilder.will(willMsg);
            }

            mqtt::properties props;
            props.add(mqtt::property(mqtt::property::SESSION_EXPIRY_INTERVAL, 0));
            connBuilder.properties(props);

            client_->connect(connBuilder.finalize())->wait();
            return true;
        } catch (const mqtt::exception& e) {
            std::cerr << "Connect error: " << e.what() << std::endl;
            return false;
        }
    }

    // IMqttClient implementation
    bool isConnected() const override { return client_->is_connected(); }

    bool subscribe(const std::string& topic, int qos, bool noLocal) override {
        try {
            mqtt::subscribe_options subOpts;
            subOpts.set_no_local(noLocal);
            client_->subscribe(topic, qos, subOpts)->wait();
            return true;
        } catch (const mqtt::exception& e) {
            std::cerr << "Subscribe error: " << e.what() << std::endl;
            return false;
        }
    }

    bool unsubscribe(const std::string& topic) override {
        try {
            client_->unsubscribe(topic)->wait();
            return true;
        } catch (const mqtt::exception& e) {
            std::cerr << "Unsubscribe error: " << e.what() << std::endl;
            return false;
        }
    }

    bool publish(const std::string& topic, const std::string& payload,
                 int qos, bool retained,
                 const std::map<std::string, std::string>& userProps) override {
        try {
            auto msg = mqtt::make_message(topic, payload, qos, retained);
            mqtt::properties props;
            for (const auto& [key, value] : userProps) {
                props.add(mqtt::property(mqtt::property::USER_PROPERTY,
                    std::make_pair(key, value)));
            }
            msg->set_properties(props);
            client_->publish(msg);
            return true;
        } catch (const mqtt::exception& e) {
            std::cerr << "Publish error: " << e.what() << std::endl;
            return false;
        }
    }

    std::string getClientId() const override { return clientId_; }

    void setMessageHandler(MqttMessageHandler handler) override {
        std::lock_guard<std::mutex> lock(mutex_);
        messageHandler_ = handler;
    }

    void setConnectionLostCallback(std::function<void(const std::string&)> callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        connectionLostCallback_ = callback;
    }

    // mqtt::callback overrides
    void message_arrived(mqtt::const_message_ptr msg) override {
        MqttMessageHandler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            handler = messageHandler_;
        }
        if (handler) {
            MqttIncomingMessage inMsg;
            inMsg.topic = msg->get_topic();
            inMsg.payload = msg->to_string();
            inMsg.qos = msg->get_qos();
            inMsg.retained = msg->is_retained();

            const auto& props = msg->get_properties();
            if (props.contains(mqtt::property::USER_PROPERTY)) {
                try {
                    auto userProps = props.get<std::vector<mqtt::string_pair>>(
                        mqtt::property::USER_PROPERTY);
                    for (const auto& prop : userProps) {
                        inMsg.userProperties[prop.first] = prop.second;
                    }
                } catch (...) {}
            }
            handler(inMsg);
        }
    }

    void connection_lost(const std::string& cause) override {
        std::function<void(const std::string&)> callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = connectionLostCallback_;
        }
        if (callback) callback(cause);
    }

    void connected(const std::string&) override {}
    void delivery_complete(mqtt::delivery_token_ptr) override {}

private:
    std::unique_ptr<mqtt::async_client> client_;
    std::string clientId_;
    std::mutex mutex_;
    MqttMessageHandler messageHandler_;
    std::function<void(const std::string&)> connectionLostCallback_;
};

int main(int argc, char* argv[]) {
    std::string broker = argc > 1 ? argv[1] : "tcp://localhost:1883";
    std::string serverId = argc > 2 ? argv[2] : "example-server";
    std::string serverName = argc > 3 ? argv[3] : "example/calculator";

    std::cout << "=== MCP over MQTT Server Example ===" << std::endl;
    std::cout << "Broker: " << broker << std::endl;
    std::cout << "Server: " << serverId << "/" << serverName << std::endl;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Create MQTT client
    PahoMqttAdapter mqttClient(broker, serverId);
    std::string willTopic = "$mcp-server/presence/" + serverId + "/" + serverName;
    if (!mqttClient.connect(willTopic)) {
        return 1;
    }
    std::cout << "Connected to MQTT broker" << std::endl;

    // Create MCP server
    McpServer mcpServer;
    mcpServer.configure({"ExampleServer", "1.0.0"});
    mcpServer.setServiceDescription("Example MCP server with calculator tools");

    // Register tools
    Tool addTool;
    addTool.name = "add";
    addTool.description = "Add two numbers";
    addTool.inputSchema.properties = {
        {"a", {{"type", "number"}}},
        {"b", {{"type", "number"}}}
    };
    addTool.inputSchema.required = {"a", "b"};
    mcpServer.registerTool(addTool, [](const nlohmann::json& args) {
        double a = args.value("a", 0.0);
        double b = args.value("b", 0.0);
        return ToolCallResult::success(std::to_string(a + b));
    });

    Tool echoTool;
    echoTool.name = "echo";
    echoTool.description = "Echo a message";
    echoTool.inputSchema.properties = {
        {"message", {{"type", "string"}}}
    };
    echoTool.inputSchema.required = {"message"};
    mcpServer.registerTool(echoTool, [](const nlohmann::json& args) {
        return ToolCallResult::success(args.value("message", ""));
    });

    mcpServer.setClientConnectedCallback([](const std::string& id, const ClientInfo& info) {
        std::cout << "[MCP] Client connected: " << id << " (" << info.name << ")" << std::endl;
    });

    mcpServer.setClientDisconnectedCallback([](const std::string& id) {
        std::cout << "[MCP] Client disconnected: " << id << std::endl;
    });

    // Start MCP server
    McpServerConfig config;
    config.serverId = serverId;
    config.serverName = serverName;

    if (!mcpServer.start(&mqttClient, config)) {
        std::cerr << "Failed to start MCP server" << std::endl;
        return 1;
    }

    std::cout << "\nMCP server running. Press Ctrl+C to exit.\n" << std::endl;

    while (g_running && mcpServer.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    mcpServer.stop();
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
