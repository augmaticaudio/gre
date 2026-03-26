#!/bin/bash

# Plugin Installation Permissions Setup
# Run this script in Terminal to set up permissions for automated plugin installation

echo "🔐 Plugin Installation Permissions Setup"
echo "======================================="
echo ""
echo "This script will set up permissions so you can install plugins to system"
echo "directories without repeated password prompts during development."
echo ""

# Check if we have write access to system plugin directories
SYSTEM_VST3_DIR="/Library/Audio/Plug-Ins/VST3"
SYSTEM_AU_DIR="/Library/Audio/Plug-Ins/Components"

echo "📁 Checking current permissions..."

# Test write access to VST3 directory
if [ -w "$SYSTEM_VST3_DIR" ]; then
    echo "✅ Already have write access to VST3 directory"
    VST3_OK=true
else
    echo "⚠️  Need write access to VST3 directory"
    VST3_NEEDS_SUDO=true
fi

# Test write access to AU directory  
if [ -w "$SYSTEM_AU_DIR" ]; then
    echo "✅ Already have write access to AU directory"
    AU_OK=true
else
    echo "⚠️  Need write access to AU directory"
    AU_NEEDS_SUDO=true
fi

# If we need sudo access, provide instructions
if [ "$VST3_NEEDS_SUDO" = true ] || [ "$AU_NEEDS_SUDO" = true ]; then
    echo ""
    echo "🛠️  To set up permissions, run these commands in Terminal:"
    echo ""
    
    if [ "$VST3_NEEDS_SUDO" = true ]; then
        echo "# Grant write access to VST3 directory:"
        echo "sudo chown -R \$(whoami):admin \"$SYSTEM_VST3_DIR\""
        echo "sudo chmod u+w \"$SYSTEM_VST3_DIR\""
        echo ""
    fi
    
    if [ "$AU_NEEDS_SUDO" = true ]; then
        echo "# Grant write access to AU directory:"
        echo "sudo chown -R \$(whoami):admin \"$SYSTEM_AU_DIR\""  
        echo "sudo chmod u+w \"$SYSTEM_AU_DIR\""
        echo ""
    fi
    
    echo "After running these commands, plugin builds will install automatically!"
else
    echo ""
    echo "✅ All permissions already configured!"
fi

echo ""
echo "🧪 Testing current plugin installation access..."

# Test VST3 installation
TEST_FILE="$SYSTEM_VST3_DIR/.test_write"
if touch "$TEST_FILE" 2>/dev/null; then
    rm "$TEST_FILE"
    echo "✅ VST3 directory write access confirmed"
else
    echo "❌ VST3 directory needs permissions setup"
fi

# Test AU installation
TEST_FILE="$SYSTEM_AU_DIR/.test_write"
if touch "$TEST_FILE" 2>/dev/null; then
    rm "$TEST_FILE"
    echo "✅ AU directory write access confirmed"  
else
    echo "❌ AU directory needs permissions setup"
fi

echo ""
echo "🎯 Target plugin directories:"
echo "   VST3: $SYSTEM_VST3_DIR"
echo "   AU:   $SYSTEM_AU_DIR"
echo ""
echo "📋 After setup, use: ./Scripts/build_and_install.sh"
echo "🧹 Always run: ./Scripts/clean_user_plugins.sh to avoid conflicts"