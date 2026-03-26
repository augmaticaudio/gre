#include "InternalClockController.h"
#include <juce_core/juce_core.h>

void InternalClockController::setSampleRate(double newSampleRate) {
    sampleRate = newSampleRate;
}

void InternalClockController::setBPM(double bpm) {
    // Clamp BPM to valid range: 40.0 - 240.0 (standalone mode reasonable range)
    currentBPM = juce::jlimit(40.0, 240.0, bpm);
}

void InternalClockController::setPlaying(bool isPlaying) {
    // If transitioning from stopped to playing, reset PPQ position
    if (isPlaying && !playing) {
        ppqPosition = 0.0;
    }
    playing = isPlaying;
}

void InternalClockController::updateClock(int numSamples) {
    // Only increment PPQ when playing
    if (playing) {
        ppqPosition += calculatePPQIncrement(numSamples);
    }
}

ClockInfo InternalClockController::getClockInfo() const {
    ClockInfo info;
    info.ppqPosition = ppqPosition;
    info.bpm = currentBPM;
    info.isPlaying = playing;
    info.sampleOffset = 0;  // Internal clock has no sample offset
    return info;
}

void InternalClockController::reset() {
    ppqPosition = 0.0;
}

double InternalClockController::calculatePPQIncrement(int numSamples) const {
    // Calculate samples per quarter note at current BPM
    // At 120 BPM: 1 beat = 0.5 seconds
    // samples_per_quarter_note = (60.0 / BPM) * sampleRate
    double samplesPerQuarterNote = (60.0 / currentBPM) * sampleRate;

    // Calculate PPQ increment for this buffer
    // ppq_increment = numSamples / samples_per_quarter_note
    return static_cast<double>(numSamples) / samplesPerQuarterNote;
}
