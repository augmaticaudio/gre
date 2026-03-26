#include "VelocityController.h"

VelocityController::VelocityController()
    : randomEngine(std::random_device{}())
{
}

void VelocityController::setChannelVelocityLevel(int channel, uint8_t value) {
    if (channel >= 0 && channel < 6) {
        channelSettings[channel].velocityLevel = juce::jlimit(uint8_t(1), uint8_t(127), value);
    }
}

void VelocityController::setChannelVelocityRandomization(int channel, float amount) {
    if (channel >= 0 && channel < 6) {
        channelSettings[channel].velocityRandomization = juce::jlimit(0.0f, 100.0f, amount);
    }
}

uint8_t VelocityController::processVelocity(int channel) {
    if (channel < 0 || channel >= 6) return 100;

    const auto& settings = channelSettings[channel];

    float velocity = float(settings.velocityLevel);

    // Apply randomization if enabled
    if (settings.velocityRandomization > 0.0f) {
        float randomFactor = randomDistribution(randomEngine);
        float randomAmount = (settings.velocityRandomization / 100.0f) * randomFactor * velocity;
        velocity += randomAmount;
    }

    return uint8_t(juce::jlimit(1.0f, 127.0f, velocity));
}
