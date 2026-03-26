#include "AccentBenderController.h"
#include <cmath>

AccentBenderController::AccentBenderController() {
    reset();
    // Initialize all instruments as enabled by default
    instrumentEnabled.fill(true);
}

void AccentBenderController::setSliderValue(int sliderIndex, float value) {
    if (sliderIndex >= 0 && sliderIndex < 4) {
        sliderValues[sliderIndex] = juce::jlimit(-1.0f, 1.0f, value);
        waveformCacheDirty = true;
    }
}

float AccentBenderController::getSliderValue(int sliderIndex) const {
    if (sliderIndex >= 0 && sliderIndex < 4) {
        return sliderValues[sliderIndex];
    }
    return 0.0f;
}

void AccentBenderController::randomize() {
    for (int i = 0; i < 4; ++i) {
        sliderValues[i] = random.nextFloat() * 2.0f - 1.0f;
    }
    waveformCacheDirty = true;
}

void AccentBenderController::reset() {
    for (int i = 0; i < 4; ++i) {
        sliderValues[i] = 0.0f;
    }
    waveformCacheDirty = true;
}

void AccentBenderController::setInstrumentEnabled(int instrumentIndex, bool enabled) {
    if (instrumentIndex >= 0 && instrumentIndex < 6) {
        instrumentEnabled[instrumentIndex] = enabled;
    }
}

bool AccentBenderController::getInstrumentEnabled(int instrumentIndex) const {
    if (instrumentIndex >= 0 && instrumentIndex < 6) {
        return instrumentEnabled[instrumentIndex];
    }
    return true;  // Default to enabled if out of range
}

void AccentBenderController::updatePosition(double ppqPosition, double bpm, int samplesPerBlock, double sr) {
    currentPPQ = ppqPosition;
    currentBPM = bpm;
    sampleRate = sr;

    // Calculate phase within current bar (0.0 - 1.0)
    // Assuming 4/4 time signature, one bar = 4 beats
    double barPosition = std::fmod(ppqPosition, 4.0) / 4.0;
    currentPhase = static_cast<float>(barPosition);

    // Calculate phase increment per sample for smooth interpolation
    double beatsPerSecond = bpm / 60.0;
    double barsPerSecond = beatsPerSecond / 4.0;
    phaseIncrement = static_cast<float>(barsPerSecond / sampleRate);
}

uint8_t AccentBenderController::processVelocity(uint8_t inputVelocity, int instrumentIndex, int sampleOffset) const {
    // Check if this instrument has AB enabled
    if (instrumentIndex < 0 || instrumentIndex >= 6 || !instrumentEnabled[instrumentIndex]) {
        return inputVelocity;
    }

    // Calculate phase for this specific sample
    float phase = currentPhase + (phaseIncrement * sampleOffset);
    phase = std::fmod(phase, 1.0f);

    // Get LFO value at current position (-1 to 1)
    float lfoValue = calculateLFOValue(phase);

    // Convert LFO value to velocity modulation
    // LFO > 0: increase velocity
    // LFO < 0: decrease velocity
    // Scale by input velocity to make modulation proportional
    float velocityMultiplier = 1.0f + lfoValue;

    // Apply to input velocity
    float outputVelocity = static_cast<float>(inputVelocity) * velocityMultiplier;

    // Clamp to valid MIDI range
    return static_cast<uint8_t>(juce::jlimit(1.0f, 127.0f, outputVelocity));
}

float AccentBenderController::getLFOValueAtPosition(float normalizedPosition) const {
    return calculateLFOValue(normalizedPosition);
}

float AccentBenderController::calculateLFOValue(float phase) const {
    // Based on screenshot analysis, the waveform uses smooth sine-like interpolation
    // between the control points at different beat divisions

    // We'll use a weighted sum of sine waves at different frequencies
    // corresponding to our beat divisions
    float value = 0.0f;
    float totalWeight = 0.0f;

    for (int i = 0; i < 4; ++i) {
        // Calculate frequency based on beat division
        float frequency = 1.0f / beatDivisions[i];

        // PHASE SHIFT: Different for each beat division
        // All should start at highest position when slider is up
        float phaseShift;
        if (i == 0) {  // 1/2 beat
            phaseShift = -0.125f;  // Shift by -1/8
        } else if (i == 1) {  // 1/4 beat
            phaseShift = -0.0625f;  // Shift by -1/16
        } else if (i == 2) {  // 1/6 beat
            phaseShift = -0.0417f;  // Shift by -1/24
        } else {  // 1/8 beat
            phaseShift = -0.03125f;  // Shift by -1/32
        }

        // Create a sine wave at this frequency with phase shift
        float adjustedPhase = phase + phaseShift;
        float sineValue = std::sin(2.0f * juce::MathConstants<float>::pi * frequency * adjustedPhase);

        // INVERT the slider value so up = positive curve starts high
        // When slider is up (+1), we want the sine to start at its peak
        float invertedSliderValue = -sliderValues[i];

        // Weight by slider value
        float weight = std::abs(invertedSliderValue);
        value += sineValue * invertedSliderValue;
        totalWeight += weight;
    }

    // Normalize if we have any active sliders
    if (totalWeight > 0.001f) {
        value /= (totalWeight + 1.0f); // Soft normalization to prevent harsh clipping
    }

    // Apply a smoothing function to create the smooth curves seen in screenshots
    // Using tanh for soft saturation
    value = std::tanh(value * 1.5f);

    return value;
}

std::vector<float> AccentBenderController::getWaveformForDisplay(int numPoints) const {
    if (waveformCacheDirty || waveformCache.size() != numPoints) {
        waveformCache.resize(numPoints);

        for (int i = 0; i < numPoints; ++i) {
            float phase = static_cast<float>(i) / static_cast<float>(numPoints - 1);
            float lfoValue = calculateLFOValue(phase);

            // Normalize to 0-1 range for display
            waveformCache[i] = (lfoValue + 1.0f) * 0.5f;
        }

        waveformCacheDirty = false;
    }

    return waveformCache;
}
