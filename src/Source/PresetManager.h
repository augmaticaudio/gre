#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

/**
 * @brief Manages preset loading, saving, and organization for the Augmatic GRE plugin
 *
 * The PresetManager handles:
 * - User preset save/load from platform-specific directories
 * - Factory preset loading from BinaryData
 * - Preset metadata and versioning
 * - Cross-platform file management
 */
class PresetManager
{
public:
    /**
     * @brief Construct a new PresetManager
     *
     * @param processor The audio processor instance
     * @param apvts The AudioProcessorValueTreeState for parameter management
     */
    PresetManager(juce::AudioProcessor& processor,
                  juce::AudioProcessorValueTreeState& apvts);

    ~PresetManager() = default;

    // Core preset operations
    void savePreset(const juce::String& presetName);
    bool loadPreset(const juce::String& presetName, const juce::String& category = "");
    bool deletePreset(const juce::String& presetName, const juce::String& category = "");
    void savePresetAs(const juce::File& file);
    bool loadPresetFromFile(const juce::File& file);
    void randomizeFromPresets();  // Randomize parameters by picking values from existing presets

    // Preset list management
    juce::StringArray getAllPresetNames() const;
    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getUserPresetNames() const;

    // Preset category/subfolder support
    struct PresetItem
    {
        juce::String name;
        juce::String category;  // Subfolder name (empty for root)
        bool isFactory;
    };
    std::vector<PresetItem> getAllPresetsWithCategories() const;

    // Current preset state
    const juce::String& getCurrentPresetName() const { return currentPresetName; }
    const juce::String& getCurrentPresetCategory() const { return currentPresetCategory; }
    juce::File getCurrentPresetFile() const;
    bool isCurrentPresetModified() const { return currentPresetModified; }
    void markPresetAsModified() { currentPresetModified = true; }
    void clearModifiedFlag() { currentPresetModified = false; }

    // Directory access
    juce::File getUserPresetDirectory() const;
    juce::File getFactoryPresetDirectory() const;

    // Preset navigation
    bool loadNextPreset();
    bool loadPreviousPreset();
    int getCurrentPresetIndex() const;

    // Refresh preset lists
    void rescanUserPresets();

    // Callback fired after a preset is loaded (for UI refresh)
    std::function<void()> onPresetLoaded;

private:
    struct PresetInfo
    {
        juce::String name;
        juce::File file;
        std::unique_ptr<juce::XmlElement> xml;  // For embedded factory presets
        bool isFactory = false;
        juce::String category;  // Optional categorization
    };

    // Private helper methods
    void initializePresetDirectories();
    void migratePresetsIfNeeded();
    void installFactoryPresetsIfNeeded();
    void loadFactoryPresets();
    void scanUserPresets();
    bool loadPresetFromXml(juce::XmlElement* xml, const juce::String& presetName);
    std::unique_ptr<juce::XmlElement> createPresetXml(const juce::String& presetName);
    juce::File getFactoryPresetsVersionFile() const;
    juce::File getFactoryManifestFile() const;
    void saveFactoryManifest(const juce::StringArray& relativePaths) const;
    static double parsePresetVersion(const juce::String& xmlContent);

    // Platform-specific directory helpers
    juce::File getPresetsBaseDirectory() const;

    // Member variables
    juce::AudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& parameters;

    std::vector<PresetInfo> factoryPresets;
    std::vector<PresetInfo> userPresets;
    juce::String currentPresetName;
    juce::String currentPresetCategory;
    bool currentPresetModified = false;
    int currentPresetIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};