#!/bin/bash
# ============================================================================
# DubInstante Android APK Builder - Local Setup & Build
# ============================================================================
# This script downloads everything needed to compile the phonegui app into
# an .apk file, directly on your Linux machine. No Android Studio required.
#
# Prerequisites:
#   - A .env file in this directory with JAVA_HOME set (see .env.example)
#   - pip3 (for aqtinstall)
#   - wget, unzip, cmake
#
# What it downloads (into ./android-toolchain, NOT system-wide):
#   - Android command-line tools + SDK (platform 33, build-tools)
#   - Android NDK 25c
#   - Qt 6.5.3 for Android arm64 + host desktop tools (via aqtinstall)
#
# Usage:
#   cd src/phonegui
#   ./setup_and_build_apk.sh
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# ── Load .env ───────────────────────────────────────────────────
ENV_FILE="$SCRIPT_DIR/.env"
if [ -f "$ENV_FILE" ]; then
    echo "📄 Loading .env..."
    set -a
    source "$ENV_FILE"
    set +a
else
    echo "❌ No .env file found. Create one from .env.example:"
    echo "   cp .env.example .env"
    echo "   # then edit JAVA_HOME to point to your JDK"
    exit 1
fi

TOOLCHAIN_DIR="$SCRIPT_DIR/android-toolchain"
QT_VERSION="6.5.3"
NDK_VERSION="25.2.9519653"
BUILD_TOOLS_VERSION="33.0.2"
PLATFORM_VERSION="android-33"

echo "╔══════════════════════════════════════════════════════════╗"
echo "║       DubInstante Android APK Builder                   ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# ── Step 0: Resolve & check Java ────────────────────────────────
# Make relative JAVA_HOME absolute
if [[ "$JAVA_HOME" != /* ]]; then
    JAVA_HOME="$SCRIPT_DIR/$JAVA_HOME"
fi
export JAVA_HOME
export PATH="$JAVA_HOME/bin:$PATH"

if [ ! -x "$JAVA_HOME/bin/javac" ]; then
    echo "❌ javac not found at $JAVA_HOME/bin/javac"
    echo "   Check JAVA_HOME in your .env file."
    exit 1
fi
JAVA_VER=$("$JAVA_HOME/bin/javac" -version 2>&1 | grep -oP '\d+' | head -1)
echo "✅ Java $JAVA_VER detected at $JAVA_HOME"

# ── Step 1: Install aqtinstall (Qt installer) ───────────────────
VENV_DIR="$TOOLCHAIN_DIR/venv"
if [ ! -d "$VENV_DIR" ]; then
    echo "📦 Creating Python virtual environment for aqtinstall..."
    mkdir -p "$TOOLCHAIN_DIR"
    python3 -m venv "$VENV_DIR" || {
        echo "❌ Failed to create virtual environment. Please install python3-venv:"
        echo "   sudo apt install python3-venv"
        exit 1
    }
fi

echo "📦 Ensuring aqtinstall is installed in venv..."
"$VENV_DIR/bin/pip" install aqtinstall --quiet
AQT_EXE="$VENV_DIR/bin/aqt"
echo "✅ aqtinstall ready"

# ── Step 2: Download Android SDK ────────────────────────────────
ANDROID_SDK_ROOT="$TOOLCHAIN_DIR/android-sdk"
if [ ! -d "$ANDROID_SDK_ROOT/cmdline-tools" ]; then
    echo "📦 Downloading Android command-line tools..."
    mkdir -p "$ANDROID_SDK_ROOT"
    CMDLINE_URL="https://dl.google.com/android/repository/commandlinetools-linux-10406996_latest.zip"
    wget -q --show-progress -O /tmp/cmdline-tools.zip "$CMDLINE_URL"
    unzip -q /tmp/cmdline-tools.zip -d "$ANDROID_SDK_ROOT/cmdline-tools-tmp"
    mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools/latest"
    mv "$ANDROID_SDK_ROOT/cmdline-tools-tmp/cmdline-tools/"* "$ANDROID_SDK_ROOT/cmdline-tools/latest/"
    rm -rf "$ANDROID_SDK_ROOT/cmdline-tools-tmp" /tmp/cmdline-tools.zip
fi
echo "✅ Android SDK command-line tools ready"

export ANDROID_SDK_ROOT
export PATH="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$PATH"

# ── Step 3: Install SDK packages + NDK ──────────────────────────
if [ ! -d "$ANDROID_SDK_ROOT/ndk/$NDK_VERSION" ]; then
    echo "📦 Installing Android SDK packages and NDK (this may take a while)..."
    yes | sdkmanager --licenses > /dev/null 2>&1 || true
    sdkmanager "platform-tools" "platforms;$PLATFORM_VERSION" "build-tools;$BUILD_TOOLS_VERSION" "ndk;$NDK_VERSION"
fi
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$NDK_VERSION"
echo "✅ Android NDK $NDK_VERSION ready"

# ── Step 4: Download Qt for Android + host tools ────────────────
QT_ANDROID_DIR="$TOOLCHAIN_DIR/Qt/$QT_VERSION/android_arm64_v8a"
QT_HOST_DIR="$TOOLCHAIN_DIR/Qt/$QT_VERSION/gcc_64"

if [ ! -d "$QT_ANDROID_DIR" ]; then
    echo "📦 Downloading Qt $QT_VERSION for Android arm64 (this may take a while)..."
    "$AQT_EXE" install-qt linux android "$QT_VERSION" android_arm64_v8a \
        -m qtmultimedia qt5compat \
        --outputdir "$TOOLCHAIN_DIR/Qt"
fi
echo "✅ Qt $QT_VERSION Android arm64 ready"

if [ ! -d "$QT_HOST_DIR" ]; then
    echo "📦 Downloading Qt $QT_VERSION host (desktop) tools..."
    "$AQT_EXE" install-qt linux desktop "$QT_VERSION" gcc_64 \
        --outputdir "$TOOLCHAIN_DIR/Qt"
fi
echo "✅ Qt $QT_VERSION host tools ready"

# ── Step 5: Build the APK ───────────────────────────────────────
echo ""
echo "🔨 Building the APK..."
echo ""

BUILD_DIR="$SCRIPT_DIR/build-android"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

"$QT_ANDROID_DIR/bin/qt-cmake" "$SCRIPT_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQT_HOST_PATH="$QT_HOST_DIR" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"

cmake --build . --target apk

# ── Done ────────────────────────────────────────────────────────
APK_PATH=$(find . -name "*.apk" -type f | head -1)
if [ -n "$APK_PATH" ]; then
    FULL_APK="$(cd "$(dirname "$APK_PATH")" && pwd)/$(basename "$APK_PATH")"
    echo ""
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║  ✅ APK built successfully!                             ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo ""
    echo "📱 APK location:"
    echo "   $FULL_APK"
    echo ""
    echo "To install on your phone via ADB:"
    echo "   adb install $FULL_APK"
else
    echo "❌ APK not found. Check build output above for errors."
    exit 1
fi
