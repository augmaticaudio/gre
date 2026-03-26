#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include <array>

class GridsEngine
{
public:
    GridsEngine();
    ~GridsEngine() = default;

    void initialize();
    void reset();
    void setSampleRate(double sampleRate) { current_sample_rate = sampleRate; }
    
    void setXCoordinate(uint8_t x) { x_coordinate = x; }
    void setYCoordinate(uint8_t y) { y_coordinate = y; }
    void setDensity(uint8_t channel, uint8_t density) { 
        if (channel < kNumChannels) channel_density[channel] = density; 
    }
    void setChaos(uint8_t chaos) { chaos_amount = chaos; }
    void setMidiNote(uint8_t channel, uint8_t note, juce::MidiBuffer* midiBuffer = nullptr, uint8_t midiChannel = 1);
    
    // Single-source PPQ timing (v0.4.169): PPQ is the sole authority for step position
    // PPQ passed directly — no separate setStepFromPPQ needed
    void processBlock(juce::MidiBuffer& midiBuffer, int bufferSize,
                      double ppqPosition, double bpm, bool isPlaying, uint8_t midiChannel = 1);


    uint8_t getCurrentStep() const { return lastTriggeredStep; }

    // Transport-aware reset methods for DAW sync
    void resetForTransport();

    // Public constants for other classes to use
    static const uint8_t kNumChannels = 3;
    static const uint8_t kStepsPerPattern = 32;

private:

    static constexpr uint8_t MAX_PATTERN_LEVEL = 255;
    static constexpr uint8_t PATTERN_DATA_SIZE = 96;
    static constexpr uint8_t MIN_VELOCITY = 80;
    static constexpr uint8_t MAX_VELOCITY = 127;
    
    uint8_t x_coordinate = 128;
    uint8_t y_coordinate = 128;
    uint8_t channel_density[kNumChannels] = {0, 0, 0};
    uint8_t chaos_amount = 0;
    
    double current_sample_rate = 44100.0;

    // Single-source PPQ timing: lastTriggeredStep prevents double-triggers
    uint8_t lastTriggeredStep = 255;  // 255 = no step triggered yet

    // Optimized memory layout: Array of Structures for better cache locality
    struct ChannelState {
        uint8_t currentNote;
        bool noteActive;
        double noteOffTimer;

        ChannelState(uint8_t note = 36) : currentNote(note), noteActive(false), noteOffTimer(0.0) {}
    };
    std::array<ChannelState, kNumChannels> channels{ChannelState(36), ChannelState(38), ChannelState(42)};

    // Thread-safe random number generation
    // Note: Removed mutable member variable to avoid race conditions
    
public:
    // Make these methods public for Phase 3 architecture
    uint8_t readDrumPattern(uint8_t step, uint8_t channel, uint8_t x, uint8_t y) const;
    uint8_t interpolatePatterns(uint8_t step, uint8_t channel, uint8_t x, uint8_t y) const;

private:
    uint8_t mixValues(uint8_t a, uint8_t b, uint8_t balance) const;
    
    static const uint8_t drum_patterns[25][96];
    static const uint8_t drum_map[5][5];
    
    void sendNoteOff(juce::MidiBuffer& buffer, int sampleOffset, uint8_t channel, uint8_t midiChannel);
    void sendNoteOn(juce::MidiBuffer& buffer, int sampleOffset, uint8_t channel, uint8_t velocity, uint8_t midiChannel);
};