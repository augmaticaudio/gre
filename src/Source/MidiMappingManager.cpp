#include "MidiMappingManager.h"
#include "StorageManager.h"

#if HAS_FACTORY_MIDI_MAPPINGS
#include "FactoryMidiMappingData.h"
#endif

// MIDI note parameters included in mappings (6 total)
// MIDI channel parameters removed from APVTS in v0.4.171 (hardcoded Ch 10 since v0.4.119)
// NOTE: MIDI output device (v0.3.447) is stored separately in XML as it's not an APVTS parameter
const juce::StringArray MidiMappingManager::MIDI_NOTE_PARAMS = {
    "bd_note",
    "sn_note",
    "hh_note",
    "bd_acc_note",
    "sn_acc_note",
    "hh_acc_note"
};

// Channel param IDs kept for backward-compatible XML reading (v0.3.403–v0.4.170 files)
static const juce::StringArray MIDI_CHANNEL_PARAMS = {
    "bd_midi_channel",
    "sn_midi_channel",
    "hh_midi_channel",
    "bd_acc_midi_channel",
    "sn_acc_midi_channel",
    "hh_acc_midi_channel"
};

// MIDI Note Name Conversion Utilities
namespace MidiNoteConversion
{
    // Note names for one octave
    static const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    /**
     * Convert MIDI note number (0-127) to human-readable note name (e.g., "C3", "A#5")
     * GUI convention: MIDI 0 = C-2, MIDI 24 = C0, MIDI 60 = C3
     * Matches MIDITabComponent display (octave = midiNote/12 - 2)
     */
    static juce::String midiNoteToName(int midiNote)
    {
        if (midiNote < 0 || midiNote > 127)
            return "C0";  // Fallback for invalid notes

        int octave = (midiNote / 12) - 2;  // GUI convention offset
        int noteIndex = midiNote % 12;

        return juce::String(NOTE_NAMES[noteIndex]) + juce::String(octave);
    }

    /**
     * Convert note name (e.g., "C4", "A#5") to MIDI note number (0-127)
     * Returns -1 if parsing fails
     */
    static int nameToMidiNote(const juce::String& noteName)
    {
        if (noteName.isEmpty())
            return -1;

        juce::String name = noteName.trim().toUpperCase();

        // Parse note letter and accidental
        int noteIndex = -1;
        int pos = 0;

        // CRITICAL: Check sharps BEFORE naturals to avoid "F" matching "F#"
        // Loop through in REVERSE order so longer names (sharps) are checked first
        for (int i = 11; i >= 0; --i)
        {
            juce::String testNote(NOTE_NAMES[i]);
            if (name.startsWith(testNote))
            {
                noteIndex = i;
                pos = testNote.length();
                break;
            }
        }

        if (noteIndex == -1)
            return -1;  // Invalid note name

        // Parse octave number
        juce::String octaveStr = name.substring(pos);
        int octave = octaveStr.getIntValue();

        // Handle negative octaves (e.g., C-2 = MIDI note 0)
        if (octaveStr.startsWith("-"))
            octave = -octaveStr.substring(1).getIntValue();

        // Calculate MIDI note number (GUI convention: octave -2 = MIDI 0)
        int midiNote = (octave + 2) * 12 + noteIndex;

        // Clamp to valid MIDI range
        if (midiNote < 0) midiNote = 0;
        if (midiNote > 127) midiNote = 127;

        return midiNote;
    }

    /**
     * Convert normalized parameter value (0.0-1.0) to MIDI note number (0-127)
     */
    static int normalizedToMidiNote(float normalized)
    {
        return juce::roundToInt(normalized * 127.0f);
    }

}

MidiMappingManager::MidiMappingManager(juce::AudioProcessorValueTreeState& apvts)
    : parameters(apvts)
{
    // Initialize directories on construction
    initializeMappingDirectories();

    // Install factory MIDI mappings from BinaryData (first launch or version update)
    installFactoryMappingsIfNeeded();

    // Scan for user mappings
    scanUserMappings();
}

