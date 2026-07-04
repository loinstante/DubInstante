cmake_minimum_required(VERSION 3.16)

project(DubInstante VERSION 0.12.0 LANGUAGES CXX)

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
# FFmpeg Dependencies
# =============================================================================

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND AND NOT WIN32)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
else()
    # On Windows or if pkg-config is not available
    find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
    find_library(AVCODEC_LIBRARY avcodec)
    find_library(AVFORMAT_LIBRARY avformat)
    find_library(AVUTIL_LIBRARY avutil)
    find_library(SWSCALE_LIBRARY swscale)
    
    if (NOT AVCODEC_INCLUDE_DIR OR NOT AVCODEC_LIBRARY)
        message(FATAL_ERROR "FFmpeg libraries not found. Please set CMAKE_PREFIX_PATH.")
    endif()

    add_library(PkgConfig::FFMPEG INTERFACE IMPORTED)
    set_target_properties(PkgConfig::FFMPEG PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${AVCODEC_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${AVCODEC_LIBRARY};${AVFORMAT_LIBRARY};${AVUTIL_LIBRARY};${SWSCALE_LIBRARY}"
    )
endif()

# =============================================================================
# Source Files by Layer
# =============================================================================

# Core Layer - Business Logic (no UI dependencies)
set(CORE_SOURCES
    src/core/PlaybackEngine.h
    src/core/PlaybackEngine.cpp
    src/core/FFmpegFrameExtractor.h
    src/core/FFmpegFrameExtractor.cpp
    src/core/RythmoManager.h
    src/core/RythmoManager.cpp
    src/core/AudioRecorder.h
    src/core/AudioRecorder.cpp
    src/core/ExportService.h
    src/core/ExportService.cpp
    src/core/SaveManager.h
    src/core/SaveManager.cpp
    src/core/SettingsManager.h
    src/core/SettingsManager.cpp
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
    src/gui/AudioMeterWidget.h
    src/gui/AudioMeterWidget.cpp
    src/gui/TrackWidget.h
    src/gui/TrackWidget.cpp
    src/gui/TrackSettingsDialog.h
    src/gui/TrackSettingsDialog.cpp
    src/gui/GlobalSettingsDialog.h
    src/gui/GlobalSettingsDialog.cpp
    src/gui/ExportDialog.h
    src/gui/ExportDialog.cpp
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

if(APPLE)
    set(MACOS_PLIST_INFO "${CMAKE_CURRENT_SOURCE_DIR}/deploy/Info.plist")
    add_executable(DubInstante MACOSX_BUNDLE
        main.cpp
        ${CORE_SOURCES}
        ${GUI_SOURCES}
        ${UTILS_SOURCES}
        resources.qrc
    )
    set_target_properties(DubInstante PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/deploy/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "DubInstante"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.dubinstante.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 DubInstante"
    )
else()
    add_executable(DubInstante
        main.cpp
        ${CORE_SOURCES}
        ${GUI_SOURCES}
        ${UTILS_SOURCES}
        resources.qrc
    )
endif()

# =============================================================================
# Include Directories
# =============================================================================

target_include_directories(DubInstante PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# Single source of truth for the application version
target_compile_definitions(DubInstante PRIVATE APP_VERSION="${PROJECT_VERSION}")

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
    PkgConfig::FFMPEG
)

# =============================================================================
# Tests (opt-in: cmake --build . --target test_savemanager)
# =============================================================================

add_executable(test_savemanager EXCLUDE_FROM_ALL
    tests/test_savemanager.cpp
    src/core/SaveManager.h
    src/core/SaveManager.cpp
    src/core/RythmoManager.h
    src/core/RythmoManager.cpp
)
target_include_directories(test_savemanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)
target_link_libraries(test_savemanager PRIVATE Qt6::Gui)
