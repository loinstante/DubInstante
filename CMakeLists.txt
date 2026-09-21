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
    src/core/Constants.h
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
    src/gui/Palette.h
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
    # Info.plist is a template: the versions come from the project version
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/deploy/Info.plist"
        "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    )
    add_executable(DubInstante MACOSX_BUNDLE
        main.cpp
        ${CORE_SOURCES}
        ${GUI_SOURCES}
        ${UTILS_SOURCES}
        resources.qrc
    )
    set_target_properties(DubInstante PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "DubInstante"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.dubinstante.app"
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

# Warnings (main target only)
if(MSVC)
    target_compile_options(DubInstante PRIVATE /W4)
else()
    target_compile_options(DubInstante PRIVATE -Wall -Wextra -Wshadow)
endif()

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
# Tests (ctest)
# =============================================================================

enable_testing()

add_executable(test_savemanager
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
add_test(NAME test_savemanager COMMAND test_savemanager)

add_executable(test_settingsmanager
    tests/test_settingsmanager.cpp
    src/core/SettingsManager.h
    src/core/SettingsManager.cpp
)
target_include_directories(test_settingsmanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)
target_link_libraries(test_settingsmanager PRIVATE Qt6::Core Qt6::Gui)
add_test(NAME test_settingsmanager COMMAND test_settingsmanager)

add_executable(test_exportservice
    tests/test_exportservice.cpp
    src/core/ExportService.h
    src/core/ExportService.cpp
)
target_include_directories(test_exportservice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)
target_link_libraries(test_exportservice PRIVATE Qt6::Core)
add_test(NAME test_exportservice COMMAND test_exportservice)

add_executable(test_exportdialog
    tests/test_exportdialog.cpp
    src/gui/ExportDialog.h
    src/gui/ExportDialog.cpp
    src/core/ExportService.h
    src/core/ExportService.cpp
)
target_include_directories(test_exportdialog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)
target_link_libraries(test_exportdialog PRIVATE Qt6::Widgets)
add_test(NAME test_exportdialog COMMAND test_exportdialog)

add_executable(test_rythmowidget
    tests/test_rythmowidget.cpp
    src/gui/RythmoWidget.h
    src/gui/RythmoWidget.cpp
    src/core/RythmoManager.h
    src/core/RythmoManager.cpp
    src/core/SettingsManager.h
    src/core/SettingsManager.cpp
)
target_include_directories(test_rythmowidget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)
target_link_libraries(test_rythmowidget PRIVATE Qt6::Widgets)
add_test(NAME test_rythmowidget COMMAND test_rythmowidget)

# Les tests construisent un QGuiApplication : sans plugin de plateforme, ils
# meurent sur une machine sans écran (CI comprise).
set_tests_properties(
    test_savemanager test_settingsmanager test_exportservice test_exportdialog
    test_rythmowidget
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
