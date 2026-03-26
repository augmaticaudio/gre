#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "GridsEngine.h"

class IndependentChaosController {
public:
    IndependentChaosController();
    ~IndependentChaosController() = default;
    
    void setChannelChaos(int channel, float chaosAmount);
    uint8_t generateChaosValue(int channel, uint8_t baseLevel);
    void resetChaosPhase();
    
private:
    std::array<float, 3> channelChaos = {0.0f, 0.0f, 0.0f}; // BD, SN, HH
    std::array<juce::Random, 3> channelRandomizers;
    
    // Replicate Grids' multiply-shift operation
    uint8_t multiplyShift8(uint32_t a, uint32_t b) const;
};