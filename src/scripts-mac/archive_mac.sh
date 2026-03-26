#!/bin/bash
# archive_mac.sh - Archive macOS build for App Store submission
#
# This script:
# 1. Configures the macOS Xcode project (Universal Binary, without version bump)
# 2. Archives via xcodebuild
# 3. Copies dSYMs into the archive (CMake workaround)
# 4. Opens Xcode Organizer for upload
#
# Usage: ./scripts-mac/archive_mac.sh
# Signing is handled by Xcode Organizer during "Distribute App".

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$HOME/AugmaticGRE_builds/build-mac"

echo "============================================"
echo "Augmatic GRE — macOS App Store Archive"
echo "============================================"
echo ""

# Get current version
CURRENT_VERSION=$(grep "project(AugmaticGRE VERSION" "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\).*/\1/')
echo "Version: $CURRENT_VERSION"
echo ""

# Configure cmake
echo "=== Configuring CMake for macOS (Universal Binary) ==="
rm -rf "$BUILD_DIR"
cd "$PROJECT_DIR" && cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

# Archive
echo ""
echo "=== Archiving ==="
ARCHIVE_PATH="$HOME/AugmaticGRE_builds/AugmaticGRE-macOS.xcarchive"
rm -rf "$ARCHIVE_PATH"

SIGNING_ARGS=""
xcodebuild archive \
    -project "$BUILD_DIR/AugmaticGRE.xcodeproj" \
    -scheme AugmaticGRE_Standalone \
    -destination "generic/platform=macOS" \
    -archivePath "$ARCHIVE_PATH" \
    CODE_SIGNING_ALLOWED=NO \
    CODE_SIGNING_REQUIRED=NO \
    -quiet

echo ""
echo "=== Fixing dSYMs ==="

# Copy dSYMs from build directory into archive
APP_DSYM="$BUILD_DIR/AugmaticGRE_artefacts/Release/Standalone/Augmatic GRE.app.dSYM"
APPEX_DSYM="$BUILD_DIR/AugmaticGRE_artefacts/Release/AUv3/Augmatic GRE.appex.dSYM"
DSYM_DIR="$ARCHIVE_PATH/dSYMs"
mkdir -p "$DSYM_DIR"

FIXED=0
if [ -d "$APP_DSYM" ]; then
    cp -R "$APP_DSYM" "$DSYM_DIR/"
    FIXED=$((FIXED + 1))
fi
if [ -d "$APPEX_DSYM" ]; then
    cp -R "$APPEX_DSYM" "$DSYM_DIR/"
    FIXED=$((FIXED + 1))
fi

if [ "$FIXED" -eq 2 ]; then
    echo "dSYMs copied into archive ($FIXED/2)"
else
    echo "WARNING: Only $FIXED/2 dSYMs found. Upload may show warnings."
fi

# Move archive to Xcode's archive directory so Organizer can see it
DATE_DIR="$HOME/Library/Developer/Xcode/Archives/$(date +%Y-%m-%d)"
mkdir -p "$DATE_DIR"
FINAL_ARCHIVE="$DATE_DIR/AugmaticGRE_Standalone $(date +%d-%m-%Y,\ %H.%M).xcarchive"
mv "$ARCHIVE_PATH" "$FINAL_ARCHIVE"

echo ""
echo "============================================"
echo "Archive ready for upload"
echo "============================================"
echo ""
echo "Archive: $(basename "$FINAL_ARCHIVE")"
echo ""
echo "Opening Xcode Organizer..."
open -a Xcode

echo ""
echo "In Organizer: select the archive > Distribute App > App Store Connect"
