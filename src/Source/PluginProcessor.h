#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include <juce_audio_devices/juce_audio_devices.h>
#include "DSP/GridsEngine.h"
#include "DSP/EuclideanEngine.h"
#include "DSP/IndependentChaosController.h"
#include "DSP/VelocityController.h"
#include "DSP/MasterSyncController.h"
#include "DSP/InternalClockController.h"
#include "DSP/ClockDivider.h"
#include "DSP/MidiShiftBuffer.h"
#include "DSP/SwingController.h"
#include "DSP/LinearDrummingController.h"
#include "DSP/AccentBenderController.h"
#include "DSP/Constants.h"
#include "PresetManager.h"
#include "MidiMappingManager.h"
#include <array>
#include <functional>
#include <algorithm>
#include <memory>
#include <map>
#include <set>

using namespace juce;

class AugmaticGREProcessor : public AudioProcessor, public AudioProcessorValueTreeState::Listener
{
public:
    AugmaticGREProcessor();
    ~AugmaticGREProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String& newName) override;

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter access
    AudioProcessorValueTreeState& getParameters() { return parameters; }

    // Preset Manager access
    PresetManager* getPresetManager() { return presetManager.get(); }

    // MIDI Mapping Manager access
    MidiMappingManager* getMidiMappingManager() { return midiMappingManager.get(); }

    // Linear Drumming cache invalidation
    void invalidateLinearDrummingCache() { linearCache.needsUpdate = true; }

    // Accent Bender Controller access
    AccentBenderController* getAccentBenderController() { return accentBenderController.get(); }

    // Internal Clock Controller access (v0.3.448+)
    InternalClockController* getInternalClock() { return internalClock.get(); }

    // Internal clock state (not in APVTS — hidden from DAW automation)
    void setInternalClockSync(bool syncToDAW) {
        internalClockSyncValue.store(syncToDAW ? 1.0f : 0.0f);
        syncController.setInternalClockMode(!syncToDAW);
    }
    bool getInternalClockSync() const { return internalClockSyncValue.load() > 0.5f; }

    void setInternalClockBPM(float bpm) {
        internalClockBpmValue.store(bpm);
        needsParameterUpdate.store(true);  // Defer setBPM() to audio thread
    }
    float getInternalClockBPM() const { return internalClockBpmValue.load(); }

    // MIDI Output Device Management (for standalone version)
    StringArray getAvailableMidiOutputDevices() const;
    void setMidiOutputDevice(int deviceIndex);
    String getCurrentMidiOutputDevice() const;
    int getCurrentMidiOutputDeviceIndex() const { return currentMidiOutputDeviceIndex; }

    // MIDI Output Channel (all 6 channels use the same output channel)
    void setMidiOutputChannel(int channel);  // 1-indexed (1-16)
    int getMidiOutputChannel() const;        // 1-indexed (1-16)

    // LED feedback - using callback function instead of forward declaration
    std::function<void(int midiNote, uint8_t velocity, int channelIndex, int stepInPattern, double ppqPosition, double bpm)> ledNotifyCallback;

    // Transport start callback - notifies editor when playback starts
    std::function<void()> onTransportStart;

    // Manual note trigger from UI (clickable LEDs/pads)
    void triggerNoteFromUI(int channelIndex, uint8_t velocity = 100);

    // Audio callback diagnostic
    uint64_t getProcessBlockCounter() const { return processBlockCounter.load(); }
    bool isAudioProcessing() const { return audioProcessingActive.load(); }

    // AudioProcessorValueTreeState::Listener implementation
    void parameterChanged(const String& parameterID, float newValue) override;
    
