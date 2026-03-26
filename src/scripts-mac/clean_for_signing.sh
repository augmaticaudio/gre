#!/bin/bash
# Clean extended attributes before code signing
# OneDrive adds resource forks that break codesign

BUILD_DIR="$1"

if [ -z "$BUILD_DIR" ]; then
    echo "Usage: $0 <build_directory>"
    exit 1
fi

echo "Cleaning extended attributes in: $BUILD_DIR"

# Remove all extended attributes recursively
find "$BUILD_DIR" -name "*.appex" -o -name "*.app" | while read bundle; do
    xattr -cr "$bundle" 2>/dev/null || true
    echo "  Cleaned: $(basename "$bundle")"
done

echo "✅ Extended attributes cleaned"
