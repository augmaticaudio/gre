#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>

/**
 * @brief Euclidean Rhythm Generator Engine
 *
 * Implements the Bjorklund algorithm for generating Euclidean rhythms.
 * Based on Eugene module from RareBreeds Orbits VCV Rack plugin.
 *
 * Features:
 * - 6 independent rhythm generators (BD, SN, HH, BD_Acc, SN_Acc, HH_Acc)
 * - Steps: Pattern length (2-32 steps, where 1 step = 16th note)
 * - Pulses: Number of hits/beats in the pattern (0-Steps)
 * - Start On: Which step the pattern starts on (1-based, 1 to Steps)
 * - DAW transport sync and timing integration
 */
class EuclideanEngine
{
public:
    EuclideanEngine();
    ~EuclideanEngine() = default;

    // Initialization and setup
    void initialize();
    void reset();
    void setSampleRate(double sampleRate) { current_sample_rate = sampleRate; }

    // Per-channel Euclidean parameters
    void setSteps(uint8_t channel, uint8_t steps);
    void setPulses(uint8_t channel, uint8_t pulses);
    void setStartOn(uint8_t channel, uint8_t startOn);

    // MIDI note assignment (reuses existing MIDI system)
    void setMidiNote(uint8_t channel, uint8_t note);

    // Main processing function - processes all channels together
    // SINGLE-SOURCE TIMING: ppqPosition is the sole authority for step position
    void processBlock(juce::MidiBuffer& midiBuffer, int bufferSize,
                      double ppqPosition, double bpm, bool isPlaying,
                      uint8_t midiChannel = 1, uint8_t velocityOverride = 0);

    // Per-channel processing function - processes only one channel
    // SINGLE-SOURCE TIMING: ppqPosition is the sole authority for step position
    void processChannelBlock(uint8_t channel, juce::MidiBuffer& midiBuffer, int bufferSize,
                            double ppqPosition, double bpm, bool isPlaying,
                            uint8_t midiChannel = 1, uint8_t velocityOverride = 0);

    // Transport-aware reset for DAW sync
    void resetForTransport();
    void resetChannelForTransport(uint8_t channel);

    // Pattern state queries
    uint8_t getCurrentStep(uint8_t channel) const;
    bool isStepActive(uint8_t channel, uint8_t step) const;

    // Public constants
    static const uint8_t kNumChannels = 6;  // BD, SN, HH, BD_Acc, SN_Acc, HH_Acc
    static const uint8_t kMinSteps = 2;
    static const uint8_t kMaxSteps = 32;
    static const uint8_t kStepResolution = 16;  // 16th notes (quarter = 16n/4)

private:
    // Bjorklund algorithm implementation
    std::vector<bool> generateEuclideanRhythm(uint8_t steps, uint8_t pulses) const;
    void applyStartOn(std::vector<bool>& pattern, uint8_t startOn, uint8_t steps) const;

    // Per-channel state
    struct ChannelState {
        uint8_t steps = 16;           // Pattern length (2-32)
        uint8_t pulses = 4;           // Number of hits (0-steps)
        uint8_t startOn = 1;          // Start step (1-based, 1 = no rotation, max = steps)
        uint8_t currentNote = 36;     // MIDI note output
        std::vector<bool> pattern;    // Generated Euclidean pattern
        bool patternDirty = true;     // Needs regeneration

        // Per-channel timing (for independent processing)
        uint32_t step_counter = 0;
        bool pendingReset = false;
        uint8_t lastTriggeredStep = 0xFF;  // Track last triggered step to prevent double-triggers

        ChannelState(uint8_t note = 36) : currentNote(note) {
            pattern.resize(kMaxSteps, false);
        }
    };

    mutable std::array<ChannelState, kNumChannels> channels;

    // Global timing state
    double current_sample_rate = 44100.0;

    // Pattern regeneration helper
    void regeneratePattern(uint8_t channel) const;
    void sendNoteOn(juce::MidiBuffer& buffer, int sampleOffset,
                    uint8_t channel, uint8_t velocity, uint8_t midiChannel);

    // Default MIDI notes matching GridsEngine defaults
    static const std::array<uint8_t, kNumChannels> defaultMidiNotes;
};
