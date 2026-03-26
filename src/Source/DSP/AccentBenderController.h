#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

/**
 * AccentBenderController - Modulates velocity of outgoing MIDI notes using an audio-rate LFO
 * The LFO shape is determined by 4 bipolar sliders corresponding to beat divisions
 */
class AccentBenderController {
public:
    AccentBenderController();
    ~AccentBenderController() = default;

    // Set slider values (-1.0 to 1.0, where 0 is center)
    void setSliderValue(int sliderIndex, float value);
    float getSliderValue(int sliderIndex) const;

    // Get all slider values for GUI display
    std::array<float, 4> getSliderValues() const { return sliderValues; }

    // Randomize all sliders
    void randomize();

    // Reset all sliders to center (0.0)
    void reset();

    // Set per-instrument enabled state (6 instruments: BD, SN, HH, BD Acc, SN Acc, HH Acc)
    void setInstrumentEnabled(int instrumentIndex, bool enabled);
    bool getInstrumentEnabled(int instrumentIndex) const;

    // Update the LFO position based on transport position
    void updatePosition(double ppqPosition, double bpm, int samplesPerBlock, double sampleRate);

    // Process velocity with the current LFO value (returns original velocity if instrument disabled)
    uint8_t processVelocity(uint8_t inputVelocity, int instrumentIndex, int sampleOffset = 0) const;

    // Get the waveform for visualization (normalized 0-1)
    std::vector<float> getWaveformForDisplay(int numPoints = 256) const;

    // Get current LFO value at a normalized position (0.0 - 1.0 through the bar)
    float getLFOValueAtPosition(float normalizedPosition) const;

private:
    // Calculate the LFO value using smooth interpolation between control points
    float calculateLFOValue(float phase) const;

    // Slider values (-1.0 to 1.0)
    std::array<float, 4> sliderValues;

    // Beat divisions for each slider (1/2, 1/4, 1/6, 1/8 as shown in screenshots)
    const std::array<float, 4> beatDivisions = {0.5f, 0.25f, 1.0f/6.0f, 0.125f};

    // Per-instrument enable states (6 instruments: BD, SN, HH, BD Acc, SN Acc, HH Acc)
    std::array<bool, 6> instrumentEnabled;

    // Position tracking
    double currentPPQ = 0.0;
    double currentBPM = 120.0;
    double sampleRate = 44100.0;

    // LFO phase (0.0 - 1.0)
    float currentPhase = 0.0f;
    float phaseIncrement = 0.0f;

    // Random number generator
    juce::Random random;

    // Waveform cache for efficient rendering
    mutable std::vector<float> waveformCache;
    mutable bool waveformCacheDirty = true;
};