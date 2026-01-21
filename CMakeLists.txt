cmake_minimum_required(VERSION 3.16)

project(DUBSync VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia OpenGLWidgets OpenGL)

add_executable(DUBSync
    main.cpp
    src/MainWindow.cpp
    src/Exporter.cpp
    src/AudioRecorderManager.cpp
    src/RythmoWidget.cpp
    src/RythmoOverlay.cpp
     src/VideoWidget.cpp
    src/PlayerController.cpp
    include/MainWindow.h
    include/Exporter.h
    include/AudioRecorderManager.h
    include/RythmoWidget.h
    include/VideoWidget.h
    include/PlayerController.h
    include/RythmoOverlay.h
    include/ClickableSlider.h
    resources.qrc
)

target_include_directories(DUBSync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(DUBSync PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::OpenGLWidgets
    Qt6::OpenGL
)
