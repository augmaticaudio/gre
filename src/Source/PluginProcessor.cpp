#include "PluginProcessor.h"
#include "PluginEditor.h"

// CRITICAL: Channel index to AccentBender instrument index mapping
// Channels: BD=0, SN=1, HH=2, BD'=3, SN'=4, HH'=5
// AccentBender instrument parameters: BD=0, BD'=1, SN=2, SN'=3, HH=4, HH'=5
static constexpr int CHANNEL_TO_INSTRUMENT[6] = {0, 2, 4, 1, 3, 5};

AugmaticGREProcessor::AugmaticGREProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, Identifier("AugmaticGRE"), createParameterLayout())
{
    // Initialize Phase 3 architecture - six GridsEngine instances (3 main + 3 accent)
    for (int ch = 0; ch < 6; ++ch) {
        channels[ch].gridsInstance = std::make_unique<GridsEngine>();
        // Main channels (0,1,2) output their assigned channel, accents (3,4,5) share patterns
        channels[ch].assignedChannelIndex = ch < 3 ? ch : (ch - 3);
    }

    // Initialize clock dividers for tempo modification per channel
    for (int ch = 0; ch < 6; ++ch) {
        clockDividers[ch] = std::make_unique<ClockDivider>();
    }

    // Initialize MIDI shift buffers for per-channel shift and humanization
    for (int ch = 0; ch < 6; ++ch) {
        midiShiftBuffers[ch] = std::make_unique<MidiShiftBuffer>(getSampleRate());
    }

    // Initialize swing controllers for per-channel swing timing (Roger Linn algorithm)
    for (int ch = 0; ch < 6; ++ch) {
        swingControllers[ch] = std::make_unique<SwingController>();
    }

    // Initialize Linear Drumming controller
    linearDrummingController = std::make_unique<LinearDrummingController>();

    // Initialize Accent Bender controller
    accentBenderController = std::make_unique<AccentBenderController>();

    // Initialize Internal Clock Controller (v0.3.448+)
    internalClock = std::make_unique<InternalClockController>();
    syncController.setInternalClock(internalClock.get());

    // CRITICAL: Enable internal clock mode in constructor for standalone
    // This ensures transport works even if prepareToPlay() hasn't been called yet
    // (e.g., when no audio device is selected)
    if (wrapperType == wrapperType_Standalone)
    {
        internalClockSyncValue.store(0.0f);  // Disable DAW sync
        syncController.setInternalClockMode(true);

        // Create virtual MIDI output port for standalone mode
        // Other apps will see "Augmatic GRE" as a MIDI input source
        virtualMidiOutput = MidiOutput::createNewDevice(VIRTUAL_MIDI_PORT_NAME);
        if (virtualMidiOutput != nullptr)
        {
            virtualMidiOutput->startBackgroundThread();
            currentMidiOutputDeviceIndex = 0;  // Virtual port is default
            currentMidiOutputDeviceName = VIRTUAL_MIDI_PORT_NAME;
        }
        else
        {
            currentMidiOutputDeviceIndex = -1;
        }
    }

    // Initialize Euclidean Sequencer Engine
    euclideanEngine = std::make_unique<EuclideanEngine>();

    // Initialize Preset Manager (after parameters are set up)
    presetManager = std::make_unique<PresetManager>(*this, parameters);

    // Initialize MIDI Mapping Manager (independent of presets)
    midiMappingManager = std::make_unique<MidiMappingManager>(parameters);

    setupChannelDefaults();
    setupParameterListeners();

    // Initialize optimization caches
    initializeCachedStrings();
    initializeParameterPointers();

    // Pre-allocate collected notes vector to avoid allocations in audio thread
    // Maximum notes per buffer = 6 channels * 32 steps (worst case for fast sequences)
    collectedNotes.reserve(192);
    pendingNoteOffs.reserve(128);
}

AugmaticGREProcessor::~AugmaticGREProcessor()
{
    // Batch removal of parameter listeners - organized by category for clarity
    // Global parameters
    const std::vector<String> globalParams = {
        "grids_x", "grids_y",
        "density_bd", "density_sd", "density_hh",
        "density_bd_acc", "density_sn_acc", "density_hh_acc",
        "chaos", "note_duration",
        "bd_note", "sn_note", "hh_note",
        "bd_acc_note", "sn_acc_note", "hh_acc_note"
    };

    for (const auto& paramID : globalParams) {
        parameters.removeParameterListener(paramID, this);
    }

    // Per-channel parameters
    const std::vector<String> channelParamSuffixes = {
        "chaos", "vel_value",
        "mute", SHIFT_SUFFIX, "humanize"
    };

    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        String prefix = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;
        for (const auto& suffix : channelParamSuffixes) {
            parameters.removeParameterListener(prefix + suffix, this);
        }
    }

    // Accent Bender parameters
    const std::array<String, 6> accentBenderInstrumentIDs = {
        "accent_bender_instrument_bd", "accent_bender_instrument_bd_acc",
        "accent_bender_instrument_sn", "accent_bender_instrument_sn_acc",
        "accent_bender_instrument_hh", "accent_bender_instrument_hh_acc"
    };
    for (int i = 0; i < 6; ++i) {
        parameters.removeParameterListener(accentBenderInstrumentIDs[i], this);
    }
    const std::array<String, 4> accentBenderSliderIDs = {
        "accent_bender_slider_2n", "accent_bender_slider_4n",
        "accent_bender_slider_4nt", "accent_bender_slider_8n"
    };
    for (int i = 0; i < 4; ++i) {
        parameters.removeParameterListener(accentBenderSliderIDs[i], this);
    }

    // Clean up MIDI output device
    if (midiOutput != nullptr) {
        midiOutput->stopBackgroundThread();
        midiOutput.reset();
    }
}

const String AugmaticGREProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AugmaticGREProcessor::acceptsMidi() const
{
    return true;
}

bool AugmaticGREProcessor::producesMidi() const
{
    return true;
}

bool AugmaticGREProcessor::isMidiEffect() const
{
    return false;
}

double AugmaticGREProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

// ============================================================================
// MIDI Output Device Management (for standalone version)
// ============================================================================

StringArray AugmaticGREProcessor::getAvailableMidiOutputDevices() const
{
    StringArray devices;

    // In standalone mode, the virtual port is the default option
    // In AUv3 mode, plugin MIDI output is the default
    if (wrapperType == wrapperType_Standalone)
    {
        devices.add(String(VIRTUAL_MIDI_PORT_NAME) + " (Virtual)");  // Index 0 = virtual MIDI port
    }
    else
    {
        devices.add("Plugin MIDI Output");  // Index 0 = use plugin's MIDI output (AUv3)
    }

    // Get available hardware MIDI devices
    auto midiDevices = MidiOutput::getAvailableDevices();
    for (int i = 0; i < midiDevices.size(); ++i) {
        // Skip our own virtual port in the hardware list (it would appear there too)
        if (midiDevices[i].name != VIRTUAL_MIDI_PORT_NAME)
            devices.add(midiDevices[i].name);
    }

    return devices;
}

void AugmaticGREProcessor::setMidiOutputDevice(int deviceIndex)
{
    // Close existing hardware MIDI output if open (but keep virtual port alive)
    if (midiOutput != nullptr) {
        midiOutput->stopBackgroundThread();
        midiOutput.reset();
    }

    // Index 0 = default output (virtual port in standalone, plugin output in AUv3)
    if (deviceIndex == 0) {
        if (wrapperType == wrapperType_Standalone && virtualMidiOutput != nullptr) {
            currentMidiOutputDeviceIndex = 0;
            currentMidiOutputDeviceName = String(VIRTUAL_MIDI_PORT_NAME) + " (Virtual)";
        } else {
            currentMidiOutputDeviceIndex = -1;
            currentMidiOutputDeviceName = "Plugin MIDI Output";
        }
        return;
    }

    // Adjust for the default option at index 0
    int hardwareDeviceIndex = deviceIndex - 1;

    // Get available devices, filtering out our virtual port
    auto midiDevices = MidiOutput::getAvailableDevices();
    int actualIndex = 0;
    for (int i = 0; i < midiDevices.size(); ++i) {
        if (midiDevices[i].name == VIRTUAL_MIDI_PORT_NAME)
            continue;  // Skip our own virtual port

        if (actualIndex == hardwareDeviceIndex) {
            // Open the selected MIDI device
            midiOutput = MidiOutput::openDevice(midiDevices[i].identifier);

            if (midiOutput != nullptr) {
                midiOutput->startBackgroundThread();
                currentMidiOutputDeviceIndex = deviceIndex;
                currentMidiOutputDeviceName = midiDevices[i].name;
            } else {
                // Failed to open device - revert to default
                setMidiOutputDevice(0);
            }
            return;
        }
        actualIndex++;
    }

    // Device not found - revert to default
    setMidiOutputDevice(0);
}

String AugmaticGREProcessor::getCurrentMidiOutputDevice() const
{
    if (currentMidiOutputDeviceIndex == -1) {
        return "Plugin MIDI Output";
    }
    if (currentMidiOutputDeviceIndex == 0 && wrapperType == wrapperType_Standalone) {
        return String(VIRTUAL_MIDI_PORT_NAME) + " (Virtual)";
    }
    return currentMidiOutputDeviceName;
}

void AugmaticGREProcessor::setMidiOutputChannel(int channel)
{
    // Clamp to valid range (1-16) and convert to 0-indexed
    int clampedChannel = juce::jlimit(1, 16, channel);
    int zeroIndexedChannel = clampedChannel - 1;

    // Set all 6 channels to use the same MIDI output channel
    for (int ch = 0; ch < 6; ++ch)
    {
        channels[ch].midiChannel = zeroIndexedChannel;
    }
}

