#!/bin/bash

# Auto Version Bumper for Augmatic GRE Plugin
# Version format: MAJOR.MINOR or MAJOR.MINOR.PATCH
# Automatically increments patch version to force DAW cache invalidation
#
# NOTE: PLUGIN_CODE is now locked to "AgGR" for production.
# It is never changed by this script. Only the version number increments.

echo "🔢 Auto-bumping plugin version..."

# REDACTED

# Read current version from CMakeLists.txt
CURRENT_VERSION=$(grep "project(AugmaticGRE VERSION" "$CMAKE_FILE" | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')

if [ -z "$CURRENT_VERSION" ]; then
    echo "❌ Could not find current version in CMakeLists.txt"
    exit 1
fi

echo "📋 Current version: $CURRENT_VERSION"

# Parse version components
IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION"

# If patch is empty, set it to 0
if [ -z "$PATCH" ]; then
    PATCH=0
fi

# Remove leading zeros for arithmetic (bash interprets 008 as octal)
PATCH=$((10#$PATCH))

# Increment patch version
PATCH=$((PATCH + 1))

# Check for overflow
if [ "$PATCH" -gt 999 ]; then
    echo "❌ Patch version overflow! Maximum is 999. Consider incrementing MINOR."
    exit 1
fi

NEW_VERSION="$MAJOR.$MINOR.$PATCH"

echo "🚀 Bumping to version: $NEW_VERSION"

# Update CMakeLists.txt
sed -i '' "s/project(AugmaticGRE VERSION $CURRENT_VERSION)/project(AugmaticGRE VERSION $NEW_VERSION)/" "$CMAKE_FILE"

# Verify the change
UPDATED_VERSION=$(grep "project(AugmaticGRE VERSION" "$CMAKE_FILE" | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')

if [ "$UPDATED_VERSION" = "$NEW_VERSION" ]; then
    echo "✅ Version successfully updated to $NEW_VERSION"
    echo ""
    echo "🎯 Changes made:"
    echo "   Version: $CURRENT_VERSION → $NEW_VERSION"
    echo "   Plugin Code: AgGR (locked — not changed)"
else
    echo "❌ Failed to update version"
    exit 1
fi