void MidiMappingManager::saveMapping(const juce::String& mappingName)
{
    auto mappingDir = getUserMappingDirectory();

    // If the current mapping was loaded from a category subfolder, save back there
    if (currentMappingIndex >= 0 && currentMappingIndex < static_cast<int>(userMappings.size())
        && userMappings[currentMappingIndex].name == mappingName
        && userMappings[currentMappingIndex].category.isNotEmpty())
    {
        mappingDir = mappingDir.getChildFile(userMappings[currentMappingIndex].category);
    }

    // Ensure the directory exists
    if (!mappingDir.exists())
        mappingDir.createDirectory();

    juce::File mappingFile = mappingDir.getChildFile(mappingName + ".xml");

    auto xml = createMappingXml(mappingName);

    if (xml != nullptr)
    {
        bool success = xml->writeTo(mappingFile);

        if (success)
        {
            currentMappingName = mappingName;
            currentMappingModified = false;
            scanUserMappings(); // Refresh list
        }
    }
}

bool MidiMappingManager::loadMapping(const juce::String& mappingName)
{
    // Search user mappings
    for (size_t i = 0; i < userMappings.size(); ++i)
    {
        if (userMappings[i].name == mappingName)
        {
            currentMappingIndex = static_cast<int>(i);
            return loadMappingFromFile(userMappings[i].file);
        }
    }

    return false;
}

bool MidiMappingManager::deleteMapping(const juce::String& mappingName)
{
    for (auto& mapping : userMappings)
    {
        if (mapping.name == mappingName)
        {
            if (mapping.file.existsAsFile())
            {
            #if JUCE_IOS
                // iOS: moveToTrash() fails in sandboxed containers — use deleteFile()
                bool success = mapping.file.deleteFile();
            #else
                bool success = mapping.file.moveToTrash();
            #endif
                if (success)
                {
                    scanUserMappings(); // Refresh list
                    if (currentMappingName == mappingName)
                    {
                        currentMappingName.clear();
                        currentMappingIndex = -1;
                    }
                }
                return success;
            }
        }
    }
    return false;
}

void MidiMappingManager::saveMappingAs(const juce::File& file)
{
    auto xml = createMappingXml(file.getFileNameWithoutExtension());

    if (xml != nullptr)
    {
        bool success = xml->writeTo(file);
        if (success)
        {
            currentMappingName = file.getFileNameWithoutExtension();
            currentMappingModified = false;
            scanUserMappings(); // Refresh list
        }
    }
}

bool MidiMappingManager::loadMappingFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(file);

    if (xml != nullptr)
    {
        return loadMappingFromXml(xml.get(), file.getFileNameWithoutExtension());
    }

    return false;
}

juce::StringArray MidiMappingManager::getAllMappingNames() const
{
    juce::StringArray names;

    for (const auto& mapping : userMappings)
        names.add(mapping.name);

    return names;
}

juce::File MidiMappingManager::getUserMappingDirectory() const
{
    // User mappings go to the User/ subdirectory (separate from Factory/)
    return StorageManager::getInstance().getMidiMappingsUserDirectory();
}

bool MidiMappingManager::loadNextMapping()
{
    if (userMappings.empty())
        return false;

    int nextIndex = currentMappingIndex + 1;
    if (nextIndex >= static_cast<int>(userMappings.size()))
        nextIndex = 0;

    return loadMapping(userMappings[nextIndex].name);
}

bool MidiMappingManager::loadPreviousMapping()
{
    if (userMappings.empty())
        return false;

    int prevIndex = currentMappingIndex - 1;
    if (prevIndex < 0)
        prevIndex = static_cast<int>(userMappings.size()) - 1;

    return loadMapping(userMappings[prevIndex].name);
}

int MidiMappingManager::getCurrentMappingIndex() const
{
    return currentMappingIndex;
}

void MidiMappingManager::rescanUserMappings()
{
    scanUserMappings();
}

void MidiMappingManager::setMidiOutputDevice(const juce::String& deviceName)
{
    currentMidiOutputDevice = deviceName;
    markMappingAsModified();
}

void MidiMappingManager::setInstrumentName(int index, const juce::String& name)
{
    if (index >= 0 && index < 6 && name.isNotEmpty())
    {
        customInstrumentNames[index] = name.substring(0, 3);
        markMappingAsModified();
        notifyInstrumentNameListeners();
    }
}

juce::String MidiMappingManager::getInstrumentName(int index) const
{
    if (index < 0 || index >= 6) return "";
    return customInstrumentNames[index];
}

juce::String MidiMappingManager::getInstrumentNameLong(int index) const
{
    if (index < 0 || index >= 6) return "";
    if (index < 3)
        return customInstrumentNames[index];
    // Accent channels: strip trailing "'" if present, append " Acc"
    juce::String base = customInstrumentNames[index];
    if (base.endsWithChar('\''))
        base = base.dropLastCharacters(1);
    return base + " Acc";
}

