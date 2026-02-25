RTC 命令行Demo
=========

说明

Linux 命令行开源Demo，提供本地视频采集、本地音频采集、推视频文件流，音频流等。

本项目通过 MQTT 协议与智能体交互，动态获取 RTC 会话信息（appId、roomId、token、userId、targetUserId），无需在配置文件中手动填写这些参数。

## 安装依赖

### 基础依赖

```bash
sudo apt update
sudo apt install openssl
sudo apt-get install build-essential
sudo apt-get install libgl1-mesa-dev libglu1-mesa-dev
sudo apt install pulseaudio libpulse-dev

sudo apt install -y libssl-dev
sudo apt install -y libxdamage-dev libxrandr-dev libxcomposite-dev
sudo apt install -y nlohmann-json3-dev  # JSON 库
```

### 安装 Paho MQTT 库

本项目依赖 Paho MQTT C 和 C++ 库，需要从源码编译安装：

```bash
# 安装 Paho MQTT C 库
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
mkdir build && cd build
cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON ..
make -j$(nproc)
sudo make install
cd ../..

# 安装 Paho MQTT C++ 库
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
mkdir build && cd build
cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON ..
make -j$(nproc)
sudo make install
cd ../..

# 更新动态库缓存
sudo ldconfig
```

最小依赖：
Cmake >= 3.14

## 获取 VolcEngineRTC SDK

本项目依赖火山引擎 RTC SDK。请按以下步骤获取并配置：

1. 访问火山引擎 RTC SDK 下载页面：https://www.volcengine.com/docs/6348/75707?lang=zh

2. 根据你的平台下载对应的 SDK：
   - **ARM64 Linux**: 下载 `VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release.zip`
   - **x86_64 Linux**: 下载 `VolcEngineRTC_Linux_3.60.103.4700_x86_64_Release.zip`

3. 解压并放置到 `third_party/Linux/` 目录下：

```bash
# ARM64 架构
unzip VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release.zip
mv VolcEngineRTC_aarch64 third_party/Linux/

# 或 x86_64 架构
unzip VolcEngineRTC_Linux_3.60.103.4700_x86_64_Release.zip
mv VolcEngineRTC_x86 third_party/Linux/
```

4. 确保目录结构如下：
```
third_party/
└── Linux/
    ├── VolcEngineRTC_aarch64/   # ARM64 SDK
    │   ├── include/
    │   └── lib/
    └── VolcEngineRTC_x86/       # x86_64 SDK
        ├── include/
        └── lib/
```

构建方式

目录结构：
```
.
├── CMakeLists.txt  //cmake 工程配置文件
├── config.json //配置信息
├── README.md
├── client_agent_message_protocol.md //客户端-智能体消息交互协议文档
├── resources //资源文件（不包含在仓库中，需要自己准备）
│   ├── 1280X720X15XI420.yuv  //I420视频帧文件（分辨率1280x720，帧率15，像素格式I420）
│   └── 48000-stereo-s16le.pcm //PCM音频帧文件（采样率48000Hz，双声道，16位）
├── src
│   ├── app_data_manager.cc  //全局数据管理类
│   ├── app_data_manager.h
│   ├── main.cc   //主函数
│   ├── mqtt_client.cpp  //MQTT客户端实现
│   ├── mqtt_client.h    //MQTT客户端头文件
│   ├── rtc_engine_wrapper.cc //火山引擎包装类
│   ├── rtc_engine_wrapper.h
│   └── util
│       ├── argparser.cc //命令行参数解析类
│       ├── argparser.h
│       ├── json11   //json配置文件解析类
│       │   ├── json11.cpp
│       │   ├── json11.hpp
│       │   └── LICENSE.txt
│       ├── thread_loop.h //线程定时器
│       ├── util.cc  //
│       └── util.h
└── third_party （不包含在仓库中，需要自己准备）
    ├── Linux
    │   ├── VolcEngineRTC_arm  //arm sdk
    │   └── VolcEngineRTC_x86 //x86
    └── Windows
```

## 构建项目

### 步骤一：安装 mcp-over-mqtt-cpp-sdk

本项目依赖 mcp-over-mqtt-cpp-sdk，需要先从源码编译安装：

```bash
git clone https://github.com/terry-xiaoyu/mcp-over-mqtt-cpp-sdk.git
cd mcp-over-mqtt-cpp-sdk
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
cd ../..

# 更新动态库缓存
sudo ldconfig
```

### 步骤二：构建 Demo 工程

```bash
git clone https://github.com/terry-xiaoyu/physical-ai-demo-cpp.git
cd physical-ai-demo-cpp
cmake -B build
```

### 步骤三：编译 Demo 工程

```bash
cmake --build build
# 或者进入 build 目录使用 make
cd build && make -j$(nproc)
```

### 步骤三：修改配置文件

编辑 `build/config.json` 文件：

```json
{
    "mqtt": {
        "broker_url": "tcp://your-mqtt-broker:1883",
        "client_id": "your-client-id",
        "agent_id": "your-agent-id",
        "username": "",
        "password": ""
    },
    "enable_audio": false,
    "enable_video": true,
    "enable_external_audio": false,
    "enable_external_video": false,
    "audio_file": "48000-stereo-s16le.pcm",
    "video_file": "1280X720X15XI420.yuv",
    "video_capture_config": {
        "width": 1280,
        "height": 720,
        "fps": 30
    },
    "video_encoder_config": {
        "width": 1280,
        "height": 720,
        "fps": 30,
        "max_bitrate": 3000
    },
    "video_device_index": 0,
    "audio_device_index": -1
}
```

