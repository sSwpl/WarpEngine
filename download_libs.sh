#!/bin/bash

# Configuration
WGPU_VERSION="v0.19.1.1" # Check for latest stable release if needed
OS=$(uname -s)
ARCH=$(uname -m)

echo "Detected OS: $OS"
echo "Detected Arch: $ARCH"

# Determine download URL based on OS/Arch
if [ "$OS" == "Darwin" ]; then
    if [ "$ARCH" == "arm64" ]; then
        FILE="wgpu-macos-aarch64-release.zip"
    else
        FILE="wgpu-macos-x86_64-release.zip"
    fi
elif [ "$OS" == "Linux" ]; then
    FILE="wgpu-linux-x86_64-release.zip"
elif [ "$OS" == "MINGW"* ] || [ "$OS" == "CYGWIN"* ]; then
    FILE="wgpu-windows-x86_64-release.zip"
else
    echo "Unsupported OS: $OS"
    exit 1
fi

URL="https://github.com/gfx-rs/wgpu-native/releases/download/${WGPU_VERSION}/${FILE}"
TARGET_DIR="external/wgpu"

echo "Downloading wgpu-native ($FILE)..."
echo "URL: $URL"

# Create directory
mkdir -p "$TARGET_DIR"

# Download and unzip
curl -L -o "$TARGET_DIR/$FILE" "$URL"
unzip -o "$TARGET_DIR/$FILE" -d "$TARGET_DIR"

# Reorganize into standard structure
mkdir -p "$TARGET_DIR/lib"
mkdir -p "$TARGET_DIR/include/webgpu"

mv "$TARGET_DIR"/libwgpu_native.dylib "$TARGET_DIR/lib/" 2>/dev/null
mv "$TARGET_DIR"/libwgpu_native.so "$TARGET_DIR/lib/" 2>/dev/null
mv "$TARGET_DIR"/wgpu_native.dll "$TARGET_DIR/lib/" 2>/dev/null
mv "$TARGET_DIR"/wgpu_native.lib "$TARGET_DIR/lib/" 2>/dev/null
mv "$TARGET_DIR"/*.a "$TARGET_DIR/lib/" 2>/dev/null # Static libs if any

mv "$TARGET_DIR"/*.h "$TARGET_DIR/include/webgpu/" 2>/dev/null

rm "$TARGET_DIR/$FILE"

echo "✅ wgpu-native installed in $TARGET_DIR"
echo "Headers in $TARGET_DIR/include/webgpu"
echo "Libraries in $TARGET_DIR/lib"
