#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "MidiMappingManager.h"

using namespace juce;

// Forward declaration
class AugmaticGREProcessor;

class LinearDrummingMatrix : public Component,
                           public AudioProcessorValueTreeState::Listener,
                           public MidiMappingManager::InstrumentNameListener
{
public:
    LinearDrummingMatrix(AudioProcessorValueTreeState& params, AugmaticGREProcessor& processor);
    ~LinearDrummingMatrix() override;

    void paint(Graphics& g) override;
    void resized() override;

    // Update matrix from current parameter values
    void updateFromParameters();

    // Get current assignments for parameter saving
    std::array<int, 6> getCurrentAssignments() const;

    // AudioProcessorValueTreeState::Listener
    void parameterChanged(const String& parameterID, float newValue) override;

    // MidiMappingManager::InstrumentNameListener
    void instrumentNamesChanged() override;

private:
    static constexpr int NUM_INSTRUMENTS = 6;
    static constexpr int NUM_PRIORITIES = 6;
    static constexpr int NUM_COLUMNS = 12;  // P1 + M1 + S1 + 6 priorities + P2 + S2 + M2

    // Instrument names (rows) - MUST match CHANNEL_NAMES order in Constants.h
    const std::array<String, NUM_INSTRUMENTS> instrumentNames = {
        "BD", "SN", "HH", "BD Acc", "SN Acc", "HH Acc"
    };

    // All column headers (P1, M1, S1, 1-6, P2, S2, M2)
    const std::array<String, NUM_COLUMNS> columnHeaders = {
        "P1", "M1", "S1", "1", "2", "3", "4", "5", "6", "P2", "S2", "M2"
    };

    AudioProcessorValueTreeState& parameters;
    AugmaticGREProcessor& audioProcessor;

    // Matrix of toggle buttons [instrument][priority]
    std::array<std::array<std::unique_ptr<ToggleButton>, NUM_PRIORITIES>, NUM_INSTRUMENTS> matrixButtons;

    // P1 (Probability Pre) sliders [instrument] - horizontal sliders before M1 column
    std::array<std::unique_ptr<Slider>, NUM_INSTRUMENTS> p1Sliders;

    // P2 (Probability Post) sliders [instrument] - horizontal sliders before S2 column
    std::array<std::unique_ptr<Slider>, NUM_INSTRUMENTS> p2Sliders;

    // M1, M2, S1, S2 toggle buttons [instrument]
    std::array<std::unique_ptr<ToggleButton>, NUM_INSTRUMENTS> m1Buttons;
    std::array<std::unique_ptr<ToggleButton>, NUM_INSTRUMENTS> m2Buttons;
    std::array<std::unique_ptr<ToggleButton>, NUM_INSTRUMENTS> s1Buttons;
    std::array<std::unique_ptr<ToggleButton>, NUM_INSTRUMENTS> s2Buttons;

    // Parameter attachments for P1 and P2 sliders
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_INSTRUMENTS> p1Attachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_INSTRUMENTS> p2Attachments;

    // Parameter attachments for M1, M2, S1, S2
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_INSTRUMENTS> m1Attachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_INSTRUMENTS> m2Attachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_INSTRUMENTS> s1Attachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_INSTRUMENTS> s2Attachments;

    // Row labels
    std::array<std::unique_ptr<Label>, NUM_INSTRUMENTS> rowLabels;

    // Column labels (two levels)
    std::array<std::unique_ptr<Label>, NUM_COLUMNS> columnLabels;  // Level 2: M1, S1, 1-6, S2, M2
    std::unique_ptr<Label> preHeaderLabel;      // Level 1: "Pre"
    std::unique_ptr<Label> priorityHeaderLabel; // Level 1: "Priority Matrix"
    std::unique_ptr<Label> postHeaderLabel;     // Level 1: "Post"

    // Current assignments: 0-5 = priority (each instrument must have exactly one)
    std::array<int, NUM_INSTRUMENTS> currentAssignments;

    // "All" row checkboxes (for each column: M1, S1, P1-P6, S2, M2)
    std::array<std::unique_ptr<ToggleButton>, NUM_COLUMNS> allRowButtons;
    std::unique_ptr<Label> allRowLabel;

    // "100%" buttons for P1 and P2 columns in "All" row
    std::unique_ptr<TextButton> p1All100Button;  // Sets all P1 sliders to 100%
    std::unique_ptr<TextButton> p2All100Button;  // Sets all P2 sliders to 100%

    // Flag to prevent recursive parameter updates when user clicks "All" button
    bool isUpdatingParameters = false;

    // Button click handlers
    void onMatrixButtonClicked(int instrument, int priority);
    void onAllRowButtonClicked(int column);
    void updateDisplayStates();
    void updateParameterFromMatrix();
    void updateAllRowCheckboxes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinearDrummingMatrix)
};