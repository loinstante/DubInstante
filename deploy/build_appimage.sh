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

# --- 2b. Fetch static FFmpeg ---
# Une build statique n'a aucune dépendance partagée : rien à déployer dans
# l'AppDir, rien à casser sur une distribution plus ancienne que celle du build.
if [ ! -x "tools/ffmpeg" ] || [ ! -x "tools/ffprobe" ]; then
    echo "--- Downloading static FFmpeg (GPL) ---"
    mkdir -p tools/ffmpeg-static
    wget -c "https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz" \
        -O tools/ffmpeg-static.tar.xz
    tar -xJf tools/ffmpeg-static.tar.xz -C tools/ffmpeg-static
    # Le dossier extrait porte le numéro de version : on retrouve les binaires
    # plutôt que de coder en dur un chemin qui changera à la prochaine release.
    for tool in ffmpeg ffprobe; do
        found=$(find tools/ffmpeg-static -type f -name "$tool" | head -n 1)
        if [ -z "$found" ]; then
            echo "ERROR: $tool not found in the downloaded archive"
            exit 1
        fi
        cp "$found" "tools/$tool"
        chmod +x "tools/$tool"
    done
fi

# --- 3. Package AppImage ---
echo "--- Packaging AppImage ---"
mkdir -p "$DIST_DIR"

# FFmpeg va dans usr/bin, à côté de l'exécutable : c'est là que
# ExportService::toolPath() le cherche une fois l'AppImage montée.
mkdir -p "$DIST_DIR/AppDir/usr/bin"
cp tools/ffmpeg tools/ffprobe "$DIST_DIR/AppDir/usr/bin/"
cp LICENSE THIRD_PARTY_LICENSES "$DIST_DIR/AppDir/"

if ldd "$DIST_DIR/AppDir/usr/bin/ffmpeg" > /dev/null 2>&1; then
    echo "ERROR: the downloaded ffmpeg is dynamically linked, it will not run on a clean machine"
    exit 1
fi
"$DIST_DIR/AppDir/usr/bin/ffmpeg" -version | head -n 1

# Set environment variables for linuxdeploy-plugin-qt
export QMAKE=$(which qmake6)
if [ -z "$QMAKE" ]; then
    export QMAKE=$(which qmake)
fi
if [ -z "$QMAKE" ]; then
    export QMAKE="/usr/lib/qt6/bin/qmake"
fi
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

# --- 5. Verify the packaged AppImage really carries FFmpeg ---
# linuxdeploy retraite le contenu de l'AppDir : on relit le paquet produit
# plutôt que l'AppDir, sinon on ne vérifie pas ce qui est publié.
rm -rf squashfs-root
./"$FINAL_NAME" --appimage-extract "usr/bin/ffmpeg" > /dev/null
./"$FINAL_NAME" --appimage-extract "usr/bin/ffprobe" > /dev/null
for tool in ffmpeg ffprobe; do
    if [ ! -x "squashfs-root/usr/bin/$tool" ]; then
        echo "ERROR: $tool is missing from $FINAL_NAME"
        exit 1
    fi
    "squashfs-root/usr/bin/$tool" -version > /dev/null
done
rm -rf squashfs-root
echo "--- FFmpeg and FFprobe verified inside the AppImage ---"

echo ""
echo "=========================================================="
echo "SUCCESS! Your AppImage is ready:"
echo "$FINAL_NAME"
echo "=========================================================="