int AugmaticGREProcessor::getMidiOutputChannel() const
{
    // Return 1-indexed channel number (all channels use the same, so just return first)
    return channels[0].midiChannel + 1;
}

int AugmaticGREProcessor::getNumPrograms()
{
    return 1;
}

int AugmaticGREProcessor::getCurrentProgram()
{
    return 0;
}

void AugmaticGREProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const String AugmaticGREProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AugmaticGREProcessor::changeProgramName(int index, const String& newName)
{
    juce::ignoreUnused(index, newName);
}

void AugmaticGREProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    samplesPerBlock = maximumExpectedSamplesPerBlock;
    
    // Prepare Phase 3 architecture
    for (auto& channel : channels) {
        channel.prepare(sampleRate, maximumExpectedSamplesPerBlock);
    }

    // Prepare MIDI shift buffers
    for (auto& shiftBuffer : midiShiftBuffers) {
        if (shiftBuffer) {
            shiftBuffer->setSampleRate(sampleRate);
        }
    }

    syncController.setSampleRate(sampleRate);

    // Initialize Internal Clock Controller (v0.3.448+)
    if (internalClock) {
        internalClock->setSampleRate(sampleRate);

        // Get initial BPM from internal state
        internalClock->setBPM(internalClockBpmValue.load());
    }

    // Auto-detect standalone mode and enable internal clock by default
    if (wrapperType == wrapperType_Standalone) {
        internalClockSyncValue.store(0.0f);  // Disable DAW sync (enable internal clock)
        syncController.setInternalClockMode(true);
    }

    // Initialize Linear Drumming
    if (linearDrummingController) {
        linearDrummingController->initialize(sampleRate);
    }

    // Initialize Euclidean Sequencer Engine
    if (euclideanEngine) {
        euclideanEngine->setSampleRate(sampleRate);
        euclideanEngine->initialize();
    }

    // Initialize all parameters with current values from the parameter tree
    updateParametersFromUI();
}

void AugmaticGREProcessor::releaseResources()
{
}

bool AugmaticGREProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Support stereo output for silent audio instrument architecture
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void AugmaticGREProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    // Increment processBlock counter for diagnostic purposes
    processBlockCounter.fetch_add(1);
    audioProcessingActive.store(true);

    // Clear audio buffer for silent audio instrument
    buffer.clear();

    // Clear input MIDI
    midiMessages.clear();

    // Process manual notes from UI (clickable LEDs/pads)
    {
        const juce::SpinLock::ScopedLockType lock(manualNoteLock);
        for (const auto& note : pendingManualNotes) {
            if (note.channelIndex >= 0 && note.channelIndex < 6) {
                int midiChannel = channels[note.channelIndex].midiChannel + 1;
                int midiNote = channels[note.channelIndex].midiNote;

                // Send NOTE ON at start of buffer
                auto noteOnMsg = MidiMessage::noteOn(midiChannel, midiNote, note.velocity);
                midiMessages.addEvent(noteOnMsg, 0);

                // Schedule NOTE OFF using the same duration system as regular notes
                double noteDurationSamples = calculateNoteDurationSamples();
                scheduleNoteOff(note.channelIndex, midiChannel, midiNote, noteDurationSamples, 0, 0);

                // Trigger LED feedback using channel index directly
                if (ledNotifyCallback) {
                    auto callback = ledNotifyCallback;
                    auto mn = midiNote;
                    auto vel = note.velocity;
                    auto chIdx = note.channelIndex;
                    int step = (chIdx >= 0 && chIdx < 6 && channels[chIdx].gridsInstance)
                               ? static_cast<int>(channels[chIdx].gridsInstance->getCurrentStep()) : 0;
                    ClockInfo ci = syncController.getClockForChannel(0);
                    double ppq = ci.ppqPosition;
                    double bpmVal = ci.bpm;
                    juce::MessageManager::callAsync([callback, mn, vel, chIdx, step, ppq, bpmVal]() {
                        if (callback) callback(mn, vel, chIdx, step, ppq, bpmVal);
                    });
                }
            }
        }
        pendingManualNotes.clear();
    }

    // Update parameters if changed from UI thread
    // Use exchange(false) to atomically clear the flag — any parameterChanged() calls
    // during updateParametersFromUI() (e.g. from master chaos override) will re-set the
    // flag, ensuring a follow-up update on the next processBlock.
    if (needsParameterUpdate.exchange(false)) {
        updateParametersFromUI();
    }
    
    // Update master synchronization
    syncController.updateFromPlayHead(getPlayHead(), buffer.getNumSamples());

    // Update Accent Bender position
    if (accentBenderController) {
        // Get timing info from any channel (they all share the same master timing)
        ClockInfo clockInfo = syncController.getClockForChannel(0);
        double ppqPosition = clockInfo.ppqPosition;
        double bpm = clockInfo.bpm;
        accentBenderController->updatePosition(ppqPosition, bpm, buffer.getNumSamples(), getSampleRate());
    }

    // Clear shift buffers when transport stops
    if (syncController.hasTransportStopped()) {
        for (auto& shiftBuffer : midiShiftBuffers) {
            if (shiftBuffer) {
                shiftBuffer->clear();
            }
        }
    }

    // Check if any channel needs reset and reset Note Duration module accordingly
    bool anyChannelNeedsReset = false;
    for (int ch = 0; ch < 6; ++ch) {
        if (syncController.shouldResetChannel(ch)) {
            anyChannelNeedsReset = true;
            break;
        }
    }

    if (anyChannelNeedsReset) {
        // Reset Note Duration module when transport starts to ensure proper synchronization
        resetNoteDurationModule();

        // Reset all shift buffers when transport starts
        for (auto& shiftBuffer : midiShiftBuffers) {
            if (shiftBuffer) {
                shiftBuffer->reset();
            }
        }

        // Notify editor that transport started (for animation line randomization)
        if (onTransportStart) {
            juce::MessageManager::callAsync([callback = onTransportStart]() {
                if (callback) callback();
            });
        }
    }

    // Process each channel independently (Phase 3 architecture)
    // Each GridsEngine manages its own timing and step counter (6 channels: 3 main + 3 accent)
    for (int ch = 0; ch < 6; ++ch) {
        if (channels[ch].enabled) {
            processChannelBlock(channels[ch], ch, buffer, midiMessages);
        }
    }

    // NEW: Process Linear Drumming after all channels are processed
    processLinearDrumming(buffer, midiMessages);

    // Clear collected notes for next buffer
    collectedNotes.clear();

    // NOTE DURATION MODULE: Process pending NOTE OFF events based on DAW transport timing
    // This runs after all channels to handle NOTE OFF events scheduled by previous NOTE ON events
    processNoteDurationModule(midiMessages, buffer.getNumSamples());

    // Update global sample counter for Note Duration module timing
    globalSampleCounter += buffer.getNumSamples();

    // MIDI OUTPUT DEVICE ROUTING (for standalone version)
    // Route MIDI to: virtual port (default in standalone), hardware device, or plugin output
    if (!midiMessages.isEmpty()) {
        MidiOutput* outputDevice = nullptr;

        // Determine which output device to use
        if (currentMidiOutputDeviceIndex == 0 && virtualMidiOutput != nullptr) {
            // Virtual port selected (standalone default)
            outputDevice = virtualMidiOutput.get();
        } else if (midiOutput != nullptr) {
            // Hardware device selected
            outputDevice = midiOutput.get();
        }
        // If outputDevice is null, MIDI goes through the plugin's normal MIDI output (AUv3)

        if (outputDevice != nullptr) {
            for (const auto metadata : midiMessages) {
                outputDevice->sendMessageNow(metadata.getMessage());
            }
        }
    }

}

bool AugmaticGREProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* AugmaticGREProcessor::createEditor()
{
    
    auto* editor = new AugmaticGREEditor(*this);
    return editor;
}

void AugmaticGREProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = parameters.copyState();

    // Add version information for migration and compatibility checking
    state.setProperty("version", JucePlugin_VersionString, nullptr);
    state.setProperty("minCompatibleVersion", "0.3.0", nullptr);

    // MIDI channel parameters removed from APVTS in v0.4.171 (hardcoded Ch 10 since v0.4.119)
    // No need to strip them from state — they're no longer in APVTS

    // Save internal parameters (not in APVTS) manually
    state.setProperty("internal_clock_sync", internalClockSyncValue.load(), nullptr);
    state.setProperty("internal_clock_bpm", internalClockBpmValue.load(), nullptr);

    // Save MIDI output channel and device (standalone mode settings)
    state.setProperty("midi_output_channel", getMidiOutputChannel(), nullptr);
    state.setProperty("midi_output_device_index", currentMidiOutputDeviceIndex, nullptr);

    std::unique_ptr<XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AugmaticGREProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
    {
        // Verify tag name matches to prevent loading wrong data
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            auto newState = ValueTree::fromXml(*xmlState);

            // Load internal parameters (not in APVTS) before replacing state
            if (newState.hasProperty("internal_clock_sync"))
                internalClockSyncValue.store((float)newState.getProperty("internal_clock_sync"));
            if (newState.hasProperty("internal_clock_bpm"))
                internalClockBpmValue.store((float)newState.getProperty("internal_clock_bpm"));

            // Apply internal clock values to controllers
            // CRITICAL: In standalone mode, ALWAYS use internal clock regardless of saved state
            if (wrapperType == wrapperType_Standalone)
            {
                internalClockSyncValue.store(0.0f);  // Force internal clock mode
                syncController.setInternalClockMode(true);
            }
            else
            {
                bool syncEnabled = internalClockSyncValue.load() > 0.5f;
                syncController.setInternalClockMode(!syncEnabled);
            }
            if (internalClock)
                internalClock->setBPM(internalClockBpmValue.load());

            // Load MIDI output channel and device (standalone mode settings)
            if (newState.hasProperty("midi_output_channel"))
                setMidiOutputChannel((int)newState.getProperty("midi_output_channel"));
            if (newState.hasProperty("midi_output_device_index"))
                setMidiOutputDevice((int)newState.getProperty("midi_output_device_index"));

            parameters.replaceState(newState);
        }
    }
}

