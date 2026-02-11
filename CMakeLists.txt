cmake_minimum_required(VERSION 3.16)

project(DubInstante VERSION 0.4.0 LANGUAGES CXX)

# =============================================================================
# C++ Configuration
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt MOC/RCC/UIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# =============================================================================
# Qt Dependencies
# =============================================================================

find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Multimedia
    MultimediaWidgets
    OpenGLWidgets
    OpenGL
    Concurrent
)

# =============================================================================
# Source Files by Layer
# =============================================================================

# Core Layer - Business Logic (no UI dependencies)
set(CORE_SOURCES
    src/core/PlaybackEngine.h
    src/core/PlaybackEngine.cpp
    src/core/RythmoManager.h
    src/core/RythmoManager.cpp
    src/core/AudioRecorder.h
    src/core/AudioRecorder.cpp
    src/core/ExportService.h
    src/core/ExportService.cpp
    src/core/SaveManager.h
    src/core/SaveManager.cpp
)

# GUI Layer - User Interface (passive widgets)
set(GUI_SOURCES
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/VideoWidget.h
    src/gui/VideoWidget.cpp
    src/gui/RythmoWidget.h
    src/gui/RythmoWidget.cpp
    src/gui/RythmoOverlay.h
    src/gui/RythmoOverlay.cpp
    src/gui/TrackPanel.h
    src/gui/TrackPanel.cpp
    src/gui/ClickableSlider.h
)

# Utils Layer - Shared Utilities
set(UTILS_SOURCES
    src/utils/TimeFormatter.h
    src/utils/TimeFormatter.cpp
)

# =============================================================================
# Executable
# =============================================================================

add_executable(DubInstante
    main.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
    ${UTILS_SOURCES}
    resources.qrc
)

# =============================================================================
# Include Directories
# =============================================================================

target_include_directories(DubInstante PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# =============================================================================
# Link Libraries
# =============================================================================

target_link_libraries(DubInstante PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Concurrent
)
