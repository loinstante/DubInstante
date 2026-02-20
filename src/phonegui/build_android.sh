#!/bin/bash
# Script to compile DubInstante Android App to APK
# Usage: ./build_android.sh

set -e

# Change to the directory of this script
cd "$(dirname "$0")"

BUILD_DIR="build-android"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Checking for qt-cmake..."
if ! command -v qt-cmake &> /dev/null; then
    echo "Error: qt-cmake not found in PATH."
    echo "Please ensure your Qt Android environment variables are set."
    echo "Example: export PATH=~/Qt/6.7.0/android_arm64_v8a/bin:\$PATH"
    echo "You also need ANDROID_SDK_ROOT and ANDROID_NDK_ROOT defined."
    exit 1
fi

echo "Configuring the Android project..."
qt-cmake ..

echo "Building the APK..."
cmake --build . --target DubInstanteAndroid_make_apk

echo "Successfully built the APK!"
echo "You can find it inside $BUILD_DIR/android-build/build/outputs/apk"
