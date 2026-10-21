cmake_minimum_required(VERSION 3.21)

project(friend VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6。配置时传入:
#   -DCMAKE_PREFIX_PATH=D:/QT/6.8.3/mingw_64
#   -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe
#   -G Ninja
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Network)

qt_add_executable(friend WIN32
    src/main.cpp
    src/ConfigManager.h
    src/ConfigManager.cpp
    src/AudioRecorder.h
    src/AudioRecorder.cpp
    src/VoskRecognizer.h
    src/VoskRecognizer.cpp
    src/LlmClient.h
    src/LlmClient.cpp
    src/PiperTts.h
    src/PiperTts.cpp
    src/AudioPlayer.h
    src/AudioPlayer.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_include_directories(friend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 只链接 Qt6。Vosk 的 libvosk.dll 在运行期由 VoskRecognizer 用
# QLibrary 动态加载,不在编译/链接期依赖,故无需 target_link_libraries。
target_link_libraries(friend PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Network
)

set_target_properties(friend PROPERTIES
    WIN32_EXECUTABLE ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "== friend: 语音 AI 助手(在线 LLM + 本地 ASR/TTS) ==")
message(STATUS "Qt6: ${Qt6_DIR}")
message(STATUS "运行期需 libvosk.dll(放 exe 同目录)与模型 + config.json 填 LLM api_key(见 README.md)")