// NonAutomatableParam template removed in v0.4.171 — all non-automatable params
// (MIDI channels, accent_bender_all, clock) moved to internal member variables

AudioProcessorValueTreeState::ParameterLayout AugmaticGREProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // DAW display name prefixes for each channel (uses prime notation for accent channels)
    static const char* DAW_PREFIX[] = {"BD", "SN", "HH", "BD'", "SN'", "HH'"};

    // Helper lambda to build per-channel prefix
    auto pfx = [](size_t ch) { return String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR; };
    auto dp  = [&](size_t ch) { return String(DAW_PREFIX[ch]); };

    // =========================================================================
    // 1. PATTERN
    // =========================================================================

    // --- 1.1 DENSITY ---
    layout.add(std::make_unique<AudioParameterInt>("grids_x", "Grids X", 0, 255, 45));
    layout.add(std::make_unique<AudioParameterInt>("grids_y", "Grids Y", 0, 255, 220));
    layout.add(std::make_unique<AudioParameterInt>("chaos", "Master Chaos", 0, 127, 0));
    layout.add(std::make_unique<AudioParameterInt>("density_bd", "BD Density", 0, 255, 210));
    layout.add(std::make_unique<AudioParameterInt>("density_sd", "SD Density", 0, 255, 60));
    layout.add(std::make_unique<AudioParameterInt>("density_hh", "HH Density", 0, 255, 255));
    layout.add(std::make_unique<AudioParameterInt>("density_bd_acc", "BD' Density", 0, 255, 0));
    layout.add(std::make_unique<AudioParameterInt>("density_sn_acc", "SN' Density", 0, 255, 0));
    layout.add(std::make_unique<AudioParameterInt>("density_hh_acc", "HH' Density", 0, 255, 0));

    // --- 1.2 CHAOS (per-channel) ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterInt>(
            pfx(ch) + CHAOS_SUFFIX, dp(ch) + " Chaos", 0, 127, 0));

    // --- 1.3 BLEND (Grids↔Euclidean) ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterFloat>(
            pfx(ch) + ENGINE_PROBABILITY_SUFFIX, dp(ch) + " Blend", 0.0f, 1.0f, 0.0f));

    // --- 1.4 STEPS + PULSES ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterInt>(
            pfx(ch) + EUCLIDEAN_STEPS_SUFFIX, dp(ch) + " Steps",
            EUCLIDEAN_MIN_STEPS, EUCLIDEAN_MAX_STEPS, EUCLIDEAN_DEFAULT_STEPS));
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterInt>(
            pfx(ch) + EUCLIDEAN_PULSES_SUFFIX, dp(ch) + " Pulses",
            0, EUCLIDEAN_MAX_STEPS, 4));

    // --- 1.5 START ON ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterInt>(
            pfx(ch) + EUCLIDEAN_START_SUFFIX, dp(ch) + " Start On",
            1, EUCLIDEAN_MAX_STEPS, EUCLIDEAN_DEFAULT_START));

    // --- 1.6 SWING ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterFloat>(
            pfx(ch) + SWING_SUFFIX, dp(ch) + " Swing",
            juce::NormalisableRange<float>(-99.0f, 99.0f, 1.0f), 0.0f));

    // --- 1.7 SHIFT ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterInt>(
            pfx(ch) + SHIFT_SUFFIX, dp(ch) + " Shift", 0, 126, 63));

    // --- 1.8 HUMANIZE ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterInt>(
            pfx(ch) + HUMANIZE_SUFFIX, dp(ch) + " Humanize", 0, 127, 0));

    // --- 1.9 CLOCK ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterChoice>(
            pfx(ch) + CLOCK_RATIO_SUFFIX, dp(ch) + " Clock Ratio",
            StringArray{"/8", "/7", "/6", "/5", "/4", "/3", "/2", "/1.5",
                       "x1", "x1.5", "x2", "x2.5", "x3", "x4", "x5", "x6", "x7", "x8"},
            8));  // Default to x1 (index 8)

    // =========================================================================
    // 2. LINEAR
    // =========================================================================

    // --- 2.1 PROBABILITY PRE ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterFloat>(
            pfx(ch) + PROBABILITY_PRE_SUFFIX, dp(ch) + " Probability Pre",
            NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    // --- 2.2 MUTE PRE ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterBool>(
            pfx(ch) + MUTE_PRE_SUFFIX, dp(ch) + " Mute Pre", false));

    // --- 2.3 SOLO PRE ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterBool>(
            pfx(ch) + SOLO_PRE_SUFFIX, dp(ch) + " Solo Pre", false));

    // --- 2.4 LINEAR PRIORITY ---
    const std::array<String, 6> linearPriorityIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };
    for (int i = 0; i < 6; ++i)
        layout.add(std::make_unique<AudioParameterChoice>(
            linearPriorityIDs[i], String(DAW_PREFIX[i]) + " Linear Drumming",
            juce::StringArray{"Priority 1", "Priority 2", "Priority 3", "Priority 4", "Priority 5", "Priority 6"},
            0));

    // --- 2.5 PROBABILITY POST ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterFloat>(
            pfx(ch) + PROBABILITY_POST_SUFFIX, dp(ch) + " Probability Post",
            NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    // --- 2.6 MUTE POST ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterBool>(
            pfx(ch) + MUTE_POST_SUFFIX, dp(ch) + " Mute Post", false));

    // --- 2.7 SOLO POST ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterBool>(
            pfx(ch) + SOLO_POST_SUFFIX, dp(ch) + " Solo Post", false));

    // =========================================================================
    // 3. VELOCITY
    // =========================================================================

    // --- 3.1 LEVEL ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterInt>(
            pfx(ch) + VEL_VALUE_SUFFIX, dp(ch) + " Velocity", 1, 127, 120));

    // --- 3.2 RANDOM ---
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch)
        layout.add(std::make_unique<AudioParameterFloat>(
            pfx(ch) + VEL_RANDOMIZE_SUFFIX, dp(ch) + " Velocity Random", 0.0f, 100.0f, 0.0f));

    // --- 3.3 BENDER (instrument toggles) ---
    const std::array<String, 6> accentBenderInstrumentIDs = {
        "accent_bender_instrument_bd", "accent_bender_instrument_sn", "accent_bender_instrument_hh",
        "accent_bender_instrument_bd_acc", "accent_bender_instrument_sn_acc", "accent_bender_instrument_hh_acc"
    };
    for (int i = 0; i < 6; ++i)
        layout.add(std::make_unique<AudioParameterBool>(
            accentBenderInstrumentIDs[i], String(DAW_PREFIX[i]) + " Bender", true));

    // --- 3.4–3.7 BENDER SLIDERS (2n, 4n, 4nt, 8n) ---
    const std::array<std::pair<String, String>, 4> accentBenderSliders = {{
        {"accent_bender_slider_2n",  "Bender 2n"},
        {"accent_bender_slider_4n",  "Bender 4n"},
        {"accent_bender_slider_4nt", "Bender 4nt"},
        {"accent_bender_slider_8n",  "Bender 8n"}
    }};
    for (const auto& [id, name] : accentBenderSliders)
        layout.add(std::make_unique<AudioParameterFloat>(
            id, name, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    // =========================================================================
    // 4. SETTINGS
    // =========================================================================

    // --- 4.1 NOTE ---
    layout.add(std::make_unique<AudioParameterInt>("bd_note", "BD MIDI Note", 0, 127, 36));      // C2
    layout.add(std::make_unique<AudioParameterInt>("sn_note", "SN MIDI Note", 0, 127, 38));      // D2
    layout.add(std::make_unique<AudioParameterInt>("hh_note", "HH MIDI Note", 0, 127, 42));      // F#2
    layout.add(std::make_unique<AudioParameterInt>("bd_acc_note", "BD' MIDI Note", 0, 127, 44));
    layout.add(std::make_unique<AudioParameterInt>("sn_acc_note", "SN' MIDI Note", 0, 127, 39));
    layout.add(std::make_unique<AudioParameterInt>("hh_acc_note", "HH' MIDI Note", 0, 127, 48));

    // --- 4.3 NOTE LENGTH ---
    layout.add(std::make_unique<AudioParameterChoice>(
        "note_duration", "Note Length",
        juce::StringArray{"4n", "8n", "16n", "32n", "64n"}, 2));  // Default 16n

    // --- 4.4 LINEAR GRID ---
    layout.add(std::make_unique<AudioParameterChoice>(
        LINEAR_DRUMMING_GRID_SUFFIX, "Linear Grid",
        juce::StringArray{"64n", "32n", "16n", "8n", "4n"}, 2));  // Default 16n

    // MIDI Channel parameters removed from APVTS in v0.4.171 — hardcoded to Ch 10 (GM Drums)
    // since v0.4.119. All channels use channels[ch].midiChannel set in setupChannelDefaults().
    //
    // accent_bender_all removed from APVTS in v0.4.170 — kept as internal member variable.
    // internal_clock_sync and internal_clock_bpm removed from APVTS in v0.4.170 —
    // managed as internal member variables, saved/loaded in getStateInformation/setStateInformation.

    return layout;
}

// Phase 3 Implementation Methods

void AugmaticGREProcessor::setupChannelDefaults() {
    // BD (Bass Drum) - Channel 0
    channels[0].midiNote = 36; // C2 - GM standard
    channels[0].midiChannel = 9; // GM Drum channel (0-indexed)

    // SN (Snare) - Channel 1
    channels[1].midiNote = 38; // D2 - GM standard
    channels[1].midiChannel = 9;

    // HH (Hi-Hat) - Channel 2
    channels[2].midiNote = 42; // F#2 - GM standard
    channels[2].midiChannel = 9;

    // BD Accent - Channel 3
    channels[3].midiNote = 44; // BD accent default
    channels[3].midiChannel = 9;
    channels[3].assignedChannelIndex = 0; // Share BD pattern with different accent processing

    // SN Accent - Channel 4
    channels[4].midiNote = 39; // SN accent default
    channels[4].midiChannel = 9;
    channels[4].assignedChannelIndex = 1; // Share SN pattern with different accent processing

    // HH Accent - Channel 5
    channels[5].midiNote = 48; // HH accent default
    channels[5].midiChannel = 9;
    channels[5].assignedChannelIndex = 2; // Share HH pattern with different accent processing
}

void AugmaticGREProcessor::setupParameterListeners() {
    // Set up listeners for ALL parameters to trigger updates when GUI changes values
    
    // Core Grids parameters
    parameters.addParameterListener("grids_x", this);
    parameters.addParameterListener("grids_y", this);
    parameters.addParameterListener("density_bd", this);
    parameters.addParameterListener("density_sd", this);
    parameters.addParameterListener("density_hh", this);
    parameters.addParameterListener("density_bd_acc", this);
    parameters.addParameterListener("density_sn_acc", this);
    parameters.addParameterListener("density_hh_acc", this);
    parameters.addParameterListener("chaos", this);
    parameters.addParameterListener("note_duration", this);

    // MIDI note assignments (main + accent)
    parameters.addParameterListener("bd_note", this);
    parameters.addParameterListener("sn_note", this);
    parameters.addParameterListener("hh_note", this);
    parameters.addParameterListener("bd_acc_note", this);
    parameters.addParameterListener("sn_acc_note", this);
    parameters.addParameterListener("hh_acc_note", this);

    // MIDI channel parameters: Listeners removed (v0.4.119) - all channels hardcoded to Ch 10 for AUv3
    // Parameters kept in APVTS for preset backward compatibility

    // Phase 3 parameters for each channel (main + accent)
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        String prefix = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;

        parameters.addParameterListener(prefix + CHAOS_SUFFIX, this);
        parameters.addParameterListener(prefix + VEL_VALUE_SUFFIX, this);
        parameters.addParameterListener(prefix + MUTE_PRE_SUFFIX, this);
        parameters.addParameterListener(prefix + MUTE_POST_SUFFIX, this);
        parameters.addParameterListener(prefix + SOLO_PRE_SUFFIX, this);
        parameters.addParameterListener(prefix + SOLO_POST_SUFFIX, this);
        parameters.addParameterListener(prefix + SHIFT_SUFFIX, this);
        parameters.addParameterListener(prefix + HUMANIZE_SUFFIX, this);

        // Euclidean engine parameters
        parameters.addParameterListener(prefix + EUCLIDEAN_STEPS_SUFFIX, this);
        parameters.addParameterListener(prefix + EUCLIDEAN_PULSES_SUFFIX, this);
        parameters.addParameterListener(prefix + EUCLIDEAN_START_SUFFIX, this);
        parameters.addParameterListener(prefix + ENGINE_PROBABILITY_SUFFIX, this);
    }

    // Linear Drumming parameter listeners
    parameters.addParameterListener(LINEAR_DRUMMING_GRID_SUFFIX, this);
    const std::array<String, 6> linearPriorityIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };
    for (int i = 0; i < 6; ++i) {
        parameters.addParameterListener(linearPriorityIDs[i], this);
    }

    // Accent Bender parameter listeners
    const std::array<String, 6> accentBenderInstrumentIDs = {
        "accent_bender_instrument_bd", "accent_bender_instrument_bd_acc",
        "accent_bender_instrument_sn", "accent_bender_instrument_sn_acc",
        "accent_bender_instrument_hh", "accent_bender_instrument_hh_acc"
    };
    for (int i = 0; i < 6; ++i) {
        parameters.addParameterListener(accentBenderInstrumentIDs[i], this);
    }
    const std::array<String, 4> accentBenderSliderIDs = {
        "accent_bender_slider_2n", "accent_bender_slider_4n",
        "accent_bender_slider_4nt", "accent_bender_slider_8n"
    };
    for (int i = 0; i < 4; ++i) {
        parameters.addParameterListener(accentBenderSliderIDs[i], this);
    }

    // NOTE: Internal Clock params removed from APVTS (v0.4.170) - no listeners needed.
    // Clock sync/BPM are now managed via internal member variables and direct setter calls.

}

