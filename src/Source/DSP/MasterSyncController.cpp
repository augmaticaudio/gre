#include "MasterSyncController.h"
#include "InternalClockController.h"

void MasterSyncController::updateFromPlayHead(juce::AudioPlayHead* playHead, int numSamples) {
    samplesProcessedSinceLastCheck = numSamples;

    // Clear single-frame transport transition flags
    transportJustStarted = false;
    transportJustStopped = false;

    // INTERNAL CLOCK MODE (v0.3.448+)
    if (useInternalClockMode && internalClock) {
        // Update internal clock
        internalClock->updateClock(numSamples);

        // Get clock info
        ClockInfo info = internalClock->getClockInfo();

        // Update internal state
        bool newTransportState = info.isPlaying;

        // Detect transport start/stop
        if (newTransportState && !wasPlayingLastTime) {
            transportJustStarted = true;
            for (int i = 0; i < 6; ++i) {
                channelNeedsReset[i] = true;
            }
            resetGlobalStep();
        }
        else if (!newTransportState && wasPlayingLastTime) {
            transportJustStopped = true;
            for (int i = 0; i < 6; ++i) {
                channelNeedsReset[i] = true;
            }
        }

        transportIsPlaying = newTransportState;
        wasPlayingLastTime = newTransportState;
        currentBPM = info.bpm;
        lastPpqPosition = info.ppqPosition;

        return;  // Skip DAW playHead processing
    }

    // DAW PLAYHEAD MODE (existing behavior)
    if (!playHead) {
        transportIsPlaying = true; // Fallback: assume playing
        return;
    }

    if (auto position = playHead->getPosition()) {
        // Handle tempo changes
        if (auto bpm = position->getBpm()) {
            if (!juce::approximatelyEqual(*bpm, currentBPM)) {
                currentBPM = *bpm;
            }
        }

        // Update transport state
        bool newTransportState = position->getIsPlaying();

        // Enhanced transport state detection for DAW sync
        if (newTransportState && !wasPlayingLastTime) {
            transportJustStarted = true;
            // Mark all channels for reset
            for (int i = 0; i < 6; ++i) {
                channelNeedsReset[i] = true;
            }
            resetGlobalStep();
        }
        else if (!newTransportState && wasPlayingLastTime) {
            transportJustStopped = true;
            // Also mark for reset on stop for next start
            for (int i = 0; i < 6; ++i) {
                channelNeedsReset[i] = true;
            }
        }

        transportIsPlaying = newTransportState;
        wasPlayingLastTime = newTransportState;

        // Detect position jumps (loops)
        if (auto ppqPos = position->getPpqPosition()) {
            detectPositionJumps(*ppqPos);
            lastPpqPosition = *ppqPos;
        }
    }
}

ClockInfo MasterSyncController::getClockForChannel(int /* channelIndex */) const {
    ClockInfo info;
    info.ppqPosition = lastPpqPosition;
    info.bpm = currentBPM;
    info.isPlaying = transportIsPlaying;
    info.sampleOffset = samplesProcessedSinceLastCheck;
    return info;
}

void MasterSyncController::resetAllChannelPhases() {
    // This would be called when position jumps are detected
    // Mark all channels for reset
    for (int i = 0; i < 6; ++i) {
        channelNeedsReset[i] = true;
    }
    resetGlobalStep(); // Also reset global step counter on position jumps
}

void MasterSyncController::detectPositionJumps(double newPpqPosition) {
    if (lastPpqPosition == 0.0 || samplesProcessedSinceLastCheck == 0) {
        // First position update or zero-length buffer — can't estimate expected position
        return;
    }

    // FIX (v0.4.169): Correct PPQ advance formula
    // OLD (bug): denominator was samplesPerBeat * 4 = samplesPerBar (×4 too large)
    // due to C++ left-to-right precedence: sampleRate * 60.0 / BPM * 4.0
    // This made expected advance 4× too small, causing false jump detection
    // on iOS when buffer size >= 1024 samples.
    double expectedPosition = lastPpqPosition +
        (static_cast<double>(samplesProcessedSinceLastCheck) * currentBPM / (currentSampleRate * 60.0));
    double threshold = 1.0 / 32.0; // 32nd note threshold

    if (std::abs(newPpqPosition - expectedPosition) > threshold) {
        resetAllChannelPhases(); // Position jump detected
    }
}