#!/bin/bash
# fix_archive_dsyms.sh - Copy dSYMs into the latest Xcode archive
#
# CMake sets a custom CONFIGURATION_BUILD_DIR, which causes Xcode's Archive
# process to not find/copy dSYM files into the archive. This script copies
# them manually from the build directory into the archive's dSYMs folder.
#
# Supports both iOS and macOS archives (auto-detected).
#
# Run this AFTER archiving in Xcode, BEFORE uploading to App Store Connect.

set -e

BUILD_DIR_IOS="$HOME/AugmaticGRE_builds/build-ios"
BUILD_DIR_MAC="$HOME/AugmaticGRE_builds/build"

# Find the latest archive
ARCHIVE_DIR="$HOME/Library/Developer/Xcode/Archives"
LATEST_ARCHIVE=$(find "$ARCHIVE_DIR" -name "*.xcarchive" -maxdepth 2 -print0 2>/dev/null | xargs -0 ls -dt 2>/dev/null | head -1)

if [ -z "$LATEST_ARCHIVE" ]; then
    echo "ERROR: No Xcode archives found in $ARCHIVE_DIR"
    echo "Archive your project in Xcode first (Product > Archive)"
    exit 1
fi

echo "============================================"
echo "Fix Archive dSYMs"
echo "============================================"
echo ""
echo "Archive: $(basename "$LATEST_ARCHIVE")"

# Detect platform from archive contents
# iOS binaries are flat (Augmatic GRE.app/Augmatic GRE)
# macOS binaries use Contents/MacOS (Augmatic GRE.app/Contents/MacOS/Augmatic GRE)
if [ -d "$LATEST_ARCHIVE/Products/Applications/Augmatic GRE.app/Contents/MacOS" ]; then
    PLATFORM="macOS"
    BUILD_DIR="$BUILD_DIR_MAC"
    APP_BINARY="$LATEST_ARCHIVE/Products/Applications/Augmatic GRE.app/Contents/MacOS/Augmatic GRE"
    APPEX_BINARY="$LATEST_ARCHIVE/Products/Applications/Augmatic GRE.app/Contents/PlugIns/Augmatic GRE.appex/Contents/MacOS/Augmatic GRE"
else
    PLATFORM="iOS"
    BUILD_DIR="$BUILD_DIR_IOS"
    APP_BINARY="$LATEST_ARCHIVE/Products/Applications/Augmatic GRE.app/Augmatic GRE"
    APPEX_BINARY="$LATEST_ARCHIVE/Products/Applications/Augmatic GRE.app/PlugIns/Augmatic GRE.appex/Augmatic GRE"
fi

echo "Platform: $PLATFORM"
echo "Build:    $BUILD_DIR"
echo ""

# Check for dSYM files in build directory
APP_DSYM="$BUILD_DIR/AugmaticGRE_artefacts/Release/Standalone/Augmatic GRE.app.dSYM"
APPEX_DSYM="$BUILD_DIR/AugmaticGRE_artefacts/Release/AUv3/Augmatic GRE.appex.dSYM"

MISSING=0
if [ ! -d "$APP_DSYM" ]; then
    echo "ERROR: .app dSYM not found at: $APP_DSYM"
    MISSING=1
fi
if [ ! -d "$APPEX_DSYM" ]; then
    echo "ERROR: .appex dSYM not found at: $APPEX_DSYM"
    MISSING=1
fi
if [ "$MISSING" -eq 1 ]; then
    echo ""
    echo "dSYM files are generated during Xcode Archive."
    echo "Make sure you archived from the project at: $BUILD_DIR/AugmaticGRE.xcodeproj"
    exit 1
fi

# Copy dSYMs into archive
DSYM_DIR="$LATEST_ARCHIVE/dSYMs"
mkdir -p "$DSYM_DIR"

cp -R "$APP_DSYM" "$DSYM_DIR/"
cp -R "$APPEX_DSYM" "$DSYM_DIR/"

echo "Copied dSYMs into archive:"
ls -1 "$DSYM_DIR/"

# Verify UUIDs match
echo ""
echo "=== UUID Verification ==="

# For Universal binaries (macOS), dwarfdump outputs multiple UUIDs — compare all
APP_BIN_UUIDS=$(dwarfdump --uuid "$APP_BINARY" 2>/dev/null | awk '{print $2}' | sort)
APP_SYM_UUIDS=$(dwarfdump --uuid "$DSYM_DIR/Augmatic GRE.app.dSYM" 2>/dev/null | awk '{print $2}' | sort)

APPEX_BIN_UUIDS=$(dwarfdump --uuid "$APPEX_BINARY" 2>/dev/null | awk '{print $2}' | sort)
APPEX_SYM_UUIDS=$(dwarfdump --uuid "$DSYM_DIR/Augmatic GRE.appex.dSYM" 2>/dev/null | awk '{print $2}' | sort)

VERIFIED=1
if [ "$APP_BIN_UUIDS" = "$APP_SYM_UUIDS" ] && [ -n "$APP_BIN_UUIDS" ]; then
    echo ".app    UUID match: $(echo "$APP_BIN_UUIDS" | tr '\n' ' ')"
else
    echo "WARNING: .app UUID mismatch!"
    echo "  Binary: $APP_BIN_UUIDS"
    echo "  dSYM:   $APP_SYM_UUIDS"
    VERIFIED=0
fi

if [ "$APPEX_BIN_UUIDS" = "$APPEX_SYM_UUIDS" ] && [ -n "$APPEX_BIN_UUIDS" ]; then
    echo ".appex  UUID match: $(echo "$APPEX_BIN_UUIDS" | tr '\n' ' ')"
else
    echo "WARNING: .appex UUID mismatch!"
    echo "  Binary: $APPEX_BIN_UUIDS"
    echo "  dSYM:   $APPEX_SYM_UUIDS"
    VERIFIED=0
fi

echo ""
if [ "$VERIFIED" -eq 1 ]; then
    echo "All UUIDs verified. You can now upload from Xcode Organizer."
else
    echo "WARNING: UUID mismatches detected. Re-archive and run this script again."
    exit 1
fi
