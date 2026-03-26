#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

/**
 * @class DocumentsBookmarkManager
 * @brief iOS-only: manages a security-scoped bookmark for the standalone app's Documents directory
 *
 * The AUv3 extension uses this to gain persistent read/write access to the standalone
 * app's Documents directory (visible in the iOS Files app). The standalone app does NOT
 * need a bookmark — it accesses its own Documents directly.
 *
 * Usage:
 *   1. Standalone: call getResolvedDocumentsRoot() → returns Documents directly
 *   2. AUv3 (no bookmark): returns empty File → caller should fall back to App Group
 *   3. AUv3 (with bookmark): returns bookmark-resolved Documents root
 *
 * The user must interact with a UIDocumentPickerViewController once to grant access.
 * After that, the bookmark persists across launches (but not across delete+reinstall).
 *
 * CRITICAL: The AUv3 extension must create its own bookmark — a bookmark created by the
 * standalone app cannot be resolved by the extension (Apple DTS confirmed).
 */
class DocumentsBookmarkManager
{
public:
    DocumentsBookmarkManager();
    ~DocumentsBookmarkManager();

    /**
     * @brief Check if a valid, resolved bookmark exists
     * @return true if the AUv3 has bookmark access to the Documents directory
     */
    bool hasValidBookmark() const;

    /**
     * @brief Present the iOS folder picker to grant Documents access
     *
     * Only meaningful when running as AUv3 extension. The picker is pre-navigated to the
     * app's Documents directory. The user taps "Open" to confirm access.
     *
     * @param parentComponent JUCE component to anchor the picker (for UIViewController lookup)
     * @param callback Fires after the user selects a folder (true) or cancels (false)
     */
    void requestBookmark(juce::Component* parentComponent,
                         std::function<void(bool success)> callback);

    /**
     * @brief Get the resolved Documents root directory
     *
     * - Standalone: returns File::getSpecialLocation(userDocumentsDirectory)
     * - AUv3 with bookmark: returns bookmark-resolved Documents URL
     * - AUv3 without bookmark: returns empty File()
     *
     * @return The Documents root, or empty File if unavailable
     */
    juce::File getResolvedDocumentsRoot() const;

private:
    // Pimpl to hide Objective-C types from C++ headers
    class Impl;
    std::unique_ptr<Impl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DocumentsBookmarkManager)
};
