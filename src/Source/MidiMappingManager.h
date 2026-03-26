#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>

/**
 * @brief Manages MIDI note and channel mapping save/load independently of presets
 *
 * The MidiMappingManager handles:
 * - Saving MIDI note and channel assignments to XML files
 * - Loading MIDI note and channel assignments from XML files
 * - Managing user mapping files
 * - Ensuring mapping persistence across preset changes
 *
 * CRITICAL: Handles these 6 MIDI note parameters:
 * - bd_note, sn_note, hh_note, bd_acc_note, sn_acc_note, hh_acc_note
 * Note: MIDI channel parameters removed from APVTS in v0.4.171 (hardcoded Ch 10 since v0.4.119).
 * Mapping XML still writes hardcoded channel values for backward compatibility.
 *
 * ADDITIONALLY: Stores MIDI output device name (v0.3.447) separately as it's not an APVTS parameter
 */
class MidiMappingManager
{
public:
    /**
     * @brief Construct a new MidiMappingManager
     *
     * @param apvts The AudioProcessorValueTreeState for parameter access
     */
    MidiMappingManager(juce::AudioProcessorValueTreeState& apvts);

    ~MidiMappingManager() = default;

    // Core mapping operations
    void saveMapping(const juce::String& mappingName);
    bool loadMapping(const juce::String& mappingName);
    bool deleteMapping(const juce::String& mappingName);
    void saveMappingAs(const juce::File& file);
    bool loadMappingFromFile(const juce::File& file);

    // Mapping list management
    juce::StringArray getAllMappingNames() const;

    // Category support (subfolder → submenu, matching PresetManager pattern)
    struct MappingItem
    {
        juce::String name;
        juce::String category;  // Subfolder name (empty for root)
    };
    std::vector<MappingItem> getAllMappingsWithCategories() const;

    // Current mapping state
    const juce::String& getCurrentMappingName() const { return currentMappingName; }
    juce::String getCurrentMappingCategory() const
    {
        if (currentMappingIndex >= 0 && currentMappingIndex < static_cast<int>(userMappings.size()))
            return userMappings[currentMappingIndex].category;
        return {};
    }
    bool isCurrentMappingModified() const { return currentMappingModified; }
    void markMappingAsModified() { currentMappingModified = true; }
    void clearModifiedFlag() { currentMappingModified = false; }

    // Directory access
    juce::File getUserMappingDirectory() const;

    // Mapping navigation
    bool loadNextMapping();
    bool loadPreviousMapping();
    int getCurrentMappingIndex() const;

    // Refresh mapping lists
    void rescanUserMappings();

    // MIDI Output Device management (v0.3.447)
    void setMidiOutputDevice(const juce::String& deviceName);
    juce::String getMidiOutputDevice() const { return currentMidiOutputDevice; }

    // Instrument Name management (v0.4.196)
    // Display names up to 3 chars. Accent defaults include "'" suffix.
    static constexpr const char* DEFAULT_INSTRUMENT_NAMES[] = {"BD", "SN", "HH", "BD'", "SN'", "HH'"};

    void setInstrumentName(int index, const juce::String& name);
    juce::String getInstrumentName(int index) const;       // Returns stored name as-is (max 3 chars)
    juce::String getInstrumentNameLong(int index) const;   // Main: as-is, Accent: strip "'" + " Acc"
    const std::array<juce::String, 6>& getInstrumentNames() const { return customInstrumentNames; }

    class InstrumentNameListener {
    public:
        virtual ~InstrumentNameListener() = default;
        virtual void instrumentNamesChanged() = 0;
    };
    void addInstrumentNameListener(InstrumentNameListener* l);
    void removeInstrumentNameListener(InstrumentNameListener* l);

private:
    struct MappingInfo
    {
        juce::String name;
        juce::File file;
        juce::String category;  // Subfolder name (empty for root)
    };

    // MIDI note parameter IDs (6 total — channel params removed from APVTS in v0.4.171)
    static const juce::StringArray MIDI_NOTE_PARAMS;

    // Private helper methods
    void initializeMappingDirectories();
    void installFactoryMappingsIfNeeded();
    juce::File getFactoryMappingsVersionFile() const;
    juce::File getFactoryManifestFile() const;
    void saveFactoryManifest(const juce::StringArray& relativePaths) const;
    void scanUserMappings();
    bool loadMappingFromXml(juce::XmlElement* xml, const juce::String& mappingName);
    std::unique_ptr<juce::XmlElement> createMappingXml(const juce::String& mappingName);

    // Platform-specific directory helpers
    juce::File getMappingsBaseDirectory() const;

    // Member variables
    juce::AudioProcessorValueTreeState& parameters;

    std::vector<MappingInfo> userMappings;
    juce::String currentMappingName;
    bool currentMappingModified = false;
    int currentMappingIndex = -1;

    // MIDI Output Device (v0.3.447) - stored in mapping but not in APVTS
    juce::String currentMidiOutputDevice = "Plugin MIDI Output (Default)";

    // Custom instrument names (v0.4.196) - 6 entries, max 3 chars each
    std::array<juce::String, 6> customInstrumentNames = {"BD", "SN", "HH", "BD'", "SN'", "HH'"};
    juce::ListenerList<InstrumentNameListener> instrumentNameListeners;
    void notifyInstrumentNameListeners();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMappingManager)
};