void AugmaticGREProcessor::parameterChanged(const String& parameterID, float newValue) {
    // Called whenever any parameter changes in the GUI
    // Set flag to update parameters in next processBlock call (thread-safe)
    needsParameterUpdate.store(true);

    // MASTER CHAOS OVERRIDE: When master "chaos" changes, propagate to all per-channel
    // chaos parameters. Done here (not in processBlock) so per-channel SliderAttachments
    // update on the same thread as the UI change, ensuring immediate knob updates.
    if (parameterID == "chaos") {
        int mainChaos = static_cast<int>(newValue);
        int prev = lastMainChaos.load();
        if (mainChaos != prev) {
            lastMainChaos.store(mainChaos);
            for (size_t ch = 0; ch < NUM_CHANNELS; ++ch) {
                String paramName = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR + CHAOS_SUFFIX;
                if (auto* p = parameters.getParameter(paramName)) {
                    p->setValueNotifyingHost(mainChaos / 127.0f);
                }
            }
        }
    }

    // PERFORMANCE OPTIMIZATION: Flag Linear Drumming parameter updates
    if (parameterID == LINEAR_DRUMMING_GRID_SUFFIX ||
        parameterID.startsWith(LINEAR_DRUMMING_PRIORITY_SUFFIX)) {
        linearCache.needsUpdate = true;
    }

    // NOTE: Internal Clock params handled via setInternalClockSync/setBPM, not via APVTS listener
}

