#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <map>

class LinearDrummingController {
public:
    struct PendingNote {
        int channelIndex;
        int sampleOffset;
        int midiNote;
        uint8_t velocity;
        bool isMuted = false;
        bool alreadyShifted = false;  // True if note came from shift buffer (don't shift again)
    };

    struct MuteWindow {
        int64_t startSample = -1;
        int64_t endSample = -1;
        bool active = false;
        int triggeringChannel = -1; // Which channel caused this mute
    };

    void initialize(double sampleRate);
    void processLinearDrumming(std::vector<PendingNote>& notes, int64_t globalSamplePosition, double currentBPM);
    void updateParameters(bool enabled, int gridDuration, const std::array<int, 6>& instrumentPriorities);
    bool isChannelMuted(int channelIndex, int64_t samplePosition);
    void reset(); // For transport reset

private:
    bool enabled_ = false;
    int gridDurationIndex_ = 2; // Default to 16n
    std::array<int, 6> instrumentPriorities_; // Each instrument's priority (0-5 maps to Priority 1-6)
    std::array<MuteWindow, 6> channelMuteWindows_;
    double sampleRate_ = 44100.0;

    // Grid duration in samples for each setting
    static constexpr double GRID_DURATIONS_QUARTERS[] = {0.0625, 0.125, 0.25, 0.5, 1.0}; // 64n, 32n, 16n, 8n, 4n

    int64_t calculateGridDurationSamples(double bpm) const;
    void applyPriorityLogic(const std::vector<size_t>& noteIndices, std::vector<PendingNote>& notes, int64_t samplePosition, double currentBPM);
    void updateMuteWindows(int64_t currentSample);
};