void MidiMappingManager::addInstrumentNameListener(InstrumentNameListener* l)
{
    instrumentNameListeners.add(l);
}

void MidiMappingManager::removeInstrumentNameListener(InstrumentNameListener* l)
{
    instrumentNameListeners.remove(l);
}

void MidiMappingManager::notifyInstrumentNameListeners()
{
    instrumentNameListeners.call(&InstrumentNameListener::instrumentNamesChanged);
}

void MidiMappingManager::initializeMappingDirectories()
{
    // Use StorageManager to initialize all directories (including App Group on iOS)
    StorageManager::getInstance().initializeDirectories();
}

juce::File MidiMappingManager::getFactoryMappingsVersionFile() const
{
    return StorageManager::getInstance().getConfigDirectory()
               .getChildFile(".factory_midi_mappings_version");
}

void MidiMappingManager::installFactoryMappingsIfNeeded()
{
#if HAS_FACTORY_MIDI_MAPPINGS
    auto versionFile = getFactoryMappingsVersionFile();
    juce::String currentVersion = JucePlugin_VersionString;

    if (versionFile.existsAsFile())
    {
        juce::String installedVersion = versionFile.loadFileAsString().trim();
        if (installedVersion == currentVersion)
            return;
    }

    // Factory mappings are installed to the Factory/ subdirectory (never User/)
    auto factoryDir = StorageManager::getInstance().getMidiMappingsFactoryDirectory();
    if (!factoryDir.exists())
        factoryDir.createDirectory();

    // Build new manifest from current BinaryData
    juce::StringArray newManifest;

    for (int i = 0; i < FactoryMidiMappingData::namedResourceListSize; ++i)
    {
        juce::String originalFilename = FactoryMidiMappingData::originalFilenames[i];

        if (!originalFilename.endsWithIgnoreCase(".xml"))
            continue;

        // Parse category from Category__Filename.xml convention
        juce::String category;
        juce::String mappingFilename = originalFilename;

        int separatorIndex = originalFilename.indexOf("__");
        if (separatorIndex >= 0)
        {
            category = originalFilename.substring(0, separatorIndex);
            mappingFilename = originalFilename.substring(separatorIndex + 2);
        }

        // Build relative path for manifest (e.g. "Category/Mapping.xml" or "Mapping.xml")
        juce::String relativePath = category.isNotEmpty()
            ? (category + "/" + mappingFilename)
            : mappingFilename;
        newManifest.add(relativePath);

        // Determine target directory (inside Factory/)
        juce::File targetDir = factoryDir;
        if (category.isNotEmpty())
        {
            targetDir = factoryDir.getChildFile(category);
            if (!targetDir.exists())
                targetDir.createDirectory();
        }

        juce::File targetFile = targetDir.getChildFile(mappingFilename);

        // Only write if file does not exist (don't overwrite user modifications)
        if (!targetFile.exists())
        {
            int dataSize;
            const char* data = FactoryMidiMappingData::getNamedResource(
                FactoryMidiMappingData::namedResourceList[i], dataSize);

            if (data != nullptr && dataSize > 0)
            {
                targetFile.replaceWithData(data, static_cast<size_t>(dataSize));
            }
        }
    }

    // --- Filesystem-based cleanup: delete ANY .xml not in new manifest ---
    // Cleanup runs ONLY inside the Factory/ directory — User/ is NEVER touched.
    // This catches stale factory mappings from ALL previous builds, including
    // pre-manifest ones that persist across iOS app reinstalls.
    for (const auto& file : factoryDir.findChildFiles(juce::File::findFiles, false, "*.xml"))
    {
        if (!newManifest.contains(file.getFileName()))
            file.deleteFile();
    }

    for (const auto& dir : factoryDir.findChildFiles(juce::File::findDirectories, false))
    {
        for (const auto& file : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            juce::String relativePath = dir.getFileName() + "/" + file.getFileName();
            if (!newManifest.contains(relativePath))
                file.deleteFile();
        }
    }

    // Clean up empty subfolders left after stale file deletion
    for (const auto& dir : factoryDir.findChildFiles(juce::File::findDirectories, false))
    {
        if (dir.findChildFiles(juce::File::findFiles, false, "*.xml").isEmpty())
            dir.deleteRecursively();
    }

    // Save new manifest and version marker
    saveFactoryManifest(newManifest);

    auto configDir = StorageManager::getInstance().getConfigDirectory();
    if (!configDir.exists())
        configDir.createDirectory();
    versionFile.replaceWithText(currentVersion);
#endif
}