void AugmaticGREProcessor::updateParametersFromUI() {
    // NOTE: Master Chaos override is now handled in parameterChanged() (not here).
    // This ensures per-channel SliderAttachments update on the UI thread.

    // Sync internal clock BPM on audio thread (deferred from UI thread via setInternalClockBPM)
    if (internalClock) internalClock->setBPM(internalClockBpmValue.load());

    // Update each channel's parameters from the parameter tree (main + accent)
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        // Optimization: Use cached parameter pointers instead of string lookups
        float chaos = channelParams[ch].chaos->load();
        int velValue = static_cast<int>(channelParams[ch].velValue->load());
        float velRandomize = channelParams[ch].velRandomize->load();

        // Apply to channel processors
        channels[ch].chaosController.setChannelChaos(ch, chaos);
        // Note: Clock division now handled directly in processChannelBlock (v161 approach)
        channels[ch].velocityController.setChannelVelocityLevel(ch, static_cast<uint8_t>(velValue));
        channels[ch].velocityController.setChannelVelocityRandomization(ch, velRandomize);
    }
    
    // CORRECTED: Each GridsEngine instance gets ONLY its own channel's density
    // This makes each instance truly independent and generates different patterns
    auto* gridsXParam = parameters.getRawParameterValue("grids_x");
    auto* gridsYParam = parameters.getRawParameterValue("grids_y");
    int gridsX = gridsXParam ? static_cast<int>(gridsXParam->load()) : 128;
    int gridsY = gridsYParam ? static_cast<int>(gridsYParam->load()) : 128;

    // Main channel densities with null checks
    auto* bdDensParam = parameters.getRawParameterValue("density_bd");
    auto* sdDensParam = parameters.getRawParameterValue("density_sd");
    auto* hhDensParam = parameters.getRawParameterValue("density_hh");
    int densities[3] = {
        bdDensParam ? static_cast<int>(bdDensParam->load()) : 0,
        sdDensParam ? static_cast<int>(sdDensParam->load()) : 0,
        hhDensParam ? static_cast<int>(hhDensParam->load()) : 0
    };

    // Accent channel densities with null checks
    auto* bdAccDensParam = parameters.getRawParameterValue("density_bd_acc");
    auto* snAccDensParam = parameters.getRawParameterValue("density_sn_acc");
    auto* hhAccDensParam = parameters.getRawParameterValue("density_hh_acc");
    int accentDensities[3] = {
        bdAccDensParam ? static_cast<int>(bdAccDensParam->load()) : 0,
        snAccDensParam ? static_cast<int>(snAccDensParam->load()) : 0,
        hhAccDensParam ? static_cast<int>(hhAccDensParam->load()) : 0
    };

    // Configure main channels (0,1,2)
    for (int ch = 0; ch < 3; ++ch) {
        if (channels[ch].gridsInstance) {
            // All instances get same X/Y coordinates (pattern morphing)
            channels[ch].gridsInstance->setXCoordinate(static_cast<uint8_t>(gridsX));
            channels[ch].gridsInstance->setYCoordinate(static_cast<uint8_t>(gridsY));

            // CRITICAL FIX: Each instance gets ONLY its own density
            // Set all densities to 0, then only set the one for this channel
            channels[ch].gridsInstance->setDensity(0, 0);  // BD off by default
            channels[ch].gridsInstance->setDensity(1, 0);  // SN off by default
            channels[ch].gridsInstance->setDensity(2, 0);  // HH off by default

            // Only activate THIS channel's density
            channels[ch].gridsInstance->setDensity(ch, static_cast<uint8_t>(densities[ch]));

            // CHAOS FIX: Each instance gets its OWN per-channel chaos, NOT global chaos!
            String prefix = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;
            auto* perChannelChaosParam = parameters.getRawParameterValue(prefix + CHAOS_SUFFIX);
            int perChannelChaos = perChannelChaosParam ? static_cast<int>(perChannelChaosParam->load()) : 0;
            int scaledChaos = std::clamp(perChannelChaos * 2, 0, 254);  // 0-127 -> 0-254
            channels[ch].gridsInstance->setChaos(static_cast<uint8_t>(scaledChaos));
        }
    }

    // Configure accent channels (3,4,5) - share patterns with main channels but use accent densities
    for (int ch = 3; ch < 6; ++ch) {
        if (channels[ch].gridsInstance) {
            // All instances get same X/Y coordinates (pattern morphing)
            channels[ch].gridsInstance->setXCoordinate(static_cast<uint8_t>(gridsX));
            channels[ch].gridsInstance->setYCoordinate(static_cast<uint8_t>(gridsY));

            // Set all densities to 0, then only set the one for the corresponding main channel
            channels[ch].gridsInstance->setDensity(0, 0);  // BD off by default
            channels[ch].gridsInstance->setDensity(1, 0);  // SN off by default
            channels[ch].gridsInstance->setDensity(2, 0);  // HH off by default

            // Accent channels share pattern with main channels: 3->0, 4->1, 5->2
            int mainChannelIndex = ch - 3;
            channels[ch].gridsInstance->setDensity(mainChannelIndex, static_cast<uint8_t>(accentDensities[mainChannelIndex]));

            // Each accent channel gets its own chaos
            String prefix = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;
            auto* perChannelChaosParam = parameters.getRawParameterValue(prefix + CHAOS_SUFFIX);
            int perChannelChaos = perChannelChaosParam ? static_cast<int>(perChannelChaosParam->load()) : 0;
            int scaledChaos = std::clamp(perChannelChaos * 2, 0, 254);  // 0-127 -> 0-254
            channels[ch].gridsInstance->setChaos(static_cast<uint8_t>(scaledChaos));
        }
    }
    
    // MIDI NOTE ASSIGNMENTS: Update from MIDI tab parameters with note-off handling

    // Main channel MIDI notes
    const char* noteParams[] = {"bd_note", "sn_note", "hh_note"};
    for (int ch = 0; ch < 3; ++ch) {
        auto* noteParam = parameters.getRawParameterValue(noteParams[ch]);
        int newMidiNote = noteParam ? static_cast<int>(noteParam->load()) : channels[ch].midiNote;

        // Check if MIDI note assignment changed
        if (newMidiNote != channels[ch].midiNote) {
            // Flag pending note-off for the old MIDI note (thread-safe)
            // This will be processed during audio processing to prevent hanging notes
            channels[ch].pendingNoteOffMidiNote.store(channels[ch].midiNote);
            channels[ch].pendingNoteOffMidiChannel.store(channels[ch].midiChannel + 1);
            channels[ch].hasPendingNoteOff.store(true);


            // Update to new MIDI note assignment
            channels[ch].midiNote = newMidiNote;
        }
    }

    // Accent channel MIDI notes
    const char* accentNoteParams[] = {"bd_acc_note", "sn_acc_note", "hh_acc_note"};
    for (int ch = 3; ch < 6; ++ch) {
        int accentIndex = ch - 3;
        auto* accentNoteParam = parameters.getRawParameterValue(accentNoteParams[accentIndex]);
        int newMidiNote = accentNoteParam ? static_cast<int>(accentNoteParam->load()) : channels[ch].midiNote;

        // Check if MIDI note assignment changed
        if (newMidiNote != channels[ch].midiNote) {
            // Flag pending note-off for the old MIDI note (thread-safe)
            // This will be processed during audio processing to prevent hanging notes
            channels[ch].pendingNoteOffMidiNote.store(channels[ch].midiNote);
            channels[ch].pendingNoteOffMidiChannel.store(channels[ch].midiChannel + 1);
            channels[ch].hasPendingNoteOff.store(true);


            // Update to new MIDI note assignment
            channels[ch].midiNote = newMidiNote;
        }
    }

    // MIDI CHANNEL: All instruments hardcoded to Channel 10 (GM Drums) for AUv3 (v0.4.119)
    // MIDI channel parameters kept in APVTS for preset backward compatibility but ignored here.
    // channels[ch].midiChannel is set to 9 (Channel 10, 0-indexed) in setupChannelDefaults().

    // Update Accent Bender parameters
    if (accentBenderController) {
        // Update per-instrument enable states from individual checkboxes
        const std::array<String, 6> accentBenderInstrumentIDs = {
            "accent_bender_instrument_bd", "accent_bender_instrument_bd_acc",
            "accent_bender_instrument_sn", "accent_bender_instrument_sn_acc",
            "accent_bender_instrument_hh", "accent_bender_instrument_hh_acc"
        };
        for (int i = 0; i < 6; ++i) {
            auto* instrumentParam = parameters.getRawParameterValue(accentBenderInstrumentIDs[i]);
            if (instrumentParam) {
                accentBenderController->setInstrumentEnabled(i, instrumentParam->load() > 0.5f);
            }
        }

        // Update slider values
        const std::array<String, 4> accentBenderSliderIDs = {
            "accent_bender_slider_2n", "accent_bender_slider_4n",
            "accent_bender_slider_4nt", "accent_bender_slider_8n"
        };
        for (int i = 0; i < 4; ++i) {
            auto* sliderParam = parameters.getRawParameterValue(accentBenderSliderIDs[i]);
            if (sliderParam) {
                accentBenderController->setSliderValue(i, sliderParam->load());
            }
        }
    }

    // EUCLIDEAN ENGINE PARAMETERS: Update from Euclidean tab parameters
    if (euclideanEngine) {
        for (int ch = 0; ch < 6; ++ch) {
            String prefix = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;

            // Read Euclidean parameters
            auto* stepsParam = parameters.getRawParameterValue(prefix + EUCLIDEAN_STEPS_SUFFIX);
            auto* pulsesParam = parameters.getRawParameterValue(prefix + EUCLIDEAN_PULSES_SUFFIX);
            auto* startOnParam = parameters.getRawParameterValue(prefix + EUCLIDEAN_START_SUFFIX);

            if (stepsParam && pulsesParam && startOnParam) {
                uint8_t steps = static_cast<uint8_t>(stepsParam->load());
                uint8_t pulses = static_cast<uint8_t>(pulsesParam->load());
                uint8_t startOn = static_cast<uint8_t>(startOnParam->load());

                // Update Euclidean engine
                euclideanEngine->setSteps(ch, steps);
                euclideanEngine->setPulses(ch, pulses);
                euclideanEngine->setStartOn(ch, startOn);
            }

            // Set MIDI note for Euclidean engine (reuse same MIDI note as GridsEngine)
            euclideanEngine->setMidiNote(ch, channels[ch].midiNote);
        }
    }

}

