# MCP Server Example

This example demonstrates how to use the [MCP over MQTT C++ SDK](https://github.com/terry-xiaoyu/mcp-over-mqtt-cpp-sdk) via CPM.cmake.

## Prerequisites

- CMake 3.14+
- C++17 compiler
- nlohmann-json
- Paho MQTT C++ (optional, for running the demo)

### Install Dependencies

**macOS:**
```bash
brew install nlohmann-json

# Optional: Install Paho MQTT C++ for the demo
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c && mkdir build && cd build
cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON ..
make -j$(sysctl -n hw.ncpu) && sudo make install
cd ../..

git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp && mkdir build && cd build
cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON ..
make -j$(sysctl -n hw.ncpu) && sudo make install
cd ../..
```

**Linux:**
```bash
sudo apt-get install -y nlohmann-json3-dev

# Optional: Install Paho MQTT C++ for the demo
# (same steps as macOS, use make -j$(nproc) instead)
```

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Run

```bash
./mcp_server_demo [broker_address] [server_id] [server_name]
./mcp_server_demo tcp://localhost:1883 my-server example/tools
```

## How It Works

This example:
1. Uses **CPM.cmake** to automatically fetch the MCP SDK from GitHub
2. Implements `IMqttClient` interface using Paho MQTT C++
3. Creates an MCP server with two tools: `add` and `echo`
4. Demonstrates handling MCP protocol over MQTT

The SDK is fetched automatically during CMake configuration - no manual download required.
