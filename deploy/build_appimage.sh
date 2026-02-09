#!/bin/bash
set -e

# --- Configuration ---
APP_NAME="DubInstante"
BUILD_DIR="build_release"
DIST_DIR="dist_appimage"
DESKTOP_FILE="deploy/dubinstante.desktop"

# Extract version from CHANGELOG.md (e.g., [0.3.4])
export VERSION=$(grep -m 1 "## \[" CHANGELOG.md | sed -n 's/.*\[\([0-9.]*\)\].*/\1/p')
echo "--- Building $APP_NAME version $VERSION ---"

# --- Tools ---
LINUXDEPLOY="linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_PLUGIN="linuxdeploy-plugin-qt-x86_64.AppImage"

# --- 1. Build Project ---
echo "--- Compiling $APP_NAME in Release mode ---"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

# --- 2. Setup Tools ---
echo "--- Ensuring deployment tools are present ---"
mkdir -p tools
cd tools

if [ ! -f "$LINUXDEPLOY" ]; then
    wget -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_QT_PLUGIN" ]; then
    wget -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT_PLUGIN"
fi

export PATH="$(pwd):$PATH"
cd ..

# --- 3. Package AppImage ---
echo "--- Packaging AppImage ---"
mkdir -p "$DIST_DIR"

# Set environment variables for linuxdeploy-plugin-qt
export QMAKE="/usr/lib/qt6/bin/qmake"
export MAKEFLAGS="-j$(nproc)"
export EXTRA_QT_PLUGINS="multimedia"

./tools/$LINUXDEPLOY --appdir "$DIST_DIR/AppDir" \
    --executable "$BUILD_DIR/$APP_NAME" \
    --desktop-file "$DESKTOP_FILE" \
    --icon-file "DubInstante.png" \
    --plugin qt \
    --output appimage

# --- 4. Custom Rename ---
FINAL_NAME="${APP_NAME}_linux_${VERSION}.AppImage"
echo "--- Finalizing: $FINAL_NAME ---"
mv DubInstante*.AppImage "$FINAL_NAME"

echo ""
echo "=========================================================="
echo "SUCCESS! Your AppImage is ready:"
echo "$FINAL_NAME"
echo "=========================================================="