void AugmaticGREProcessor::processChannelBlock(ChannelProcessor& channel, int channelIndex, 
                                         AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    
    const int numSamples = buffer.getNumSamples();
    
    // CRITICAL: Process pending note-offs first (for MIDI note assignment changes)
    if (channel.hasPendingNoteOff.load()) {
        // Send note-off for the old MIDI note to prevent hanging notes
        int oldMidiNote = channel.pendingNoteOffMidiNote.load();
        int midiChan = channel.pendingNoteOffMidiChannel.load();
        
        auto noteOffMessage = juce::MidiMessage::noteOff(midiChan, oldMidiNote);
        midiMessages.addEvent(noteOffMessage, 0); // Send immediately at start of buffer
        
        // Clear the pending note-off flag
        channel.hasPendingNoteOff.store(false);
        
    }
    
    // Get base timing from MasterSyncController
    const auto baseClock = syncController.getClockForChannel(channelIndex);

    // Apply clock division/multiplication via ClockDivider
    // Update clock divider ratio from cached parameter pointer
    if (channelParams[channelIndex].clockRatio) {
        clockDividers[channelIndex]->setRatioIndex(channelParams[channelIndex].clockRatio->getIndex());
    }

    // Process clock with division/multiplication
    bool shouldReset = syncController.shouldResetChannel(channelIndex);
    ClockInfo modifiedClock = clockDividers[channelIndex]->processClockInfo(baseClock, shouldReset);
    
    // PHASE 3 IMPLEMENTATION: Apply timing modifications to samples per beat

    // Step 1: Calculate timing from modified BPM
    double baseSamplesPerBeat = (currentSampleRate * 60.0) / modifiedClock.bpm;

    // Transport Sync Integration: ALWAYS reset BOTH engines on transport start
    // This ensures both engines maintain transport sync even when muted
    if (shouldReset) {
        // Reset GridsEngine
        channel.gridsInstance->resetForTransport();

        // Reset EuclideanEngine
        if (euclideanEngine) {
            euclideanEngine->resetChannelForTransport(channelIndex);
        }

        // Reset supporting systems
        clockDividers[channelIndex]->reset();  // Reset divider phase
        if (swingControllers[channelIndex]) {
            swingControllers[channelIndex]->reset();  // Reset swing controller
        }
        syncController.clearChannelResetFlag(channelIndex);
    }

    // NOTE: PPQ sync now handled globally to preserve +1 offset musical alignment

    // Update shift buffer parameters from cached pointers
    if (channelParams[channelIndex].shift) {
        midiShiftBuffers[channelIndex]->setShiftAmount(channelParams[channelIndex].shift->get());
    }
    if (channelParams[channelIndex].humanize) {
        midiShiftBuffers[channelIndex]->setHumanizationAmount(channelParams[channelIndex].humanize->get());
    }

    // Update BPM for shift calculation
    midiShiftBuffers[channelIndex]->setBPM(modifiedClock.bpm);

    // SWING TIMING (v0.3.485): Apply Roger Linn swing algorithm to PPQ position
    if (channelParams[channelIndex].swing && swingControllers[channelIndex]) {
        swingControllers[channelIndex]->setSwingAmount(channelParams[channelIndex].swing->get());
    }

    // Apply swing to PPQ position BEFORE syncing engines
    // This matches the Impromptu Clocked → Valley Topograph architecture:
    // swing lives in the clock signal, not in the pattern algorithm
    // Clock ratio scales the swing grid so it works at any clock division/multiplication
    double swungPPQ = modifiedClock.ppqPosition;
    if (swingControllers[channelIndex]) {
        float effectiveClockRatio = clockDividers[channelIndex]->getEffectiveRatio();
        swungPPQ = swingControllers[channelIndex]->applySwingToPPQ(modifiedClock.ppqPosition, modifiedClock.bpm, effectiveClockRatio);
    }

    // SINGLE-SOURCE PPQ TIMING (v0.4.169): Both engines receive PPQ directly.
    // No separate setStepFromPPQ call needed — PPQ is the sole authority for step position.
    // This eliminates the dual-timing drift that caused tempo acceleration on iPad.

    // PARALLEL PROCESSING ARCHITECTURE:
    // Both engines run continuously to maintain transport sync.
    // Engine Probability parameter determines per-note selection between outputs.
    // This enables zero-latency switching and live parameter tweaking.

    // Process both engines in parallel to separate buffers
    juce::MidiBuffer gridsBuffer;
    juce::MidiBuffer euclideanBuffer;

    // Process GridsEngine (single-source PPQ timing)
    channel.gridsInstance->processBlock(gridsBuffer, numSamples, swungPPQ, modifiedClock.bpm,
                                       modifiedClock.isPlaying, channel.midiChannel + 1);

    // Process EuclideanEngine (single-source PPQ timing - ppqPosition is the sole authority)
    if (euclideanEngine) {
        euclideanEngine->processChannelBlock(channelIndex, euclideanBuffer, numSamples, swungPPQ,
                                            modifiedClock.bpm, modifiedClock.isPlaying, channel.midiChannel + 1);
    }

    // Get Engine Probability parameter (0.0 = 100% Grids, 1.0 = 100% Euclidean)
    auto* engineProbParam = parameters.getRawParameterValue(cachedEngProbParamNames[channelIndex]);
    float engineProbability = engineProbParam ? engineProbParam->load() : 0.0f;

    // Merge buffers based on Engine Probability using a SINGLE random roll per sample position.
    // Both engines use identical PPQ→step mapping and fire at the same sample offsets.
    // Two independent rolls caused duplicates (both pass) and drops (neither passes).
    juce::MidiBuffer channelMidiBuffer;

    // Index notes by sample position from each engine
    std::map<int, juce::MidiMessage> gridsNotes, euclideanNotes;
    for (const auto metadata : gridsBuffer) {
        if (metadata.getMessage().isNoteOn())
            gridsNotes.emplace(metadata.samplePosition, metadata.getMessage());
    }
    for (const auto metadata : euclideanBuffer) {
        if (metadata.getMessage().isNoteOn())
            euclideanNotes.emplace(metadata.samplePosition, metadata.getMessage());
    }

    // Collect all unique sample positions
    std::set<int> allPositions;
    for (auto& [pos, _] : gridsNotes) allPositions.insert(pos);
    for (auto& [pos, _] : euclideanNotes) allPositions.insert(pos);

    // Single random roll per position — mutual exclusion guaranteed
    for (int pos : allPositions) {
        bool hasGrids = gridsNotes.count(pos) > 0;
        bool hasEuclidean = euclideanNotes.count(pos) > 0;
        float randomValue = rngDist(audioRng);

        if (hasGrids && hasEuclidean) {
            // Both engines fired: one roll picks winner
            if (randomValue >= engineProbability)
                channelMidiBuffer.addEvent(gridsNotes.at(pos), pos);
            else
                channelMidiBuffer.addEvent(euclideanNotes.at(pos), pos);
        } else if (hasGrids) {
            if (randomValue >= engineProbability)
                channelMidiBuffer.addEvent(gridsNotes.at(pos), pos);
        } else if (hasEuclidean) {
            if (randomValue < engineProbability)
                channelMidiBuffer.addEvent(euclideanNotes.at(pos), pos);
        }
    }

    // Process the MIDI events from GridsEngine and apply Phase 3 post-processing
    for (const auto metadata : channelMidiBuffer) {
        auto message = metadata.getMessage();

        if (message.isNoteOn()) {
            // Apply Phase 3 post-processing
            uint8_t originalVelocity = static_cast<uint8_t>(message.getVelocity());

            // Apply independent chaos
            uint8_t chaosVelocity = channel.chaosController.generateChaosValue(channelIndex, originalVelocity);


            // Apply velocity processing (simplified without swing)
            auto finalVelocity = channel.velocityController.processVelocity(channelIndex);

            // P1 PROBABILITY CHECK: Filter notes based on probability percentage (BEFORE M1)
            auto* p1Param = parameters.getRawParameterValue(cachedP1ParamNames[channelIndex]);
            float p1Probability = p1Param ? p1Param->load() : 100.0f;

            // Generate random value 0-100 and compare with probability
            float randomValue = rngDist(audioRng) * 100.0f;
            bool passedProbability = (randomValue < p1Probability);

            // Skip note if it didn't pass probability check
            if (!passedProbability) {
                continue;  // Note filtered by probability - skip to next MIDI message
            }

            // Check if channel is muted before processing (M1 - Mute Pre)
            auto* mutePreParam = parameters.getRawParameterValue(cachedMutePreParamNames[channelIndex]);
            bool isMuted = mutePreParam ? static_cast<bool>(mutePreParam->load()) : false;

            // Only filter muted notes here - Solo filtering moved to after Linear Drumming processing
            if (!isMuted) {
                // Collect note for Linear Drumming processing - Solo filtering happens later
                LinearDrummingController::PendingNote note;
                note.channelIndex = channelIndex;
                note.sampleOffset = metadata.samplePosition;
                note.midiNote = channel.midiNote;
                note.velocity = finalVelocity;
                collectedNotes.push_back(note);
            }

            // LED feedback moved to processLinearDrumming to show only actual MIDI output
        }
        else if (message.isNoteOff()) {
            // OPTION 2 IMPLEMENTATION: Ignore NOTE OFF messages from GridsEngine
            // Note duration is now handled by post-processing above
            // GridsEngine's NOTE OFF messages are no longer used to prevent double note-offs
            // The post-processing system schedules NOTE OFF events based on global note duration
        }
    }

    // Create temporary buffer for shifted events
    juce::MidiBuffer shiftedMidiBuffer;

    // Process shifted events from the shift buffer
    midiShiftBuffers[channelIndex]->processShiftedEvents(shiftedMidiBuffer, numSamples);

    // CRITICAL FIX: Collect shifted NOTE ON events for Linear Drumming priority filtering
    // Previously, shifted notes bypassed priority resolution and were output directly
    for (const auto metadata : shiftedMidiBuffer) {
        auto message = metadata.getMessage();

        if (message.isNoteOn()) {
            // Apply Accent Bender velocity modulation to shifted events
            uint8_t velocity = static_cast<uint8_t>(message.getVelocity());
            if (accentBenderController) {
                int instrumentIdx = CHANNEL_TO_INSTRUMENT[channelIndex];
                velocity = accentBenderController->processVelocity(velocity, instrumentIdx, metadata.samplePosition);
            }

            // Collect shifted notes for priority filtering (same as current notes)
            LinearDrummingController::PendingNote note;
            note.channelIndex = channelIndex;
            note.sampleOffset = metadata.samplePosition;
            note.midiNote = message.getNoteNumber();
            note.velocity = velocity;
            note.alreadyShifted = true;  // Mark as already shifted to prevent double-shifting
            collectedNotes.push_back(note);

            // NOTE OFF was already scheduled when note was added to shift buffer
            // Do NOT schedule another NOTE OFF here to avoid double-scheduling
        } else if (message.isNoteOff()) {
            // CRITICAL FIX (v0.3.492): Pass through NOTE OFF messages from shift buffer
            // These are the scheduled NOTE OFF events that went through the shift buffer
            midiMessages.addEvent(message, metadata.samplePosition);
        }
    }
}

//==============================================================================
// OPTIMIZATION METHODS: Pre-compute strings and cache parameter pointers
//==============================================================================

