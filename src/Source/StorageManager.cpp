#include "StorageManager.h"

StorageManager& StorageManager::getInstance()
{
    static StorageManager instance;
    return instance;
}

StorageManager::StorageManager()
{
    dataDirCached = false;

#if JUCE_IOS
    bookmarkManager = std::make_unique<DocumentsBookmarkManager>();
#endif
}

juce::File StorageManager::getDataDirectory() const
{
    // Return cached value if available
    if (dataDirCached)
        return cachedDataDirectory;

#if JUCE_IOS
    // iOS v2 architecture: Documents is the single source of truth.
    // Standalone: Documents directly. AUv3: bookmark-resolved Documents or App Group fallback.

    if (!juce::SystemStats::isRunningInAppExtensionSandbox())
    {
        // Standalone app: use Documents directory directly (always visible in Files app)
        cachedDataDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        dataDirCached = true;
        return cachedDataDirectory;
    }

    // AUv3 extension: try bookmark first, fall back to App Group
    if (bookmarkManager != nullptr)
    {
        auto resolved = bookmarkManager->getResolvedDocumentsRoot();
        if (resolved != juce::File())
        {
            // Bookmark resolved successfully — write directly to Documents
            cachedDataDirectory = resolved;
            dataDirCached = true;
            return cachedDataDirectory;
        }
    }

    // AUv3 fallback: App Group shared container (not visible in Files app)
    {
        auto container = juce::File::getContainerForSecurityApplicationGroupIdentifier(APP_GROUP_ID);
        if (container != juce::File())
        {
            cachedDataDirectory = container.getChildFile("AugmaticGRE");
            dataDirCached = true;
            return cachedDataDirectory;
        }
    }

    // Last resort fallback (should not happen)
    jassertfalse;
    cachedDataDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    dataDirCached = true;
    return cachedDataDirectory;

#elif JUCE_MAC
    // macOS: Use standard sandboxing-safe location
    // No App Groups needed - both standalone and AUv3 can access the same paths
    cachedDataDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
// REDACTED
    dataDirCached = true;
    return cachedDataDirectory;

#else
    #error "Augmatic GRE only supports macOS and iOS (AUv3). Windows/Linux builds are not supported."
#endif
}

// --- Presets directories ---

juce::File StorageManager::getPresetsDirectory() const
{
    return getDataDirectory().getChildFile("Presets");
}

juce::File StorageManager::getPresetsFactoryDirectory() const
{
    return getPresetsDirectory().getChildFile("Factory");
}

juce::File StorageManager::getPresetsUserDirectory() const
{
    return getPresetsDirectory().getChildFile("User");
}

// --- MIDI Mappings directories ---

juce::File StorageManager::getMidiMappingsDirectory() const
{
    return getDataDirectory().getChildFile("MIDI Mappings");
}

juce::File StorageManager::getMidiMappingsFactoryDirectory() const
{
    return getMidiMappingsDirectory().getChildFile("Factory");
}

juce::File StorageManager::getMidiMappingsUserDirectory() const
{
    return getMidiMappingsDirectory().getChildFile("User");
}

// --- Config directory ---

juce::File StorageManager::getConfigDirectory() const
{
    return getDataDirectory().getChildFile("Config");
}

void StorageManager::initializeDirectories()
{
    auto dataDir = getDataDirectory();
    if (!dataDir.exists())
        dataDir.createDirectory();

    // Presets: root, Factory, User
    auto presetsDir = getPresetsDirectory();
    if (!presetsDir.exists())
        presetsDir.createDirectory();

    auto presetsFactory = getPresetsFactoryDirectory();
    if (!presetsFactory.exists())
        presetsFactory.createDirectory();

    auto presetsUser = getPresetsUserDirectory();
    if (!presetsUser.exists())
        presetsUser.createDirectory();

    // MIDI Mappings: root, Factory, User
    auto mappingsDir = getMidiMappingsDirectory();
    if (!mappingsDir.exists())
        mappingsDir.createDirectory();

    auto mappingsFactory = getMidiMappingsFactoryDirectory();
    if (!mappingsFactory.exists())
        mappingsFactory.createDirectory();

    auto mappingsUser = getMidiMappingsUserDirectory();
    if (!mappingsUser.exists())
        mappingsUser.createDirectory();

    // Config
    auto configDir = getConfigDirectory();
    if (!configDir.exists())
        configDir.createDirectory();
}

// ============================================================================
// iOS-specific: migration and App Group access
// ============================================================================

#if JUCE_IOS

juce::File StorageManager::getAppGroupDirectory() const
{
    auto container = juce::File::getContainerForSecurityApplicationGroupIdentifier(APP_GROUP_ID);
    if (container != juce::File())
        return container.getChildFile("AugmaticGRE");
    return {};
}

#endif // JUCE_IOS
