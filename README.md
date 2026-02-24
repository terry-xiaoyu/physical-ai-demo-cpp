RTC 命令行Demo
=========

说明

Linux 命令行开源Demo，提供本地视频采集、本地音频采集、推视频文件流，音频流等。

需要安装依赖视频和音频的依赖OpenGl，PulseAudio, OpenSSL。本地生成 TOKEN 时需要用到 OpenSSL。TOKEN 用于在加入房间时鉴权。

```
sudo apt update
sudo apt install openssl
sudo apt-get install build-essential
sudo apt-get install libgl1-mesa-dev libglu1-mesa-dev
sudo apt install pulseaudio libpulse-dev

sudo apt install -y libssl-dev
sudo apt install -y libxdamage-dev libxrandr-dev libxcomposite-dev
```

最小依赖：
Cmake >= 3.13

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
├── resources //资源文件（不包含在仓库中，需要自己准备）
│   ├── 1280X720X15XI420.yuv  //I420视频帧文件（分辨率1280x720，帧率15，像素格式I420）
│   └── 48000-stereo-s16le.pcm //PCM音频帧文件（采样率48000Hz，双声道，16位）
├── src
│   ├── app_data_manager.cc  //全局数据管理类
│   ├── app_data_manager.h
│   ├── main.cc   //主函数
│   ├── rtc_engine_wrapper.cc //火山引擎包装类
│   ├── rtc_engine_wrapper.h
│   └── util
│       ├── argparser.cc //命令行参数解析类
│       ├── argparser.h
│       ├── json11   //json配置文件解析类
│       │   ├── json11.cpp
│       │   ├── json11.hpp
│       │   └── LICENSE.txt
│       ├── thread_loop.h //线程定时器
│       ├── util.cc  //
│       └── util.h
└── third_party （不包含在仓库中，需要自己准备）
    ├── Linux 
    │   ├── VolcEngineRTC_arm  //arm sdk
    │   └── VolcEngineRTC_x86 //x86
    └── Windows
```

## 构建项目

本项目使用 [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) 管理部分依赖，CMake 配置时会自动下载所需的依赖库。

### 步骤一：构建 Demo 工程

```bash
cd QuickStart_Terminal_Demo
cmake -B build
```

> **注意**：首次构建时，CPM.cmake 会自动从 GitHub 下载依赖，请确保网络畅通。

### 步骤二：编译 Demo 工程

```bash
cmake --build build
# 或者进入 build 目录使用 make
cd build && make -j$(nproc)
```

### 步骤三：获取 AppId 和 App Key

1. 登录[火山引擎控制台](https://console.volcengine.com/rtc/listRTC)
2. 创建应用并获取 **AppId** 和 **App Key**
3. App Key 用于生成临时 Token，详见[密钥说明](https://www.volcengine.com/docs/6348/69828)

### 步骤四：修改配置文件

编辑 `build/config.json` 文件：

```json
{
    "app_id": "your_app_id",
    "app_key": "your_app_key",
    "room_id": "your_room_id",
    "user_id": "your_user_id",
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
    "video_device_id": "print in console",
    "audio_device_id": "print in console"
}
```

**必须修改的字段：**
- `app_id` - 在控制台获取的 AppId
- `app_key` - 在控制台获取的 App Key
- `room_id` - 房间 ID（自定义）
- `user_id` - 用户 ID（自定义）

### 步骤五：运行 Demo

```bash
cd build
./rtccli
```

默认配置：关闭音频采集，开启视频内部采集。如需体验不同功能，修改 `config.json` 后重启程序。config.json配置文件各字段功能含义如下表所示：
-------------------------------------------------------------------------------------
字段名称                |                   功能含义
-------------------------------------------------------------------------------------
app_id                |                   应用唯一标识
-------------------------------------------------------------------------------------
app_key                 |          在控制台上获取的 AppKey，用于生成临时 Token
-------------------------------------------------------------------------------------
room_id               |                  房间ID
-------------------------------------------------------------------------------------
user_id               |                  用户id
-------------------------------------------------------------------------------------
enable_audio          |          打开或者关闭音频采集模块
-------------------------------------------------------------------------------------
enable_video          |          打开或者关闭视频采集模块
-------------------------------------------------------------------------------------
enable_external_audio | 是否开启外部音频采集。默认为false表示开启内部音频采集，这个也是SDK默认行为；
                      | true表示开启外部音频采集，对应调用setAudioSourceType，设置主流type类型为
                      | kAudioSourceTypeExternal。该字段只在打开音频采集模块功能即
                      | enable_video=true时生效。
---------------------------------------------------------------------------------------
enable_external_video | 是否开启外部视频采集。默认为false表示开启内部视频采集，这个也是SDK默认行为;
					  | true表示开启外部视频采集，对应调用setVideoSourceType，设置主流type类型为
					  | kVideoSourceTypeExternal。该字段只在打开视频采集模块功能即
					  |enable_video=true时生效。
----------------------------------------------------------------------------------------					  
audio_file            | 用于指定外部音频采集时使用到的pcm原始音频文件。当开启外部音频采集功能后，会读取
                      | 文件PCM数据，然后每隔10ms一次循环调用pushExternalAudioFrame将pcm数据编码后
					  | 推送给远端用户。
----------------------------------------------------------------------------------------
video_file            | 用于指定外部视频采集时使用到的yuv原始视频文件。当开启外部视频采集功能后，会读取
                      | 文件yuv文件数据，然后每隔1000/fps （其中fps为帧率，文件名称中会指定）毫秒循环调用
					  | pushExternalVideoFrame将原始视频数据编码后推送给远端用户。
-----------------------------------------------------------------------------------------
video_capture_config  | 设置视频采集相关参数，包括分辨率、帧率 。该字段只在视频内部采集开启情形下才会生效，
                      | 对应 setVideoCaptureConfig。
-----------------------------------------------------------------------------------------
video_encoder_config  | 设置视频编码相关参数，包括分辨率、帧率、码率，对应 setVideoEncoderConfig。 
					  | 码率默认推荐如下，码率范围1000-10000kbps
                      | - 1920*1080@60fps，推荐6000kbps
					  | - 1920*1080@30fps，推荐4000kbps
                      | - 1920*1080@15fps，推荐2500kbps
                      | - 1280*720@60fps，推荐4000kbps
                      | - 1280*720@30fps，推荐3000kbps，作为默认码率
                      | - 1280*720@15fps，推荐2000kbps
                      | - 640*480@30fps, 推荐1500kbps
                      | - 640*480@15fps, 推荐1000kbps 
------------------------------------------------------------------------------------------
audio_device_index    | 根据枚举音频设备时使用的索引，获取音频设备ID，调用setAudioCaptureDevice设置当前
                      | 音频内部采集时使用的音频设备.程序每次启动时会调用 enumerateAudioCaptureDevices 
					  | 枚举所有音频设备，并将设备索引、 ID 、名称打印并输出到终端,可以从终端日志中获取预设的
					  | 音频设备索引，写入配置文件，再次重启后生效。
-------------------------------------------------------------------------------------------
video_device_index    | 根据枚举视频设备时使用的索引，获取视频设备ID，调用setVideoCaptureDevice设置当前
                      | 视频内部采集时使用的视频设备 。程序每次启动时会调用 enumerateVideoCaptureDevices，
					  | 枚举所有视频设备，并将设备索引、 ID 、名称打印并输出到终端，可以从终端日志中获取预设的
					  | 视频设备索引，写入配置文件，再次重启后生效。
------------------------------------------------------------------------------------------