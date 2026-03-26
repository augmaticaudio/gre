#!/bin/bash
# build_auv3.sh - Build Augmatic GRE with AUv3 support (macOS)
# Requires Xcode generator (Ninja cannot build AUv3)

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-auv3"

echo "============================================"
echo "Augmatic GRE AUv3 Build (macOS)"
echo "============================================"
echo ""

# Check for Xcode
if ! command -v xcodebuild &> /dev/null; then
    echo "ERROR: Xcode not found. AUv3 builds require Xcode."
    exit 1
fi

echo "Xcode version: $(xcodebuild -version | head -1)"
echo ""

# Bump version
echo "=== Bumping Version ==="
"$PROJECT_DIR/scripts-mac/bump_version.sh"
echo ""

# Get new version
NEW_VERSION=$(grep "project(AugmaticGRE VERSION" "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\).*/\1/')
echo "Building version: $NEW_VERSION"
echo ""

# Clean previous AUv3 build
if [ -d "$BUILD_DIR" ]; then
    echo "=== Cleaning Previous Build ==="
    rm -rf "$BUILD_DIR"
fi

# Configure with Xcode generator (REQUIRED for AUv3)
echo "=== Configuring CMake (Xcode Generator) ==="
cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DBUILD_ALL_FORMATS=ON

echo ""

# Build all targets
echo "=== Building All Formats ==="
xcodebuild -project "$BUILD_DIR/AugmaticGRE.xcodeproj" \
    -configuration Release \
    -jobs $(sysctl -n hw.ncpu) \
    -quiet

echo ""

# Install VST3
echo "=== Installing VST3 ==="
VST3_SOURCE="$BUILD_DIR/AugmaticGRE_artefacts/Release/VST3"
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"

if [ -d "$VST3_SOURCE" ]; then
    cp -r "$VST3_SOURCE"/*.vst3 "$VST3_DEST/"
    echo "VST3 installed to: $VST3_DEST"
else
    echo "WARNING: VST3 not found at $VST3_SOURCE"
fi

# Install AU
echo "=== Installing AU ==="
AU_SOURCE="$BUILD_DIR/AugmaticGRE_artefacts/Release/AU"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"

if [ -d "$AU_SOURCE" ]; then
    cp -r "$AU_SOURCE"/*.component "$AU_DEST/"
    echo "AU installed to: $AU_DEST"
else
    echo "WARNING: AU not found at $AU_SOURCE"
fi

# Install AUv3 (copy to Applications)
echo "=== Installing AUv3 ==="
AUV3_SOURCE="$BUILD_DIR/AugmaticGRE_artefacts/Release/AUv3"
AUV3_DEST="/Applications"

if [ -d "$AUV3_SOURCE" ]; then
    # Find the .app bundle
    APP_BUNDLE=$(find "$AUV3_SOURCE" -maxdepth 1 -name "*.app" | head -1)
    if [ -n "$APP_BUNDLE" ]; then
        APP_NAME=$(basename "$APP_BUNDLE")

        # Remove old version if exists
        if [ -d "$AUV3_DEST/$APP_NAME" ]; then
            rm -rf "$AUV3_DEST/$APP_NAME"
        fi

        cp -r "$APP_BUNDLE" "$AUV3_DEST/"
        echo "AUv3 app installed to: $AUV3_DEST/$APP_NAME"
        echo ""
        echo "IMPORTANT: Launch the app once to register AUv3:"
        echo "  open \"$AUV3_DEST/$APP_NAME\""
    else
        echo "WARNING: No .app bundle found in $AUV3_SOURCE"
    fi
else
    echo "WARNING: AUv3 not found at $AUV3_SOURCE"
fi

echo ""
echo "============================================"
echo "Build Complete - Augmatic GRE v$NEW_VERSION"
echo "============================================"
echo ""
echo "Installed:"
echo "  VST3: $VST3_DEST"
echo "  AU:   $AU_DEST"
echo "  AUv3: $AUV3_DEST"
echo ""
echo "To register AUv3 with the system:"
echo "  1. Launch the standalone app from /Applications"
echo "  2. Restart your DAW"
echo ""
echo "To validate:"
echo "  auval -a | grep \"Augmatic\""
echo "  pluginval --validate ~/Library/Audio/Plug-Ins/VST3/\"Augmatic GRE\"*.vst3"
