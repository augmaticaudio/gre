#!/bin/bash

# Clean User Plugin Directory Script
# Removes old Augmatic GRE plugin versions to prevent DAW cache conflicts

echo "Cleaning old plugin versions..."

USER_VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
USER_AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

echo "Cleaning VST3 plugins from: $USER_VST3_DIR"

# Remove all Augmatic GRE VST3 plugins
if [ -d "$USER_VST3_DIR" ]; then
    find "$USER_VST3_DIR" -name "Augmatic GRE*.vst3" -type d 2>/dev/null | while read -r plugin; do
        echo "  Removing: $(basename "$plugin")"
        rm -rf "$plugin"
    done
else
    echo "  VST3 directory not found: $USER_VST3_DIR"
fi

echo "Cleaning AU plugins from: $USER_AU_DIR"

# Remove all Augmatic GRE AU plugins
if [ -d "$USER_AU_DIR" ]; then
    find "$USER_AU_DIR" -name "Augmatic GRE*.component" -type d 2>/dev/null | while read -r plugin; do
        echo "  Removing: $(basename "$plugin")"
        rm -rf "$plugin"
    done
else
    echo "  AU directory not found: $USER_AU_DIR"
fi

echo "Plugin cleanup complete!"

# Verify cleanup
REMAINING_VST3=$(find "$USER_VST3_DIR" -name "Augmatic GRE*.vst3" -type d 2>/dev/null | wc -l)
REMAINING_AU=$(find "$USER_AU_DIR" -name "Augmatic GRE*.component" -type d 2>/dev/null | wc -l)

if [ "$REMAINING_VST3" -eq 0 ] && [ "$REMAINING_AU" -eq 0 ]; then
    echo "All plugins successfully removed"
else
    echo "Some plugins may remain - manual cleanup might be needed"
    if [ "$REMAINING_VST3" -gt 0 ]; then
        echo "   VST3 plugins remaining: $REMAINING_VST3"
    fi
    if [ "$REMAINING_AU" -gt 0 ]; then
        echo "   AU plugins remaining: $REMAINING_AU"
    fi
fi
