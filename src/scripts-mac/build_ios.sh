#!/bin/bash
# build_ios.sh - Configure Augmatic GRE for iOS/iPad build
# This script prepares the Xcode project for iOS deployment
# Actual deployment must be done through Xcode (for signing)
# CRITICAL: Builds outside OneDrive to avoid resource fork issues

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# CRITICAL: Build outside OneDrive to avoid code signing failures
BUILD_DIR="$HOME/AugmaticGRE_builds/build-ios"
mkdir -p "$BUILD_DIR"

echo "============================================"
echo "Augmatic GRE iOS Build Configuration"
echo "============================================"
echo ""

# Check for Xcode
if ! command -v xcodebuild &> /dev/null; then
    echo "ERROR: Xcode not found. iOS builds require Xcode."
    exit 1
fi

echo "Xcode version: $(xcodebuild -version | head -1)"
echo ""

# Auto-bump version (same as macOS build_and_install.sh)
echo "=== Auto-Bumping Version ==="
"$PROJECT_DIR/scripts-mac/bump_version.sh"
echo ""

# Get new version
CURRENT_VERSION=$(grep "project(AugmaticGRE VERSION" "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\).*/\1/')
echo "Building version: $CURRENT_VERSION"
echo ""

# Clean previous iOS build
if [ -d "$BUILD_DIR" ]; then
    echo "=== Cleaning Previous Build ==="
    rm -rf "$BUILD_DIR"
fi

# Configure for iOS with Xcode generator
echo "=== Configuring CMake for iOS ==="
echo "Building in: $BUILD_DIR (outside OneDrive to avoid resource fork issues)"
echo ""
cd "$PROJECT_DIR" && cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DBUILD_IOS=ON

echo ""
echo "============================================"
echo "iOS Project Configured Successfully"
echo "============================================"
echo ""
echo "Xcode project created at:"
echo "  $BUILD_DIR/AugmaticGRE.xcodeproj"
echo ""
echo "Next steps:"
echo "  1. Open Xcode project:"
echo "     open \"$BUILD_DIR/AugmaticGRE.xcodeproj\""
echo ""
echo "  2. In Xcode:"
echo "     - Select 'AugmaticGRE_Standalone' target"
echo "     - Go to Signing & Capabilities"
echo "     - Enable 'Automatically manage signing'"
echo "     - Select your Team (Apple ID)"
echo ""
echo "  3. Connect iPad via USB"
echo ""
echo "  4. Select iPad as destination in Xcode toolbar"
echo ""
echo "  5. Press Cmd+R to build and deploy"
echo ""
echo "  6. After install, launch the app on iPad to register AUv3"
echo ""
echo "  7. Test in GarageBand or AUM on iPad"
echo ""
echo "  For App Store upload:"
echo "     ./scripts-mac/archive_ios.sh"
echo "     Then in Xcode Organizer: select archive > Distribute App > App Store Connect"
echo ""

# Open Xcode project
read -p "Open Xcode project now? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    open "$BUILD_DIR/AugmaticGRE.xcodeproj"
fi
