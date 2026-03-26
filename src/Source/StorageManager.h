#pragma once

#include <juce_core/juce_core.h>

#if JUCE_IOS
#include "iOS/DocumentsBookmarkManager.h"
#endif

/**
 * @brief Cross-platform storage manager with security-scoped bookmark support
 *
 * Provides unified access to storage paths for both standalone apps and AUv3 plugins.
 *
 * iOS architecture (v2 — security-scoped bookmarks):
 *   - Standalone: writes directly to Documents (visible in Files app)
 *   - AUv3 with bookmark: writes to bookmark-resolved Documents (visible in Files app)
 *   - AUv3 without bookmark: writes to App Group shared container (fallback, NOT visible in Files app)
 *
 * Factory and User content are separated into distinct subdirectories so that factory
 * cleanup can never destroy user data.
 *
 * macOS: Uses standard JUCE paths (no App Groups or bookmarks needed).
 */
class StorageManager
{
public:
    static StorageManager& getInstance();

    /**
     * @brief Get the root data directory for all plugin data
     *
     * iOS standalone: Documents directory (directly visible in Files app)
     * iOS AUv3 (bookmark): bookmark-resolved Documents directory
     * iOS AUv3 (no bookmark): App Group shared container
// REDACTED
     */
    juce::File getDataDirectory() const;

    // --- Presets directories ---
    juce::File getPresetsDirectory() const;        // .../Presets/
    juce::File getPresetsFactoryDirectory() const;  // .../Presets/Factory/
    juce::File getPresetsUserDirectory() const;     // .../Presets/User/

    // --- MIDI Mappings directories ---
    juce::File getMidiMappingsDirectory() const;          // .../MIDI Mappings/
    juce::File getMidiMappingsFactoryDirectory() const;   // .../MIDI Mappings/Factory/
    juce::File getMidiMappingsUserDirectory() const;      // .../MIDI Mappings/User/

    // --- Config directory ---
    juce::File getConfigDirectory() const;

    /** Initialize storage directories (creates them if they don't exist) */
    void initializeDirectories();

#if JUCE_IOS
    /** Get the DocumentsBookmarkManager for UI integration (bookmark setup button) */
    DocumentsBookmarkManager* getDocumentsBookmarkManager() { return bookmarkManager.get(); }

    /** Get the App Group directory (for fallback) */
    juce::File getAppGroupDirectory() const;
#endif

private:
    StorageManager();
    ~StorageManager() = default;

// REDACTED

    // Cached paths
    mutable juce::File cachedDataDirectory;
    mutable bool dataDirCached = false;

#if JUCE_IOS
    std::unique_ptr<DocumentsBookmarkManager> bookmarkManager;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StorageManager)
};
