#include "IndependentChaosController.h"

IndependentChaosController::IndependentChaosController() {
    resetChaosPhase();
}

void IndependentChaosController::setChannelChaos(int channel, float chaosAmount) {
    if (channel >= 0 && channel < 3) {
        channelChaos[channel] = juce::jlimit(0.0f, 1.0f, chaosAmount);
    }
}

uint8_t IndependentChaosController::generateChaosValue(int channel, uint8_t baseLevel) {
    if (channel < 0 || channel >= 3) return baseLevel;
    
    // Apply chaos using Grids' original algorithm pattern
    uint8_t randomness = uint8_t(channelChaos[channel] * 255.0f);
    uint8_t perturbation = multiplyShift8(channelRandomizers[channel].nextInt(), randomness);
    
    // Apply perturbation like original Grids
    if (baseLevel < 255 - perturbation) {
        return baseLevel + perturbation;
    }
    return 255;
}

void IndependentChaosController::resetChaosPhase() {
    for (auto& randomizer : channelRandomizers) {
        randomizer.setSeed(juce::Time::currentTimeMillis());
    }
}

uint8_t IndependentChaosController::multiplyShift8(uint32_t a, uint32_t b) const {
    return uint8_t((a * b) >> 8);
}