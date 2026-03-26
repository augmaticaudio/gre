cmake_minimum_required(VERSION 3.22)

# macOS deployment target (MUST be set BEFORE project() for Xcode to pick it up)
# Set to 11.0 for broad compatibility with Intel and Apple Silicon Macs
if(NOT DEFINED CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# Project Configuration
project(AugmaticGRE VERSION 1.0.1)
set(CMAKE_CXX_STANDARD 17)

# Generate dSYM files for Release builds (required for App Store crash symbolication)
set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES")

# Universal binary for CI tag builds (macOS only)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(DEFINED ENV{GITHUB_REF} AND "$ENV{GITHUB_REF}" MATCHES "^refs/tags/v")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    endif()
endif()

# Build options
option(BUILD_IOS "Build for iOS/iPadOS (requires Xcode generator)" OFF)

# Platform-specific format configuration
if(BUILD_IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # iOS: AUv3 and Standalone only
    set(PLUGIN_FORMATS "AUv3" "Standalone")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version")
    message(STATUS "Building for iOS - Formats: AUv3, Standalone")
else()
    # macOS: AUv3 + Standalone (Standalone app embeds AUv3 appex for DAW registration)
    set(PLUGIN_FORMATS "AUv3" "Standalone")
    message(STATUS "Building for macOS - Formats: AUv3, Standalone")
endif()

# JUCE Configuration
# Use environment variable JUCE_DIR if set, otherwise default to platform-specific path
if(DEFINED ENV{JUCE_DIR})
    add_subdirectory($ENV{JUCE_DIR} JUCE)
elseif(WIN32)
    add_subdirectory(C:/JUCE JUCE)
else()
    add_subdirectory(/Applications/JUCE JUCE)
endif()

# Plugin Configuration
juce_add_plugin(AugmaticGRE
# REDACTED
# REDACTED

    IS_SYNTH FALSE                   # Not a synth — Augmatic GRE is a MIDI processor
    NEEDS_MIDI_INPUT TRUE            # Accept MIDI input
    NEEDS_MIDI_OUTPUT TRUE           # Generate/output MIDI
    IS_MIDI_EFFECT TRUE              # MIDI Effect type (aumi) — correct for iPad DAW MIDI routing
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE     # Automatically copy to plugin folders

# REDACTED
    PLUGIN_CODE AgGR                 # Unique 4-character plugin code — LOCKED for production (never change after first release)

    FORMATS ${PLUGIN_FORMATS}        # Target formats (AUv3-only by default)
    PRODUCT_NAME "Augmatic GRE"      # Clean name without version (version shown in GUI)

    # App Icons - Custom Xcassets for AUv3 icon display in hosts (iOS + macOS)
    # ICON_BIG generates .icns for macOS standalone app icon
    # See todo/specs-icon.md for configuration details
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images.xcassets/AppIcon.appiconset/AppIcon-1024.png"
    CUSTOM_XCASSETS_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images.xcassets"

    # ============================================
    # AUv3 / macOS App Store / iOS Requirements
    # ============================================
    AU_SANDBOX_SAFE TRUE             # Required for Logic Pro validation
    HARDENED_RUNTIME_ENABLED TRUE   # Required for macOS App Store notarization

    # iOS/iPadOS Settings
    IPAD_SCREEN_ORIENTATIONS
        UIInterfaceOrientationPortrait
        UIInterfaceOrientationPortraitUpsideDown
        UIInterfaceOrientationLandscapeLeft
        UIInterfaceOrientationLandscapeRight

    BACKGROUND_AUDIO_ENABLED TRUE    # Keep audio when app is backgrounded

    # Files app visibility (iOS) - makes Documents folder visible in Files app
    FILE_SHARING_ENABLED TRUE
    # Merge additional plist keys: Files app support, Bluetooth MIDI, export compliance
    PLIST_TO_MERGE "<plist><dict><key>LSApplicationCategoryType</key><string>public.app-category.music</string><key>LSSupportsOpeningDocumentsInPlace</key><true/><key>NSBluetoothAlwaysUsageDescription</key><string>Augmatic GRE uses Bluetooth to connect to wireless MIDI controllers and instruments.</string><key>ITSAppUsesNonExemptEncryption</key><false/></dict></plist>"
)

# Source Files
target_sources(AugmaticGRE
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LinearDrummingMatrix.cpp
        Source/PresetManager.cpp
        Source/MidiMappingManager.cpp
        Source/StorageManager.cpp
        Source/UI/PresetPanel.cpp
        Source/UI/MidiMappingPanel.cpp
        Source/DSP/GridsEngine.cpp
        Source/DSP/EuclideanEngine.cpp
        Source/DSP/IndependentChaosController.cpp
        Source/DSP/VelocityController.cpp
        Source/DSP/MasterSyncController.cpp
        Source/DSP/InternalClockController.cpp
        Source/DSP/ClockDivider.cpp
        Source/DSP/SwingController.cpp
        Source/DSP/LinearDrummingController.cpp
        Source/DSP/AccentBenderController.cpp
        Source/UI/AccentBenderPanel.cpp
        Source/UI/XYPadAnimation.cpp
        Source/NativeTitleBar_mac.mm
        Source/iOS/DocumentsBookmarkManager.mm
)

# ============================================
# Factory Presets - Embedded as BinaryData
# ============================================
# Factory presets are staged with category prefixes to avoid name collisions
# across subfolders. Convention: CategoryName__PresetName.xml
# Root-level presets have no prefix. Subdirectories are auto-discovered.
# IMPORTANT: Adding/removing preset files requires re-running cmake configure.

set(PRESET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content/presets")
set(PRESET_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/staged_presets")

# Clean and recreate staging directory at configure time
file(REMOVE_RECURSE "${PRESET_STAGING_DIR}")
file(MAKE_DIRECTORY "${PRESET_STAGING_DIR}")

set(ALL_STAGED_PRESETS "")

# Root-level presets (no prefix)
file(GLOB ROOT_PRESETS CONFIGURE_DEPENDS "${PRESET_SOURCE_DIR}/*.xml")
foreach(PRESET_FILE ${ROOT_PRESETS})
    get_filename_component(PRESET_NAME "${PRESET_FILE}" NAME)
    configure_file("${PRESET_FILE}" "${PRESET_STAGING_DIR}/${PRESET_NAME}" COPYONLY)
    list(APPEND ALL_STAGED_PRESETS "${PRESET_STAGING_DIR}/${PRESET_NAME}")
endforeach()

# Category presets (auto-discover all subdirectories, Category__ prefix avoids name collisions)
file(GLOB PRESET_SUBDIRS LIST_DIRECTORIES true "${PRESET_SOURCE_DIR}/*")
foreach(SUBDIR ${PRESET_SUBDIRS})
    if(IS_DIRECTORY "${SUBDIR}")
        get_filename_component(CATEGORY "${SUBDIR}" NAME)
        file(GLOB CATEGORY_PRESETS CONFIGURE_DEPENDS "${SUBDIR}/*.xml")
        foreach(PRESET_FILE ${CATEGORY_PRESETS})
            get_filename_component(PRESET_NAME "${PRESET_FILE}" NAME)
            set(STAGED_NAME "${CATEGORY}__${PRESET_NAME}")
            configure_file("${PRESET_FILE}" "${PRESET_STAGING_DIR}/${STAGED_NAME}" COPYONLY)
            list(APPEND ALL_STAGED_PRESETS "${PRESET_STAGING_DIR}/${STAGED_NAME}")
        endforeach()
    endif()
endforeach()

# Create separate BinaryData target (avoids conflict with icon BinaryData)
if(ALL_STAGED_PRESETS)
    juce_add_binary_data(AugmaticGRE_FactoryPresets
        HEADER_NAME "FactoryPresetData.h"
        NAMESPACE FactoryPresetData
        SOURCES ${ALL_STAGED_PRESETS}
    )
    target_link_libraries(AugmaticGRE PRIVATE AugmaticGRE_FactoryPresets)
    target_compile_definitions(AugmaticGRE PRIVATE HAS_FACTORY_PRESETS=1)
    list(LENGTH ALL_STAGED_PRESETS PRESET_COUNT)
    message(STATUS "Factory presets embedded: ${PRESET_COUNT} files")
endif()

# ============================================
# Factory MIDI Mappings - Embedded as BinaryData
# ============================================
# Platform-specific: each platform has its own folder under content/midi-mapping/
#   macos/    — macOS builds
#   ipad/     — iPad/iOS builds (default for BUILD_IOS)
#   iphone/   — iPhone builds (future, requires BUILD_IPHONE=ON)
#
# Subfolders within the platform folder become menu categories in the app
# (e.g. macos/Native Instruments/Battery.xml → "Native Instruments" submenu).
# Staging convention: CategoryName__Filename.xml (same as presets).
# IMPORTANT: Adding/removing mapping files requires re-running cmake configure.

# Select platform-specific mapping source directory
if(BUILD_IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(MAPPING_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content/midi-mapping/ipad")
    message(STATUS "MIDI mappings: iPad platform (content/midi-mapping/ipad/)")
else()
    set(MAPPING_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content/midi-mapping/macos")
    message(STATUS "MIDI mappings: macOS platform (content/midi-mapping/macos/)")
endif()

set(MAPPING_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/staged_midi_mappings")

# Clean and recreate staging directory at configure time
file(REMOVE_RECURSE "${MAPPING_STAGING_DIR}")
file(MAKE_DIRECTORY "${MAPPING_STAGING_DIR}")

set(ALL_STAGED_MAPPINGS "")

# Root-level mappings (no category prefix)
file(GLOB ROOT_MAPPINGS CONFIGURE_DEPENDS "${MAPPING_SOURCE_DIR}/*.xml")
foreach(MAPPING_FILE ${ROOT_MAPPINGS})
    get_filename_component(MAPPING_NAME "${MAPPING_FILE}" NAME)
    configure_file("${MAPPING_FILE}" "${MAPPING_STAGING_DIR}/${MAPPING_NAME}" COPYONLY)
    list(APPEND ALL_STAGED_MAPPINGS "${MAPPING_STAGING_DIR}/${MAPPING_NAME}")
endforeach()

# Category mappings (auto-discover all subdirectories, Category__ prefix avoids name collisions)
file(GLOB MAPPING_SUBDIRS LIST_DIRECTORIES true "${MAPPING_SOURCE_DIR}/*")
foreach(SUBDIR ${MAPPING_SUBDIRS})
    if(IS_DIRECTORY "${SUBDIR}")
        get_filename_component(CATEGORY "${SUBDIR}" NAME)
        file(GLOB CATEGORY_MAPPINGS CONFIGURE_DEPENDS "${SUBDIR}/*.xml")
        foreach(MAPPING_FILE ${CATEGORY_MAPPINGS})
            get_filename_component(MAPPING_NAME "${MAPPING_FILE}" NAME)
            set(STAGED_NAME "${CATEGORY}__${MAPPING_NAME}")
            configure_file("${MAPPING_FILE}" "${MAPPING_STAGING_DIR}/${STAGED_NAME}" COPYONLY)
            list(APPEND ALL_STAGED_MAPPINGS "${MAPPING_STAGING_DIR}/${STAGED_NAME}")
        endforeach()
    endif()
endforeach()

# Create separate BinaryData target for MIDI mappings
if(ALL_STAGED_MAPPINGS)
    juce_add_binary_data(AugmaticGRE_FactoryMidiMappings
        HEADER_NAME "FactoryMidiMappingData.h"
        NAMESPACE FactoryMidiMappingData
        SOURCES ${ALL_STAGED_MAPPINGS}
    )
    target_link_libraries(AugmaticGRE PRIVATE AugmaticGRE_FactoryMidiMappings)
    target_compile_definitions(AugmaticGRE PRIVATE HAS_FACTORY_MIDI_MAPPINGS=1)
    list(LENGTH ALL_STAGED_MAPPINGS MAPPING_COUNT)
    message(STATUS "Factory MIDI mappings embedded: ${MAPPING_COUNT} files")
endif()

# Include Directories
target_include_directories(AugmaticGRE
    PRIVATE
        Source
        Source/DSP
)

# JUCE Module Links
target_link_libraries(AugmaticGRE
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices      # MidiOutput for hardware MIDI device support
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils        # Required for AUv3 (provides AUViewController)
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra          # PopupMenu and LookAndFeel for ComboBox styling
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compile Definitions
target_compile_definitions(AugmaticGRE
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ============================================
# dSYM Generation (required for App Store crash symbolication)
# ============================================
# JUCE sets GCC_GENERATE_DEBUGGING_SYMBOLS=NO for Release at the target level,
# which overrides project-level CMAKE_XCODE_ATTRIBUTE_* settings.
# Must set per-target to override JUCE's defaults.
# After archiving, run scripts-mac/fix_archive_dsyms.sh to copy dSYMs into the archive
# (CMake's custom CONFIGURATION_BUILD_DIR prevents Xcode from finding them automatically).
set_target_properties(AugmaticGRE_Standalone PROPERTIES
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
    XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES"
)
set_target_properties(AugmaticGRE_AUv3 PROPERTIES
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
    XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES"
)

# ============================================
# iOS App Groups Configuration
# ============================================
# App Groups allow sharing data between the standalone app and AUv3 extension
# This is CRITICAL for iOS - without it, presets/mappings won't be shared
if(BUILD_IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # App Group identifier (must match entitlements and StorageManager.h)
# REDACTED

    # Set entitlements for both the standalone app and AUv3 extension
    set_target_properties(AugmaticGRE_Standalone PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AugmaticGRE.entitlements"
    )

    set_target_properties(AugmaticGRE_AUv3 PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AugmaticGRE.entitlements"
    )

    message(STATUS "iOS App Groups enabled: ${APP_GROUP_ID}")
else()
    # macOS App Store: sandbox entitlements (required for Mac App Store submission)
    set_target_properties(AugmaticGRE_Standalone PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AugmaticGRE-macOS.entitlements"
    )
    set_target_properties(AugmaticGRE_AUv3 PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AugmaticGRE-macOS.entitlements"
    )
    message(STATUS "macOS App Store sandbox entitlements enabled")
endif()

# ============================================
# Privacy Manifest (required for App Store since May 2024)
# ============================================
# Apple requires PrivacyInfo.xcprivacy declaring Required Reason APIs used by JUCE:
# - UserDefaults (CA92.1), SystemBootTime (35F9.1), FileTimestamp (DDA9.1, C617.1)
# Must be embedded in BOTH the standalone app and AUv3 extension bundles.
set(PRIVACY_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/Resources/PrivacyInfo.xcprivacy")
set_source_files_properties(${PRIVACY_MANIFEST} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)
target_sources(AugmaticGRE_Standalone PRIVATE ${PRIVACY_MANIFEST})
if(BUILD_IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_sources(AugmaticGRE_AUv3 PRIVATE ${PRIVACY_MANIFEST})
    message(STATUS "Privacy manifest added to Standalone and AUv3 targets")
else()
    message(STATUS "Privacy manifest added to Standalone target")
endif()

# Build Tests (optional)
if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()