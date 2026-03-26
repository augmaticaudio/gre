#!/bin/bash

# AUv3 Build and Install Script
# Builds AUv3 + Standalone using Xcode generator (required for AUv3)
# Installs to /Applications and registers with system
# Note: VST3 format available with -DBUILD_VST3=ON if needed for Ableton Live

set -e

echo "🔨 Building and installing Augmatic GRE AUv3 plugin..."

# Change to project directory
cd "$(dirname "$0")/.."
PROJECT_DIR="$(pwd)"

# CRITICAL: Build outside OneDrive to avoid resource fork issues with code signing
BUILD_DIR="$HOME/AugmaticGRE_builds/build"
mkdir -p "$BUILD_DIR"

# Clean user directories first to prevent conflicts
echo "🧹 Cleaning user directories..."
./scripts-mac/clean_user_plugins.sh

# Check for Xcode (required for AUv3)
if ! command -v xcodebuild &> /dev/null; then
    echo "❌ ERROR: Xcode not found. AUv3 builds require Xcode."
    exit 1
fi

# Test build FIRST with current version to ensure it compiles
echo ""
echo "🔍 Testing build with current version..."
echo "📁 Building in: $BUILD_DIR (outside OneDrive to avoid resource fork issues)"

if [ ! -f "$BUILD_DIR/AugmaticGRE.xcodeproj/project.pbxproj" ]; then
    echo "⚙️  Configuring CMake with Xcode generator..."
    cd "$PROJECT_DIR" && cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
fi

xcodebuild -project "$BUILD_DIR/AugmaticGRE.xcodeproj" -configuration Release -quiet

BUILD_EXIT_CODE=$?
if [ $BUILD_EXIT_CODE -ne 0 ]; then
    echo "❌ Build failed with exit code $BUILD_EXIT_CODE"
    echo "🚫 VERSION NOT BUMPED - Fix compilation errors first!"
    exit $BUILD_EXIT_CODE
fi

echo "✅ Test build successful!"

# Only bump version AFTER successful build
echo ""
echo "🔢 Auto-bumping version for AUv3 cache invalidation..."
./scripts-mac/bump_version.sh

# Reconfigure CMake to pick up new version
echo ""
echo "⚙️  Reconfiguring CMake with new version..."
cd "$PROJECT_DIR" && cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

# Build again with new version for final installation
echo ""
echo "⚙️  Building AUv3 plugin with new version..."
xcodebuild -project "$BUILD_DIR/AugmaticGRE.xcodeproj" -configuration Release -quiet

BUILD_EXIT_CODE=$?
if [ $BUILD_EXIT_CODE -ne 0 ]; then
    echo "❌ Final build failed with exit code $BUILD_EXIT_CODE"
    echo "⚠️  Version was bumped but final build failed - check for issues"
    exit $BUILD_EXIT_CODE
fi

echo ""
echo "✅ Build completed successfully!"

# Install AUv3 to /Applications
echo ""
echo "📦 Installing AUv3 app..."

# Get current version for versioned filename verification
CURRENT_VERSION=$(grep "project(AugmaticGRE VERSION" CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')

STANDALONE_SOURCE="$BUILD_DIR/AugmaticGRE_artefacts/Release/Standalone"
APP_DEST="/Applications"

if [ -d "$STANDALONE_SOURCE" ]; then
    # Find the .app bundle
    APP_BUNDLE=$(find "$STANDALONE_SOURCE" -maxdepth 1 -name "*.app" | head -1)
    if [ -n "$APP_BUNDLE" ]; then
        APP_NAME=$(basename "$APP_BUNDLE")

        # Remove ALL old Augmatic GRE versions (identity is now shared across versions)
        echo "🧹 Removing old Augmatic GRE versions from /Applications..."
        find "$APP_DEST" -maxdepth 1 -name "Augmatic GRE*" -type d | while read -r old_app; do
            echo "   Removing: $(basename "$old_app")"
            rm -rf "$old_app"
        done

        cp -r "$APP_BUNDLE" "$APP_DEST/"
        echo "✅ AUv3 app installed to: $APP_DEST/$APP_NAME"

        # Launch the app to register AUv3 with the system
        echo ""
        echo "🚀 Launching app to register AUv3 with system..."
        open "$APP_DEST/$APP_NAME"

        # Wait a moment for registration
        sleep 2

        echo "✅ AUv3 registered with system"
    else
        echo "❌ ERROR: No .app bundle found in $STANDALONE_SOURCE"
        exit 1
    fi
else
    echo "❌ ERROR: Standalone app not found at $STANDALONE_SOURCE"
    exit 1
fi

echo ""
echo "🎵 AUv3 plugin ready for testing!"
echo ""
echo "📋 Next steps:"
echo "   1. Restart your DAW completely"
echo "   2. Look for 'Augmatic GRE' in AUv3 instruments"
echo "   3. Test in Logic Pro, GarageBand, or other AUv3 hosts"
echo ""
echo "🔍 To validate AUv3 registration:"
echo "   auval -a | grep 'Augmatic'"

# CRITICAL RULE #14: Create ZIP archive after compilation
echo ""
echo "📦 Creating project ZIP archive (CLAUDE.md Rule #14)..."
CURRENT_VERSION=$(grep "project(AugmaticGRE VERSION" CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')

# Clean up old build artifacts to reduce ZIP size
echo "🧹 Cleaning old build artifacts..."
ARTIFACTS_SIZE_BEFORE=$(du -sh "$BUILD_DIR/AugmaticGRE_artefacts" 2>/dev/null | awk '{print $1}' || echo "0")
echo "   Build artifacts before cleanup: $ARTIFACTS_SIZE_BEFORE"

# Remove all old versioned files except the current version
find "$BUILD_DIR/AugmaticGRE_artefacts" -name "*v0.3.*" -not -name "*v${CURRENT_VERSION}*" -type f -delete 2>/dev/null || true
find "$BUILD_DIR/AugmaticGRE_artefacts" -name "*v0.3.*" -not -name "*v${CURRENT_VERSION}*" -type d -delete 2>/dev/null || true

# Clean up any .a files that are not current version
find "$BUILD_DIR/AugmaticGRE_artefacts" -name "*.a" -not -name "*v${CURRENT_VERSION}*" -delete 2>/dev/null || true

ARTIFACTS_SIZE_AFTER=$(du -sh "$BUILD_DIR/AugmaticGRE_artefacts" 2>/dev/null | awk '{print $1}' || echo "0")
echo "   Build artifacts after cleanup: $ARTIFACTS_SIZE_AFTER"

cd ..
ZIP_NAME="AugmaticGRE_v${CURRENT_VERSION}.zip"

if [ -f "${ZIP_NAME}" ]; then
    echo "⚠️  Removing existing ZIP: ${ZIP_NAME}"
    rm "${ZIP_NAME}"
fi

zip -r "${ZIP_NAME}" "Augmatic GRE"/ -x "Augmatic GRE/build/*" "Augmatic GRE/.git/*" "Augmatic GRE/Resources/GUI/node_modules/*" > /dev/null 2>&1

if [ -f "${ZIP_NAME}" ]; then
    ZIP_SIZE=$(ls -la "${ZIP_NAME}" | awk '{print $5}')
    echo "✅ ZIP created: ${ZIP_NAME} (${ZIP_SIZE} bytes)"
else
    echo "❌ ZIP creation FAILED - CRITICAL RULE #14 VIOLATION"
    exit 1
fi

cd "Augmatic GRE"