void AugmaticGREProcessor::initializeCachedStrings() {
    // Pre-compute all parameter strings to avoid allocations in audio thread
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        cachedParameterPrefixes[ch] = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;
        cachedChaosParamNames[ch] = cachedParameterPrefixes[ch] + CHAOS_SUFFIX;
        cachedVelValueParamNames[ch] = cachedParameterPrefixes[ch] + VEL_VALUE_SUFFIX;
        cachedVelRandomizeParamNames[ch] = cachedParameterPrefixes[ch] + VEL_RANDOMIZE_SUFFIX;
        cachedEngProbParamNames[ch] = cachedParameterPrefixes[ch] + ENGINE_PROBABILITY_SUFFIX;
        cachedP1ParamNames[ch] = cachedParameterPrefixes[ch] + PROBABILITY_PRE_SUFFIX;
        cachedMutePreParamNames[ch] = cachedParameterPrefixes[ch] + MUTE_PRE_SUFFIX;
        cachedSoloPreParamNames[ch] = cachedParameterPrefixes[ch] + SOLO_PRE_SUFFIX;
        cachedP2ParamNames[ch] = cachedParameterPrefixes[ch] + PROBABILITY_POST_SUFFIX;
        cachedMutePostParamNames[ch] = cachedParameterPrefixes[ch] + MUTE_POST_SUFFIX;
        cachedSoloPostParamNames[ch] = cachedParameterPrefixes[ch] + SOLO_POST_SUFFIX;
    }

    // Priority Matrix cached strings (formerly "Linear Drumming")
    cachedLinearGridParamName = LINEAR_DRUMMING_GRID_SUFFIX;
    const std::array<String, 6> linearPriorityIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };
    for (int i = 0; i < 6; ++i) {
        cachedLinearPriorityParamNames[i] = linearPriorityIDs[i];
    }
}

void AugmaticGREProcessor::initializeParameterPointers() {
    // Cache parameter pointers to avoid string lookups in updateParametersFromUI
    for (size_t ch = 0; ch < NUM_CHANNELS; ++ch) {
        channelParams[ch].chaos = parameters.getRawParameterValue(cachedChaosParamNames[ch]);
        channelParams[ch].velValue = parameters.getRawParameterValue(cachedVelValueParamNames[ch]);
        channelParams[ch].velRandomize = parameters.getRawParameterValue(cachedVelRandomizeParamNames[ch]);
        // Cache typed parameter pointers (eliminates dynamic_cast in audio thread)
        String prefix = cachedParameterPrefixes[ch];
        channelParams[ch].clockRatio = dynamic_cast<AudioParameterChoice*>(parameters.getParameter(prefix + CLOCK_RATIO_SUFFIX));
        channelParams[ch].shift = dynamic_cast<AudioParameterInt*>(parameters.getParameter(prefix + SHIFT_SUFFIX));
        channelParams[ch].humanize = dynamic_cast<AudioParameterInt*>(parameters.getParameter(prefix + HUMANIZE_SUFFIX));
        channelParams[ch].swing = dynamic_cast<AudioParameterFloat*>(parameters.getParameter(prefix + SWING_SUFFIX));
    }
}

//==============================================================================
// NOTE DURATION MODULE - Independent timing system for consistent note durations
//==============================================================================

double AugmaticGREProcessor::calculateNoteDurationSamples() const {
    // CRITICAL: Calculate note duration based on MASTER DAW BPM (not clock-divided timing)
    // Use syncController.getCurrentBPM() to get the actual DAW tempo
    // NOT getClockForChannel(0) which returns channel 0's Clock Ratio-modified BPM!
    double masterBPM = syncController.getCurrentBPM();
    double masterSamplesPerBeat = (currentSampleRate * 60.0) / masterBPM;

    auto* noteDurationParam = parameters.getRawParameterValue("note_duration");
    int noteDuration = noteDurationParam ? static_cast<int>(noteDurationParam->load()) : 2;

    switch (noteDuration) {
        case 0: return masterSamplesPerBeat;        // 4n - Quarter note
        case 1: return masterSamplesPerBeat / 2.0;  // 8n - 8th note
        case 2: return masterSamplesPerBeat / 4.0;  // 16n - 16th note (default)
        case 3: return masterSamplesPerBeat / 8.0;  // 32n - 32nd note
        case 4: return masterSamplesPerBeat / 16.0; // 64n - 64th note
        default: return masterSamplesPerBeat / 4.0; // Default to 16th note
    }
}

void AugmaticGREProcessor::scheduleNoteOff(int channelIndex, int midiChannel, int midiNote, double noteDurationSamples, int64_t humanizationOffset, int noteOnSampleOffset) {
    // GRID-ALIGNED NOTE OFF: Schedule NOTE OFF to occur precisely on grid boundaries
    // This ensures notes don't overlap and creates clean, rhythmic output

    // Calculate the target NOTE OFF sample (note on time + duration)
    // CRITICAL: Must account for note.sampleOffset to handle swing timing correctly
    // NOTE OFF time = exact note ON time + exact duration (no grid snapping)
    // This ensures swing/shift/humanize don't affect note duration
    int64_t targetNoteOffSample = globalSampleCounter + noteOnSampleOffset + static_cast<int64_t>(noteDurationSamples);

    // RATCHET FIX: Check if there's already a pending NOTE OFF for this channel/note
    // This prevents multiple conflicting NOTE OFF events when chaos creates rapid ratchets
    auto existingNoteOff = std::find_if(pendingNoteOffs.begin(), pendingNoteOffs.end(),
        [midiChannel, midiNote](const PendingNoteOff& pending) {
            return pending.midiChannel == midiChannel && pending.midiNote == midiNote;
        });

    if (existingNoteOff != pendingNoteOffs.end()) {
        // Update existing NOTE OFF to extend the note duration
        // Always use the latest (furthest) scheduled time to ensure full note duration
        existingNoteOff->scheduledSample = std::max(existingNoteOff->scheduledSample, targetNoteOffSample);
        // Update humanization offset to match the latest note
        existingNoteOff->humanizationOffset = humanizationOffset;
    } else {
        // No existing pending NOTE OFF - add new one with channelIndex and humanization offset
        // Cap at 1024 entries to prevent unbounded growth under extreme chaos/ratchet settings
        if (pendingNoteOffs.size() < 1024)
            pendingNoteOffs.emplace_back(channelIndex, midiChannel, midiNote, targetNoteOffSample, humanizationOffset);
    }
}

void AugmaticGREProcessor::processNoteDurationModule(MidiBuffer& midiMessages, int numSamples) {
    // Process all pending NOTE OFF events that should occur in this buffer
    auto it = pendingNoteOffs.begin();
    while (it != pendingNoteOffs.end()) {
        // Check if this NOTE OFF should occur in the current buffer
        int64_t bufferStartSample = globalSampleCounter;
        int64_t bufferEndSample = globalSampleCounter + numSamples - 1;

        if (it->scheduledSample >= bufferStartSample && it->scheduledSample <= bufferEndSample) {
            // Calculate the sample offset within this buffer
            int sampleOffset = static_cast<int>(it->scheduledSample - bufferStartSample);
            sampleOffset = juce::jlimit(0, numSamples - 1, sampleOffset);

            // Create NOTE OFF message
            auto noteOffMessage = juce::MidiMessage::noteOff(it->midiChannel, it->midiNote);

            // Check if shift or humanization is enabled for this channel
            int shiftAmount = channelParams[it->channelIndex].shift ? channelParams[it->channelIndex].shift->get() : 63;
            int humanizeAmount = channelParams[it->channelIndex].humanize ? channelParams[it->channelIndex].humanize->get() : 0;

            if (shiftAmount == 63 && humanizeAmount == 0) {
                // No shift/humanize: Add NOTE OFF directly to output
                midiMessages.addEvent(noteOffMessage, sampleOffset);
            } else {
                // Shift/humanize enabled: Route through shift buffer
                // CRITICAL: Use the SAME humanization offset as the NOTE ON
                // This ensures NOTE OFF fires at correct time relative to humanized NOTE ON
                midiShiftBuffers[it->channelIndex]->addMidiEventWithHumanization(
                    noteOffMessage,
                    sampleOffset,
                    it->midiChannel,
                    it->humanizationOffset  // Use stored humanization from NOTE ON
                );
            }

            // Remove this pending NOTE OFF from the list
            it = pendingNoteOffs.erase(it);
        } else if (it->scheduledSample < bufferStartSample) {
            // This NOTE OFF is overdue - send it immediately and remove
            auto noteOffMessage = juce::MidiMessage::noteOff(it->midiChannel, it->midiNote);

            // Check if shift or humanization is enabled
            int shiftAmount = channelParams[it->channelIndex].shift ? channelParams[it->channelIndex].shift->get() : 63;
            int humanizeAmount = channelParams[it->channelIndex].humanize ? channelParams[it->channelIndex].humanize->get() : 0;

            if (shiftAmount == 63 && humanizeAmount == 0) {
                midiMessages.addEvent(noteOffMessage, 0);
            } else {
                midiShiftBuffers[it->channelIndex]->addMidiEventWithHumanization(
                    noteOffMessage, 0, it->midiChannel, it->humanizationOffset);
            }

            it = pendingNoteOffs.erase(it);
        } else {
            // This NOTE OFF is for a future buffer - keep it
            ++it;
        }
    }
}

void AugmaticGREProcessor::resetNoteDurationModule() {
    // Clear all pending NOTE OFF events and reset global sample counter
    // This is called when transport starts to ensure proper synchronization
    pendingNoteOffs.clear();
    globalSampleCounter = 0;
}

