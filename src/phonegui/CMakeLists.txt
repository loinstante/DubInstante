cmake_minimum_required(VERSION 3.16)

project(DubInstanteAndroid VERSION 0.6.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Qml)

qt_add_executable(DubInstanteAndroid
    main.cpp
)

qt_add_qml_module(DubInstanteAndroid
    URI DubInstante
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES Main.qml
)

if(ANDROID)
    set_property(TARGET DubInstanteAndroid APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
                 "${CMAKE_CURRENT_SOURCE_DIR}/android")

    # Android 15 requires 16KB page alignment for native libraries
    target_link_options(DubInstanteAndroid PRIVATE "-Wl,-z,max-page-size=16384")

    # Force modern SDK targets (Qt defaults to API 23/31 which is ancient)
    set_target_properties(DubInstanteAndroid PROPERTIES
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 35
    )
endif()

target_link_libraries(DubInstanteAndroid PRIVATE
    Qt6::Quick
    Qt6::Qml
)
