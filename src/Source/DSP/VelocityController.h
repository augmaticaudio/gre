#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <random>

class VelocityController {
public:
    VelocityController();
    ~VelocityController() = default;

    void setChannelVelocityLevel(int channel, uint8_t value);
    void setChannelVelocityRandomization(int channel, float amount);
    uint8_t processVelocity(int channel);

private:
    struct ChannelVelocitySettings {
        uint8_t velocityLevel = 100;         // Fixed velocity (1-127)
        float velocityRandomization = 0.0f;  // 0.0 - 100.0 randomization amount
    };

    std::array<ChannelVelocitySettings, 6> channelSettings;

    // Random number generation for velocity randomization
    std::mt19937 randomEngine;
    std::uniform_real_distribution<float> randomDistribution{-1.0f, 1.0f};
};