void AugmaticGREProcessor::processLinearDrumming(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    if (!linearDrummingController || collectedNotes.empty()) return;

    // PERFORMANCE OPTIMIZATION: Only update parameters when they've changed
    if (linearCache.needsUpdate) {
        // Get current parameters (only when needed) with null checks to prevent crashes
        auto* gridParam = parameters.getRawParameterValue(cachedLinearGridParamName);

        int gridIndex = gridParam ? static_cast<int>(gridParam->load()) : 2; // Default to 16n

        // Validate grid index (must be 0-4 for 64n, 32n, 16n, 8n, 4n)
        if (gridIndex < 0 || gridIndex >= 5) {
            gridIndex = 2; // Default to 16n
        }

        // Get instrument priorities from parameters
        // Each instrument now has its own priority directly (0-5 maps to Priority 1-6)
        for (int i = 0; i < 6; ++i) {
            auto* priorityParam = parameters.getRawParameterValue(cachedLinearPriorityParamNames[i]);
            int priority = priorityParam ? static_cast<int>(priorityParam->load()) : 0; // Default to Priority 1

            // Validate priority (must be 0-5 for Priority 1-6)
            if (priority < 0) priority = 0;
            if (priority >= 6) priority = 5;

            linearCache.priorities[i] = priority;
        }

        linearCache.gridIndex = gridIndex;
        linearCache.needsUpdate = false;

        // Update Priority Matrix parameters (Priority Matrix is always enabled as of v0.3.400)
        // The "enabled" parameter was removed - priority resolution now always active
        linearDrummingController->updateParameters(true, gridIndex, linearCache.priorities);
    }

    // CRITICAL: Apply S1 (Solo1) filtering BEFORE priority processing
    // If any S1 is enabled, only keep notes from S1-enabled instruments
    bool anySolo1Enabled = false;
    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        auto* solo1Param = parameters.getRawParameterValue(cachedSoloPreParamNames[i]);
        if (solo1Param && static_cast<bool>(solo1Param->load())) {
            anySolo1Enabled = true;
            break;
        }
    }

    if (anySolo1Enabled) {
        // Filter out notes that are not from S1-enabled instruments
        for (auto& note : collectedNotes) {
            if (!note.isMuted && note.channelIndex >= 0 && note.channelIndex < NUM_CHANNELS) {
                auto* solo1NoteParam = parameters.getRawParameterValue(cachedSoloPreParamNames[note.channelIndex]);
                bool isSolo1 = solo1NoteParam ? static_cast<bool>(solo1NoteParam->load()) : false;
                if (!isSolo1) {
                    note.isMuted = true;  // Mute notes from non-S1 instruments
                }
            }
        }
    }

    // CRITICAL: Priority Matrix ALWAYS processes, regardless of Linear Drumming enable state
    // The "Linear Drumming Enable" checkbox was incorrectly gating priority resolution
    // Priority Matrix is a core feature and must always work
    double currentBPM = syncController.getCurrentBPM();
    linearDrummingController->processLinearDrumming(collectedNotes, globalSampleCounter, currentBPM);

    // CRITICAL: Apply M2 filtering AFTER priority processing
    // This leaves "holes" - if a Priority 1 instrument is M2-muted, lower priority instruments won't fill its slots
    for (auto& note : collectedNotes) {
        if (!note.isMuted && note.channelIndex >= 0 && note.channelIndex < NUM_CHANNELS) {
            auto* mute2Param = parameters.getRawParameterValue(cachedMutePostParamNames[note.channelIndex]);
            bool isMuted2 = mute2Param ? static_cast<bool>(mute2Param->load()) : false;
            if (isMuted2) {
                note.isMuted = true;  // Mark as muted (leaves holes in pattern)
            }
        }
    }

    // Check if any channel has solo enabled (for Solo filtering after Linear Drumming)
    bool anySoloEnabled = false;
    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        auto* soloPostParam = parameters.getRawParameterValue(cachedSoloPostParamNames[i]);
        if (soloPostParam && static_cast<bool>(soloPostParam->load())) {
            anySoloEnabled = true;
            break;
        }
    }

    // Output all notes (after Linear Drumming processing and Solo filtering)
    for (const auto& note : collectedNotes) {
        if (!note.isMuted) {
            // Bounds check for channel index
            if (note.channelIndex < 0 || note.channelIndex >= 6) {
                continue; // Skip invalid channel indices
            }

            // P2 PROBABILITY CHECK: Filter notes based on probability percentage (AFTER Mix Matrix, BEFORE S2)
            auto* p2Param = parameters.getRawParameterValue(cachedP2ParamNames[note.channelIndex]);
            float p2Probability = p2Param ? p2Param->load() : 100.0f;

            // Generate random value 0-100 and compare with probability
            float p2RandomValue = rngDist(audioRng) * 100.0f;
            bool passedP2Probability = (p2RandomValue < p2Probability);

            // Skip note if it didn't pass P2 probability check
            if (!passedP2Probability) {
                continue;  // Note filtered by P2 probability - skip to next note
            }

            // Apply Solo filtering AFTER Linear Drumming processing (S2 - Solo Post)
            bool isSoloed = false;
            if (anySoloEnabled) {
                auto* soloPostNoteParam = parameters.getRawParameterValue(cachedSoloPostParamNames[note.channelIndex]);
                isSoloed = soloPostNoteParam ? static_cast<bool>(soloPostNoteParam->load()) : false;

                // Skip this note if Solo is enabled but this channel is not soloed
                if (!isSoloed) {
                    continue;
                }
            }

            // Apply Accent Bender velocity modulation as final stage
            uint8_t finalVelocity = note.velocity;
            if (accentBenderController) {
                int instrumentIdx = CHANNEL_TO_INSTRUMENT[note.channelIndex];
                finalVelocity = accentBenderController->processVelocity(note.velocity, instrumentIdx, note.sampleOffset);
            }

            // Create MIDI message
            auto processedMessage = juce::MidiMessage::noteOn(
                channels[note.channelIndex].midiChannel + 1,
                note.midiNote,
                finalVelocity
            );

            // CRITICAL FIX: Check if shift OR humanization is enabled for this channel
            // Only bypass shift buffer if BOTH shift AND humanization are OFF
            int shiftAmount = channelParams[note.channelIndex].shift ? channelParams[note.channelIndex].shift->get() : 63;  // 63 = center/OFF
            int humanizeAmount = channelParams[note.channelIndex].humanize ? channelParams[note.channelIndex].humanize->get() : 0;

            // CRITICAL: If note already came from shift buffer, output directly (don't shift again)
            if (note.alreadyShifted || (shiftAmount == 63 && humanizeAmount == 0)) {
                // Output directly: either already shifted OR shift/humanization both OFF

                // Send NOTE ON
                midiMessages.addEvent(processedMessage, note.sampleOffset);

                // Schedule NOTE OFF only if this is a fresh note (not from shift buffer)
                // Notes from shift buffer already have their NOTE OFF scheduled
                if (!note.alreadyShifted) {
                    double noteDurationSamples = calculateNoteDurationSamples();
                    scheduleNoteOff(
                        note.channelIndex,  // Add channelIndex
                        channels[note.channelIndex].midiChannel + 1,
                        note.midiNote,
                        noteDurationSamples,
                        0,  // No humanization
                        note.sampleOffset  // CRITICAL: Pass note ON sample offset for swing timing
                    );
                }
            } else {
                // Shift OR humanization enabled: Add to shift buffer for time-shifted/humanized output
                // CRITICAL: Capture the humanization offset applied to NOTE ON
                int64_t humanizationOffset = midiShiftBuffers[note.channelIndex]->addMidiEvent(
                    processedMessage,
                    note.sampleOffset,
                    channels[note.channelIndex].midiChannel + 1
                );

                // Schedule NOTE OFF at normal duration
                // CRITICAL: Pass the humanization offset so NOTE OFF uses SAME offset as NOTE ON
                double noteDurationSamples = calculateNoteDurationSamples();
                scheduleNoteOff(
                    note.channelIndex,  // Add channelIndex
                    channels[note.channelIndex].midiChannel + 1,
                    note.midiNote,
                    noteDurationSamples,
                    humanizationOffset,  // Pass humanization offset from NOTE ON
                    note.sampleOffset  // CRITICAL: Pass note ON sample offset for swing timing
                );
            }

            // Trigger LED feedback - THREAD SAFE: Use MessageManager::callAsync
            // to avoid calling GUI repaint() directly from audio thread
            if (ledNotifyCallback) {
                // Capture by value to avoid dangling references
                auto callback = ledNotifyCallback;
                auto midiNote = note.midiNote;
                auto velocity = note.velocity;
                auto chIdx = note.channelIndex;
                int step = (chIdx >= 0 && chIdx < 6 && channels[chIdx].gridsInstance)
                           ? static_cast<int>(channels[chIdx].gridsInstance->getCurrentStep()) : 0;
                ClockInfo ci = syncController.getClockForChannel(0);
                double ppq = ci.ppqPosition;
                double bpmVal = ci.bpm;

                juce::MessageManager::callAsync([callback, midiNote, velocity, chIdx, step, ppq, bpmVal]() {
                    if (callback) {
                        callback(midiNote, velocity, chIdx, step, ppq, bpmVal);
                    }
                });
            }
        }
    }
}

//==============================================================================

void AugmaticGREProcessor::triggerNoteFromUI(int channelIndex, uint8_t velocity)
{
    if (channelIndex < 0 || channelIndex >= 6)
        return;

    // Thread-safe: add to queue for processing in audio thread
    const juce::SpinLock::ScopedLockType lock(manualNoteLock);
    pendingManualNotes.push_back({channelIndex, velocity});
}

// Plugin instantiation
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AugmaticGREProcessor();
}