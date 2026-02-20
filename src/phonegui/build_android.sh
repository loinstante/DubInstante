#!/bin/bash
# Script to compile DubInstante Android App to APK
# Usage: ./build_android.sh
#
# Prerequisites:
#   - Qt 6.x for Android (arm64_v8a) installed
#   - ANDROID_SDK_ROOT and ANDROID_NDK_ROOT set
#   - Qt host tools (desktop) also installed for cross-compilation (moc, rcc, etc.)
#
# Example setup:
#   export ANDROID_SDK_ROOT=~/Android/Sdk
#   export ANDROID_NDK_ROOT=~/Android/Sdk/ndk/<version>
#   export QT_ANDROID=~/Qt/6.5.3/android_arm64_v8a
#   export QT_HOST=~/Qt/6.5.3/gcc_64

set -e

cd "$(dirname "$0")"

BUILD_DIR="build-android"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Check required env vars
if [ -z "$QT_ANDROID" ]; then
    echo "Error: QT_ANDROID is not set."
    echo "Set it to your Qt Android kit path, e.g.:"
    echo "  export QT_ANDROID=~/Qt/6.5.3/android_arm64_v8a"
    exit 1
fi

if [ -z "$QT_HOST" ]; then
    echo "Error: QT_HOST is not set."
    echo "Set it to your Qt desktop (host tools) path, e.g.:"
    echo "  export QT_HOST=~/Qt/6.5.3/gcc_64"
    exit 1
fi

if [ -z "$ANDROID_SDK_ROOT" ]; then
    echo "Error: ANDROID_SDK_ROOT is not set."
    exit 1
fi

if [ -z "$ANDROID_NDK_ROOT" ]; then
    echo "Error: ANDROID_NDK_ROOT is not set."
    exit 1
fi

echo "Configuring the Android project..."
"$QT_ANDROID/bin/qt-cmake" .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQT_HOST_PATH="$QT_HOST"

echo "Building the APK..."
cmake --build . --target apk

echo ""
echo "Successfully built the APK!"
echo "You can find it inside: $(pwd)/android-build/build/outputs/apk/"
