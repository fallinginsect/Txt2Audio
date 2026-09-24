cmake_minimum_required(VERSION 3.16)

project(Txt2Audio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

qt_add_executable(Txt2Audio
    main.cpp
    mainwindow.cpp
    mainwindow.h
    DropArea.cpp
    DropArea.h
    TextSplitter.cpp
    TextSplitter.h
    TtsClient.cpp
    TtsClient.h
    TtsManager.cpp
    TtsManager.h
    WavUtils.cpp
    WavUtils.h
)

target_link_libraries(Txt2Audio PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# Windows 下以 GUI 程序运行，不弹控制台窗口
if(WIN32)
    set_target_properties(Txt2Audio PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# 安装时自动调用 windeployqt 打包依赖
install(TARGETS Txt2Audio
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)

qt_generate_deploy_app_script(
    TARGET Txt2Audio
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})