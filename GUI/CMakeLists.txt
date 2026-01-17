cmake_minimum_required(VERSION 3.16)
project(BluetoothTelemetryGUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(BluetoothTelemetryGUI
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(BluetoothTelemetryGUI
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    bluetooth
)

# Installation
install(TARGETS BluetoothTelemetryGUI
    RUNTIME DESTINATION bin
)