private:
    // Parameter management
    AudioProcessorValueTreeState parameters;
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Preset management
    std::unique_ptr<PresetManager> presetManager;

    // MIDI Mapping management (independent of presets)
    std::unique_ptr<MidiMappingManager> midiMappingManager;

    // Phase 3 Architecture: Three GridsEngine instances, each with ALL channel data but filtered output
    struct ChannelProcessor {
        std::unique_ptr<GridsEngine> gridsInstance; // Contains ALL channel data (BD,SN,HH)
        IndependentChaosController chaosController;
        VelocityController velocityController;

        // Per-channel state
        bool enabled = true;
        bool useEuclidean = false; // Toggle between Grids (false) and Euclidean (true) mode
        int midiChannel = 9; // GM Drum channel (0-indexed, so 10 in 1-indexed)
        int midiNote = 36;   // Output MIDI note
        int assignedChannelIndex = 0; // Which channel this instance outputs (0=BD, 1=SN, 2=HH)

        // Thread-safe pending note-off system
        std::atomic<bool> hasPendingNoteOff{false};
        std::atomic<int> pendingNoteOffMidiNote{36};
        std::atomic<int> pendingNoteOffMidiChannel{10};

        void prepare(double sampleRate, int maximumExpectedSamplesPerBlock) {
            if (gridsInstance) {
                gridsInstance->setSampleRate(sampleRate);
                gridsInstance->initialize();
            }
        }
    };
    
    std::array<ChannelProcessor, 6> channels;  // 0-2: BD,SN,HH  3-5: BD_Acc,SN_Acc,HH_Acc
    std::array<std::unique_ptr<ClockDivider>, 6> clockDividers;  // One per channel for tempo modification
    std::array<std::unique_ptr<MidiShiftBuffer>, 6> midiShiftBuffers;  // One per channel for shift/humanization
    std::array<std::unique_ptr<SwingController>, 6> swingControllers;  // One per channel for swing timing (Roger Linn algorithm)
    MasterSyncController syncController;

    // Internal Clock Controller (v0.3.448+)
    std::unique_ptr<InternalClockController> internalClock;

    // Euclidean Sequencer Engine - handles all 6 channels
    std::unique_ptr<EuclideanEngine> euclideanEngine;

    // Linear Drumming
    std::unique_ptr<LinearDrummingController> linearDrummingController;
    std::vector<LinearDrummingController::PendingNote> collectedNotes; // Buffer for notes before linear processing

    // Accent Bender - master velocity modulation
    std::unique_ptr<AccentBenderController> accentBenderController;

    // Cached parameter strings for Priority Matrix (formerly "Linear Drumming")
    juce::String cachedLinearGridParamName;
    std::array<juce::String, 6> cachedLinearPriorityParamNames;

    // Performance optimization: Cache parameter values
    struct LinearDrummingCache {
        int gridIndex = 2;
        std::array<int, 6> priorities = {{0, 0, 0, 0, 0, 0}}; // Each instrument's priority (0-5 = Priority 1-6)
        std::atomic<bool> needsUpdate{true};
    } linearCache;
    
    
    // Timing
    double currentSampleRate = 44100.0;
    int samplesPerBlock = 512;
    
    // Thread-safe parameter caching
    std::atomic<bool> needsParameterUpdate{false};

    // Optimization: Pre-computed strings to avoid allocations in audio thread
    std::array<juce::String, NUM_CHANNELS> cachedParameterPrefixes;
    std::array<juce::String, NUM_CHANNELS> cachedChaosParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedVelValueParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedVelRandomizeParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedEngProbParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedP1ParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedMutePreParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedSoloPreParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedP2ParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedMutePostParamNames;
    std::array<juce::String, NUM_CHANNELS> cachedSoloPostParamNames;

    // Optimization: Cache parameter pointers to avoid string lookups
    struct ChannelParameters {
        std::atomic<float>* chaos = nullptr;
        std::atomic<float>* velValue = nullptr;
        std::atomic<float>* velRandomize = nullptr;
        // Cached typed parameter pointers (avoid dynamic_cast in audio thread)
        juce::AudioParameterChoice* clockRatio = nullptr;
        juce::AudioParameterInt* shift = nullptr;
        juce::AudioParameterInt* humanize = nullptr;
        juce::AudioParameterFloat* swing = nullptr;
    };
    std::array<ChannelParameters, NUM_CHANNELS> channelParams;

    // Fix thread safety: atomic for cross-thread access (parameterChanged + processBlock)
    std::atomic<int> lastMainChaos{-1};

    // Internal parameters (not exposed to DAW automation, saved/loaded manually)
    std::atomic<float> internalClockSyncValue{1.0f};   // 1.0 = sync to DAW, 0.0 = internal clock
    std::atomic<float> internalClockBpmValue{120.0f};   // Internal clock BPM (20-400)

    // Note Duration Module - Independent timing system for consistent note durations
    struct PendingNoteOff {
        int channelIndex;            // Channel index (0-5) for shift buffer routing
        int midiChannel;             // MIDI channel number
        int midiNote;                // MIDI note number
        int64_t scheduledSample;     // Global sample count when NOTE OFF should occur
        int64_t humanizationOffset;  // Humanization offset from NOTE ON (in samples)

        PendingNoteOff(int chIdx, int ch, int note, int64_t sample, int64_t humanization = 0)
            : channelIndex(chIdx), midiChannel(ch), midiNote(note),
              scheduledSample(sample), humanizationOffset(humanization) {}
    };

    std::vector<PendingNoteOff> pendingNoteOffs;
    int64_t globalSampleCounter = 0;  // Tracks total samples processed since start

    // MIDI Output Device Management (for standalone version)
    std::unique_ptr<MidiOutput> midiOutput;
    std::unique_ptr<MidiOutput> virtualMidiOutput;  // Virtual MIDI port for standalone mode
    int currentMidiOutputDeviceIndex = 0;  // 0 = virtual port (default in standalone), -1 = plugin output
    String currentMidiOutputDeviceName;
    static constexpr const char* VIRTUAL_MIDI_PORT_NAME = "Augmatic GRE";

    // Manual note trigger queue (from UI clicks on LEDs/pads)
    struct ManualNote {
        int channelIndex;
        uint8_t velocity;
    };
    std::vector<ManualNote> pendingManualNotes;
    juce::SpinLock manualNoteLock;  // Thread-safe access between UI and audio thread

    // Private methods for Phase 3
    void processChannelBlock(ChannelProcessor& channel, int channelIndex,
                           AudioBuffer<float>& buffer, MidiBuffer& midiMessages);
    void setupChannelDefaults();
    void updateParametersFromUI();
    void setupParameterListeners();

    // Note Duration Module methods
    void processNoteDurationModule(MidiBuffer& midiMessages, int numSamples);
    void scheduleNoteOff(int channelIndex, int midiChannel, int midiNote, double noteDurationSamples, int64_t humanizationOffset = 0, int noteOnSampleOffset = 0);
    double calculateNoteDurationSamples() const;
    void resetNoteDurationModule();

    // Linear Drumming method
    void processLinearDrumming(AudioBuffer<float>& buffer, MidiBuffer& midiMessages);

    // Optimization methods
    void initializeCachedStrings();
    void initializeParameterPointers();

    // Thread-safe RNG for audio thread (probability, engine blend)
    std::minstd_rand audioRng{std::random_device{}()};
    std::uniform_real_distribution<float> rngDist{0.0f, 1.0f};

    // Audio callback diagnostic
    std::atomic<uint64_t> processBlockCounter{0};
    std::atomic<bool> audioProcessingActive{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AugmaticGREProcessor)
};