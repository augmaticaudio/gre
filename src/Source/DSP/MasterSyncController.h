#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "ClockInfo.h"

// Forward declaration to avoid circular dependency
class InternalClockController;

class MasterSyncController {
public:
    MasterSyncController() = default;
    ~MasterSyncController() = default;
    
    void updateFromPlayHead(juce::AudioPlayHead* playHead, int numSamples);
    ClockInfo getClockForChannel(int channelIndex) const;
    void setSampleRate(double sampleRate) { currentSampleRate = sampleRate; }
    void resetAllChannelPhases();

    // Global step counter for pattern synchronization
    uint32_t getGlobalStepCounter() const { return globalStepCounter; }
    void advanceGlobalStep() { globalStepCounter++; }
    void resetGlobalStep() { globalStepCounter = 0; }

    // BPM access for timing calculations
    double getCurrentBPM() const { return currentBPM; }

    // Transport state tracking for DAW sync
    bool hasTransportStarted() const { return transportJustStarted; }
    bool hasTransportStopped() const { return transportJustStopped; }
    bool shouldResetChannel(int channelIndex) const {
        return channelIndex < 6 ? channelNeedsReset[channelIndex] : false;
    }
    void clearChannelResetFlag(int channelIndex) {
        if (channelIndex < 6) channelNeedsReset[channelIndex] = false;
    }

    // Internal clock support (v0.3.448+)
    void setInternalClockMode(bool useInternal) { useInternalClockMode = useInternal; }
    void setInternalClock(InternalClockController* clock) { internalClock = clock; }
    bool isInternalClockMode() const { return useInternalClockMode; }

private:
    double currentBPM = 120.0;
    double lastPpqPosition = 0.0;
    int samplesProcessedSinceLastCheck = 0;
    bool transportIsPlaying = false;
    double currentSampleRate = 44100.0;
    
    // Global step counter shared by all channels
    uint32_t globalStepCounter = 0;
    bool wasPlayingLastTime = false;

    // Transport state tracking for DAW sync
    bool transportJustStarted = false;
    bool transportJustStopped = false;

    // Per-channel reset flags for 6 GridsEngine instances
    std::array<bool, 6> channelNeedsReset = {false};

    // Internal clock mode (v0.3.448+)
    bool useInternalClockMode = false;
    InternalClockController* internalClock = nullptr;

    void detectPositionJumps(double newPpqPosition);
};