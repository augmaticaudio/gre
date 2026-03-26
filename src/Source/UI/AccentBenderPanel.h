#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/AccentBenderController.h"
#include "../MidiMappingManager.h"

using namespace juce;

/**
 * Waveform display component for visualizing the mLFO shape
 */
class AccentWaveformDisplay : public Component, private Timer {
public:
    AccentWaveformDisplay(AccentBenderController& controller);
    ~AccentWaveformDisplay() override;

    void paint(Graphics& g) override;
    void resized() override;

    void updateWaveform();

private:
    void timerCallback() override;

    AccentBenderController& accentController;
    std::vector<float> waveformData;
    Path waveformPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccentWaveformDisplay)
};

/**
 * Panel containing the Accent Bender controls and display
 */
class AccentBenderPanel : public Component, private Slider::Listener,
                         public MidiMappingManager::InstrumentNameListener {
public:
    AccentBenderPanel(AudioProcessorValueTreeState& apvts, AccentBenderController& controller, MidiMappingManager* mappingMgr = nullptr);
    ~AccentBenderPanel() override;

    void resized() override;
    void paint(Graphics& g) override;

    void sliderValueChanged(Slider* slider) override;

    void updateAllCheckboxFromInstruments();

    // MidiMappingManager::InstrumentNameListener
    void instrumentNamesChanged() override;

private:
    void setupSlider(std::unique_ptr<Slider>& slider, const String& paramID,
                     const String& label, int index);
    void randomizeSliders();
    void resetSliders();

    AudioProcessorValueTreeState& parameters;
    AccentBenderController& accentController;
    MidiMappingManager* midiMappingManager = nullptr;

    // Waveform display
    std::unique_ptr<AccentWaveformDisplay> waveformDisplay;

    // Bipolar sliders (4 beat divisions)
    std::array<std::unique_ptr<Slider>, 4> beatSliders;
    std::array<std::unique_ptr<Label>, 4> beatLabels;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 4> sliderAttachments;

    // Header label
    std::unique_ptr<Label> headerLabel;

    // Control buttons
    std::unique_ptr<TextButton> randomButton;
    std::unique_ptr<TextButton> resetButton;

    // Per-instrument enable checkboxes (6 instruments)
    std::array<std::unique_ptr<ToggleButton>, 6> instrumentCheckboxes;
    std::array<std::unique_ptr<Label>, 6> instrumentLabels;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, 6> instrumentAttachments;

    // "All" checkbox
    std::unique_ptr<ToggleButton> allCheckbox;
    std::unique_ptr<Label> allLabel;
    // NOTE: allAttachment removed (v0.4.170) — accent_bender_all no longer in APVTS

    // Beat division labels (renamed to musical notation)
    const std::array<String, 4> beatDivisionLabels = {"2n", "4n", "4nt", "8n"};

    // Instrument names for checkboxes
    const std::array<String, 6> instrumentNames = {"BD", "SN", "HH", "BD Acc", "SN Acc", "HH Acc"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccentBenderPanel)
};