**必须修改的字段：**
- `mqtt.broker_url` - MQTT Broker 的地址，例如 `tcp://192.168.1.100:1883`
- `mqtt.client_id` - 客户端 ID，用于 MQTT 连接和与智能体通信
- `mqtt.agent_id` - 智能体 ID，消息将发送到该智能体

**可选字段：**
- `mqtt.username` - MQTT 认证用户名（如果 Broker 需要认证）
- `mqtt.password` - MQTT 认证密码（如果 Broker 需要认证）

### 步骤四：运行 Demo

```bash
cd build
./rtccli
```

程序启动后会执行以下流程：
1. 连接到 MQTT Broker
2. 订阅 `$agent-client/{clientId}/#` 主题
3. 向智能体发送 `initializeSession` 消息初始化会话
4. 向智能体发送 `startVoiceChat` 消息请求语音会话
5. 从智能体响应中获取 `appId`、`roomId`、`token`、`userId`、`targetUserId`
6. 使用获取的信息初始化 RTC 引擎并加入房间

默认配置：关闭音频采集，开启视频内部采集。如需体验不同功能，修改 `config.json` 后重启程序。

## 智能体交互协议

详见 [client_agent_message_protocol.md](client_agent_message_protocol.md) 文档。

主要交互流程：

1. **初始化会话**：客户端向智能体发送 `initializeSession` 请求
2. **发起语音会话**：客户端向智能体发送 `startVoiceChat` 请求，智能体返回 RTC 连接所需的参数
3. **清理会话**：程序退出时向智能体发送 `destroySession` 消息

## 配置字段说明

| 字段名称 | 功能含义 |
| --- | --- |
| `mqtt.broker_url` | MQTT Broker 地址 |
| `mqtt.client_id` | 客户端 ID |
| `mqtt.agent_id` | 智能体 ID |
| `mqtt.username` | MQTT 认证用户名（可选） |
| `mqtt.password` | MQTT 认证密码（可选） |
| `enable_audio` | 打开或者关闭音频采集模块 |
| `enable_video` | 打开或者关闭视频采集模块 |
| `enable_external_audio` | 是否开启外部音频采集。默认为 `false` 表示开启内部音频采集，这个也是 SDK 默认行为；`true` 表示开启外部音频采集，对应调用 `setAudioSourceType`，设置主流 type 类型为 `kAudioSourceTypeExternal`。该字段只在打开音频采集模块功能即 `enable_audio=true` 时生效。 |
| `enable_external_video` | 是否开启外部视频采集。默认为 `false` 表示开启内部视频采集，这个也是 SDK 默认行为；`true` 表示开启外部视频采集，对应调用 `setVideoSourceType`，设置主流 type 类型为 `kVideoSourceTypeExternal`。该字段只在打开视频采集模块功能即 `enable_video=true` 时生效。 |
| `audio_file` | 用于指定外部音频采集时使用到的 PCM 原始音频文件。当开启外部音频采集功能后，会读取文件 PCM 数据，然后每隔 10ms 一次循环调用 `pushExternalAudioFrame` 将 PCM 数据编码后推送给远端用户。 |
| `video_file` | 用于指定外部视频采集时使用到的 YUV 原始视频文件。当开启外部视频采集功能后，会读取文件 YUV 文件数据，然后每隔 1000/fps（其中 fps 为帧率，文件名称中会指定）毫秒循环调用 `pushExternalVideoFrame` 将原始视频数据编码后推送给远端用户。 |
| `video_capture_config` | 设置视频采集相关参数，包括分辨率、帧率。该字段只在视频内部采集开启情形下才会生效，对应 `setVideoCaptureConfig`。 |
| `video_encoder_config` | 设置视频编码相关参数，包括分辨率、帧率、码率，对应 `setVideoEncoderConfig`。码率默认推荐如下，码率范围 1000-10000kbps：<br>- 1920×1080@60fps，推荐 6000kbps<br>- 1920×1080@30fps，推荐 4000kbps<br>- 1920×1080@15fps，推荐 2500kbps<br>- 1280×720@60fps，推荐 4000kbps<br>- 1280×720@30fps，推荐 3000kbps（默认码率）<br>- 1280×720@15fps，推荐 2000kbps<br>- 640×480@30fps，推荐 1500kbps<br>- 640×480@15fps，推荐 1000kbps |
| `audio_device_index` | 根据枚举音频设备时使用的索引，获取音频设备 ID，调用 `setAudioCaptureDevice` 设置当前音频内部采集时使用的音频设备。程序每次启动时会调用 `enumerateAudioCaptureDevices` 枚举所有音频设备，并将设备索引、ID、名称打印并输出到终端，可以从终端日志中获取预设的音频设备索引，写入配置文件，再次重启后生效。 |
| `video_device_index` | 根据枚举视频设备时使用的索引，获取视频设备 ID，调用 `setVideoCaptureDevice` 设置当前视频内部采集时使用的视频设备。程序每次启动时会调用 `enumerateVideoCaptureDevices` 枚举所有视频设备，并将设备索引、ID、名称打印并输出到终端，可以从终端日志中获取预设的视频设备索引，写入配置文件，再次重启后生效。 |
