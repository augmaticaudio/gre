#pragma once
#include <juce_core/juce_core.h>
#include "ClockInfo.h"

using namespace juce;

class ClockDivider {
public:
    ClockDivider();

    // Process incoming clock with division/multiplication
    ClockInfo processClockInfo(const ClockInfo& inputClock, bool& shouldReset);

    // Set division/multiplication ratio
    void setRatioIndex(int index);
    int getRatioIndex() const { return ratioIndex; }

    // Get human-readable ratio string
    String getRatioString() const;

    // Reset internal state
    void reset();

    // Get effective clock ratio as a multiplier (>1 = faster, <1 = slower, 1.0 = normal)
    float getEffectiveRatio() const {
        float r = getRatioValue();
        return isDivisionMode() ? (1.0f / r) : r;
    }

private:
    // Ratio lookup table - 18 essential ratios (removed /16 and /12)
    static constexpr float ratioValues[18] = {
        // Division ratios (indices 0-7): /8 to /1.5
        8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.5f,
        // Multiplication ratios (indices 8-17): x1 to x8
        1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f
    };

    static constexpr int multiplicationStartIndex = 8;  // Where multiplications start

    int ratioIndex;
    bool lastTransportState;

    float getRatioValue() const;
    bool isDivisionMode() const;
};