void MidiMappingManager::scanUserMappings()
{
    userMappings.clear();

    auto& storage = StorageManager::getInstance();

    // Helper lambda: scan a directory for mappings (root + one level of subfolders)
    auto scanDirectory = [this](const juce::File& dir)
    {
        if (!dir.exists())
            return;

        // Scan root-level .xml files
        for (const auto& file : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            MappingInfo info;
            info.name = file.getFileNameWithoutExtension();
            info.file = file;
            info.category = "";  // Root level
            userMappings.push_back(std::move(info));
        }

        // Scan one level of subfolders
        for (const auto& subfolder : dir.findChildFiles(juce::File::findDirectories, false))
        {
            juce::String folderName = subfolder.getFileName();

            for (const auto& file : subfolder.findChildFiles(juce::File::findFiles, false, "*.xml"))
            {
                MappingInfo info;
                info.name = file.getFileNameWithoutExtension();
                info.file = file;
                info.category = folderName;
                userMappings.push_back(std::move(info));
            }
        }
    };

    // Scan Factory directory (installed from BinaryData)
    scanDirectory(storage.getMidiMappingsFactoryDirectory());

    // Scan User directory (user-created mappings)
    scanDirectory(storage.getMidiMappingsUserDirectory());

    // Sort by category first, then by name
    std::sort(userMappings.begin(), userMappings.end(),
              [](const MappingInfo& a, const MappingInfo& b) {
                  if (a.category == b.category)
                      return a.name.compareIgnoreCase(b.name) < 0;
                  return a.category < b.category;
              });
}

std::vector<MidiMappingManager::MappingItem> MidiMappingManager::getAllMappingsWithCategories() const
{
    std::vector<MappingItem> items;
    for (const auto& mapping : userMappings)
    {
        MappingItem item;
        item.name = mapping.name;
        item.category = mapping.category;
        items.push_back(item);
    }
    return items;
}

bool MidiMappingManager::loadMappingFromXml(juce::XmlElement* xml, const juce::String& mappingName)
{
    if (xml == nullptr)
    {
        return false;
    }

    // Verify this is a MIDI mapping file
    if (!xml->hasTagName("MidiMapping"))
    {
        return false;
    }

    // Check version to determine format
    // New: "presetVersion" = "1.0" (standardized attribute name)
    // Legacy: "version" attribute used in older files:
    //   v1.0 = floats, v2.0 = note names, v3.0 = + channel names, v4.0 = + MIDI output device, v5.0 = + custom instrument names
    juce::String version = xml->getStringAttribute("presetVersion", "");
    bool isNewFormat = version.isNotEmpty();
    if (!isNewFormat)
        version = xml->getStringAttribute("version", "1.0");

    // New presetVersion="1.0" files have all features; legacy files use incremental version checks
    bool isVersion2OrLater = isNewFormat || version.startsWith("2.") || version.startsWith("3.") || version.startsWith("4.") || version.startsWith("5.");
    bool isVersion4OrLater = isNewFormat || version.startsWith("4.") || version.startsWith("5.");
    bool isVersion5OrLater = isNewFormat || version.startsWith("5.");

    // Load MIDI output device (v0.3.447) - version 4.0+
    if (isVersion4OrLater)
    {
        currentMidiOutputDevice = xml->getStringAttribute("midiOutputDevice", "Plugin MIDI Output (Default)");
    }
    else
    {
        // Older versions default to plugin MIDI output
        currentMidiOutputDevice = "Plugin MIDI Output (Default)";
    }

    // Load MIDI note parameters (6 total — channel params removed from APVTS in v0.4.171)
    // Uses convertTo0to1() matching PresetManager pattern for reliable parameter restoration
    for (const auto& paramID : MIDI_NOTE_PARAMS)
    {
        auto* paramElement = xml->getChildByName(paramID);
        if (paramElement != nullptr)
        {
            float rawValue = 0.0f;

            if (isVersion2OrLater)
            {
                // Version 2.0+: Parse human-readable note name (e.g., "C4", "A#5")
                juce::String noteName = paramElement->getStringAttribute("value", "C0");
                int midiNote = MidiNoteConversion::nameToMidiNote(noteName);
                if (midiNote < 0) midiNote = 0;
                rawValue = static_cast<float>(midiNote);
            }
            else
            {
                // Version 1.0: Read normalized float directly (backward compatibility)
                float normalized = static_cast<float>(paramElement->getDoubleAttribute("value", 0.0));
                rawValue = normalized * 127.0f;
            }

            if (auto* param = parameters.getParameter(paramID))
            {
                // Use convertTo0to1 matching PresetManager pattern
                float newNorm = param->convertTo0to1(rawValue);
                param->setValueNotifyingHost(newNorm);
            }
        }
    }
    // Note: MIDI channel elements from older XML files (v0.3.403–v0.4.170) are silently ignored.
    // All channels are hardcoded to Ch 10 (GM Drums) since v0.4.119.

    // Load custom instrument names (v0.4.196) - version 5.0+
    if (isVersion5OrLater)
    {
        if (auto* namesElement = xml->getChildByName("InstrumentNames"))
        {
            for (int i = 0; i < 6; ++i)
            {
                juce::String attrName = "ch" + juce::String(i);
                customInstrumentNames[i] = namesElement->getStringAttribute(attrName, DEFAULT_INSTRUMENT_NAMES[i]);
            }
        }
        else
        {
            // New-format file without InstrumentNames element: reset to defaults
            for (int i = 0; i < 6; ++i)
                customInstrumentNames[i] = DEFAULT_INSTRUMENT_NAMES[i];
        }
    }
    else
    {
        // Pre-v5 mappings: reset to defaults
        for (int i = 0; i < 6; ++i)
            customInstrumentNames[i] = DEFAULT_INSTRUMENT_NAMES[i];
    }

    currentMappingName = mappingName;
    currentMappingModified = false;

    notifyInstrumentNameListeners();

    return true;
}

