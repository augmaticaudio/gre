#include "ClockDivider.h"

ClockDivider::ClockDivider()
    : ratioIndex(8)  // Default to ×1 (index 8 in ratioValues array)
    , lastTransportState(false)
{
}

ClockInfo ClockDivider::processClockInfo(const ClockInfo& inputClock, bool& /*shouldReset*/) {
    ClockInfo outputClock = inputClock;

    // Detect transport state changes
    if (!lastTransportState && inputClock.isPlaying) {
        // shouldReset is already set by MasterSyncController
    }
    lastTransportState = inputClock.isPlaying;

    // Apply clock division/multiplication
    float ratio = getRatioValue();
    bool isDiv = isDivisionMode();

    if (isDiv) {
        // Division: Slow down the clock
        // BPM gets divided (slower tempo)
        outputClock.bpm = inputClock.bpm / ratio;

        // CRITICAL: PPQ position must also be divided for correct sync
        // Example: /2 ratio at PPQ 8.0 → engine sees PPQ 4.0 (half the musical position)
        outputClock.ppqPosition = inputClock.ppqPosition / ratio;
    } else {
        // Multiplication: Speed up the clock
        // BPM gets multiplied (faster tempo)
        outputClock.bpm = inputClock.bpm * ratio;

        // CRITICAL: PPQ position must also be multiplied for correct sync
        // Example: x2 ratio at PPQ 4.0 → engine sees PPQ 8.0 (double the musical position)
        outputClock.ppqPosition = inputClock.ppqPosition * ratio;
    }

    // Clamp BPM to reasonable range
    outputClock.bpm = juce::jlimit(10.0, 999.0, outputClock.bpm);

    // Preserve all other ClockInfo fields
    outputClock.isPlaying = inputClock.isPlaying;
    outputClock.sampleOffset = inputClock.sampleOffset;

    return outputClock;
}

void ClockDivider::setRatioIndex(int index) {
    ratioIndex = juce::jlimit(0, 17, index);  // 18 entries: indices 0-17
}

float ClockDivider::getRatioValue() const {
    if (ratioIndex >= 0 && ratioIndex < 18) {  // Match array size of 18
        return ratioValues[ratioIndex];
    }
    return 1.0f;  // Default to x1
}

bool ClockDivider::isDivisionMode() const {
    return ratioIndex < multiplicationStartIndex;
}

String ClockDivider::getRatioString() const {
    float ratio = getRatioValue();
    bool isDiv = isDivisionMode();

    // Format as division or multiplication using standard ASCII characters
    if (isDiv) {
        if (ratio == 1.5f) return "/1.5";
        if (ratio == 2.5f) return "/2.5";
        return "/" + String(static_cast<int>(ratio));
    } else {
        if (ratio == 1.5f) return "x1.5";
        if (ratio == 2.5f) return "x2.5";
        return "x" + String(static_cast<int>(ratio));
    }
}

void ClockDivider::reset() {
    lastTransportState = false;
}