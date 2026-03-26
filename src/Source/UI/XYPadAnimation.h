#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class XYPadAnimation : public juce::Component, private juce::Timer
{
public:
    XYPadAnimation(juce::AudioProcessorValueTreeState& apvts);
    ~XYPadAnimation() override;

    // Called from onMIDINoteGenerated — already on GUI thread
    // channelIndex: 0=BD, 1=SN, 2=HH (horizontal L→R), 3=BD', 4=SN', 5=HH' (vertical T→B)
    void triggerNote(int channelIndex, uint8_t velocity, double ppqPosition, double bpm);

    // Called when user interacts with XY pad (drag/click)
    void triggerUserRipple(float x, float y);

    // Called on playback start to randomize line assignments
    void randomizeLineAssignments();

    // Match dot size to the purple cursor dot on the XY pad
    void setDotRadius(float radius);

    // Enable/disable animation
    void setAnimationEnabled(bool enabled) { animationEnabled = enabled; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // ---- Data Structures ----

    struct Splash
    {
        float x = 0.0f;
        float y = 0.0f;
        float alpha = 0.0f;
        float age = 0.0f;           // Age in frames
        float holdFrames = 15.0f;   // Frames at full alpha (8th note)
        float fadeFrames = 15.0f;   // Frames to fade out (8th note)
        float dotRadius = 12.5f;
        juce::Colour colour;
        bool active = false;
    };

    struct Halo
    {
        float x = 0.0f;
        float y = 0.0f;
        float currentRadius = 0.0f;
        float maxRadius = 100.0f;
        float expansionSpeed = 1.5f;
        float alpha = 0.0f;
        float decayRate = 0.01f;
        juce::Colour colour;
        bool active = false;
    };

    // ---- Constants ----
    static constexpr int kNumTracks = 6;
    static constexpr int kMaxSplashes = 24;
    static constexpr int kMaxHalos = 48;
    static constexpr int kTimerHz = 60;

    // ---- Members ----
    juce::AudioProcessorValueTreeState& parameters;

    std::array<Splash, kMaxSplashes> splashes;
    int nextSplashIndex = 0;

    std::array<Halo, kMaxHalos> halos;
    int nextHaloIndex = 0;

    float dotRadius = 12.5f;
    bool animationEnabled = true;

    // Line assignment: maps channelIndex to line position (0-5)
    // Randomized on playback start, preserves track colors
    std::array<int, kNumTracks> lineAssignments = {0, 1, 2, 3, 4, 5};

    // Track colours indexed by channelIndex (BD=0, SN=1, HH=2, BD'=3, SN'=4, HH'=5)
    static constexpr uint32_t trackColourValues[kNumTracks] = {
        0xffff4757,  // BD  - Red
        0xff5f9fec,  // SN  - Blue
        0xfffeca57,  // HH  - Yellow
        0xffff6b9d,  // BD' - Pink
        0xff1dd1a1,  // SN' - Green
        0xffff9f43   // HH' - Orange
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYPadAnimation)
};