std::unique_ptr<juce::XmlElement> MidiMappingManager::createMappingXml(const juce::String& mappingName)
{
    auto xml = std::make_unique<juce::XmlElement>("MidiMapping");

    // Metadata
    xml->setAttribute("name", mappingName);
    xml->setAttribute("presetVersion", "1.0");
    xml->setAttribute("timestamp", juce::Time::getCurrentTime().toISO8601(true));

    // Save MIDI output device (v0.3.447) - stored as attribute, not parameter
    xml->setAttribute("midiOutputDevice", currentMidiOutputDevice);

    // Save custom instrument names (v0.4.196) - only if any differ from defaults
    {
        bool hasCustomNames = false;
        for (int i = 0; i < 6; ++i)
        {
            if (customInstrumentNames[i] != DEFAULT_INSTRUMENT_NAMES[i])
            {
                hasCustomNames = true;
                break;
            }
        }
        if (hasCustomNames)
        {
            auto* namesElement = xml->createNewChildElement("InstrumentNames");
            for (int i = 0; i < 6; ++i)
                namesElement->setAttribute("ch" + juce::String(i), customInstrumentNames[i]);
        }
    }

    // Save MIDI note parameters (6 total)
    for (const auto& paramID : MIDI_NOTE_PARAMS)
    {
        if (auto* param = parameters.getParameter(paramID))
        {
            auto* paramElement = xml->createNewChildElement(paramID);

            // MIDI Note parameter (0-127) -> human-readable note name
            int midiNote = MidiNoteConversion::normalizedToMidiNote(param->getValue());
            juce::String noteName = MidiNoteConversion::midiNoteToName(midiNote);
            paramElement->setAttribute("value", noteName);
        }
    }

    // Write hardcoded Channel 10 values for backward compatibility with older plugin versions
    // (v0.3.403–v0.4.170 mapping files included channel params)
    for (const auto& channelParamID : MIDI_CHANNEL_PARAMS)
    {
        auto* paramElement = xml->createNewChildElement(channelParamID);
        paramElement->setAttribute("value", "Channel 10 (GM)");
    }

    return xml;
}

juce::File MidiMappingManager::getFactoryManifestFile() const
{
    return StorageManager::getInstance().getConfigDirectory()
               .getChildFile(".factory_midi_mappings_manifest");
}

void MidiMappingManager::saveFactoryManifest(const juce::StringArray& relativePaths) const
{
    auto configDir = StorageManager::getInstance().getConfigDirectory();
    if (!configDir.exists())
        configDir.createDirectory();

    getFactoryManifestFile().replaceWithText(relativePaths.joinIntoString("\n"));
}

juce::File MidiMappingManager::getMappingsBaseDirectory() const
{
    return StorageManager::getInstance().getMidiMappingsDirectory();
}
