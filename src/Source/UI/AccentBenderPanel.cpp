#include "AccentBenderPanel.h"

//==============================================================================
// AccentWaveformDisplay
//==============================================================================

AccentWaveformDisplay::AccentWaveformDisplay(AccentBenderController& controller)
    : accentController(controller) {
    startTimerHz(30); // Update at 30 FPS
    updateWaveform();
}

AccentWaveformDisplay::~AccentWaveformDisplay() {
    stopTimer();
}

void AccentWaveformDisplay::paint(Graphics& g) {
    // Background
    g.fillAll(Colour(0xff1e1e1e));

    // Draw grid
    float width = static_cast<float>(getWidth());
    float height = static_cast<float>(getHeight());

    // Main vertical grid lines at beat divisions (4 divisions for one bar)
    g.setColour(Colour(0xff595e5f));  // Same as frame border
    for (int i = 1; i < 4; ++i) {
        float x = (width / 4.0f) * i;
        g.drawLine(x, 0, x, height, 0.5f);
    }

    // Additional subdivision lines (8th notes) - slightly dimmer
    g.setColour(Colour(0xff4a4e4f));
    for (int i = 1; i < 8; ++i) {
        if (i % 2 != 0) {  // Skip the main beat lines
            float x = (width / 8.0f) * i;
            g.drawLine(x, 0, x, height, 0.4f);
        }
    }

    // Even finer subdivision lines (16th notes) - more dimmer
    g.setColour(Colour(0xff3a3e3f));
    for (int i = 1; i < 16; ++i) {
        if (i % 2 != 0) {  // Skip the 8th note lines
            float x = (width / 16.0f) * i;
            g.drawLine(x, 0, x, height, 0.3f);
        }
    }

    // Horizontal center line
    g.drawLine(0, height * 0.5f, width, height * 0.5f, 0.5f);

    // Draw waveform in violet to match other sliders
    g.setColour(Colour(0xff9b74f6));
    g.strokePath(waveformPath, PathStrokeType(2.0f));

    // Border
    g.setColour(Colour(0xff595e5f));
    g.drawRect(getLocalBounds(), 1);
}

void AccentWaveformDisplay::resized() {
    updateWaveform();
}

void AccentWaveformDisplay::updateWaveform() {
    waveformPath.clear();

    if (getWidth() <= 0 || getHeight() <= 0) return;

    const int numPoints = getWidth();
    waveformData = accentController.getWaveformForDisplay(numPoints);

    if (!waveformData.empty()) {
        float height = static_cast<float>(getHeight());

        for (int i = 0; i < numPoints; ++i) {
            float x = static_cast<float>(i);
            float y = height * (1.0f - waveformData[i]);

            if (i == 0) {
                waveformPath.startNewSubPath(x, y);
            } else {
                waveformPath.lineTo(x, y);
            }
        }
    }

    repaint();
}

void AccentWaveformDisplay::timerCallback() {
    updateWaveform();
}

//==============================================================================
// AccentBenderPanel
//==============================================================================

AccentBenderPanel::AccentBenderPanel(AudioProcessorValueTreeState& apvts, AccentBenderController& controller, MidiMappingManager* mappingMgr)
    : parameters(apvts), accentController(controller), midiMappingManager(mappingMgr) {

    // Create header label with normal size and white color
    headerLabel = std::make_unique<Label>("headerLabel", "Velocity Bender");
    headerLabel->setFont(Font(14.0f));  // Normal size to match other labels
    headerLabel->setColour(Label::textColourId, Colours::white);  // White color
    headerLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(headerLabel.get());

    // Create waveform display
    waveformDisplay = std::make_unique<AccentWaveformDisplay>(accentController);
    addAndMakeVisible(waveformDisplay.get());

    // Create per-instrument checkboxes
    for (int i = 0; i < 6; ++i) {
        // Checkbox
        instrumentCheckboxes[i] = std::make_unique<ToggleButton>();
        instrumentCheckboxes[i]->setToggleState(true, dontSendNotification);
        instrumentCheckboxes[i]->onClick = [this] { updateAllCheckboxFromInstruments(); };
        addAndMakeVisible(instrumentCheckboxes[i].get());

        // Label — use long-form names from MidiMappingManager if available
        juce::String labelText = midiMappingManager ? midiMappingManager->getInstrumentNameLong(i) : instrumentNames[i];
        instrumentLabels[i] = std::make_unique<Label>("label_" + String(i), labelText);
        instrumentLabels[i]->setJustificationType(Justification::centredLeft);
        instrumentLabels[i]->setColour(Label::textColourId, Colour(0xffa887dc));  // Purple like Euclidean labels
        instrumentLabels[i]->attachToComponent(instrumentCheckboxes[i].get(), true);
        addAndMakeVisible(instrumentLabels[i].get());
    }

    // Create "All" checkbox
    allCheckbox = std::make_unique<ToggleButton>();
    allCheckbox->setToggleState(true, dontSendNotification);
    allCheckbox->onClick = [this] {
        bool allState = allCheckbox->getToggleState();
        for (int i = 0; i < 6; ++i) {
            instrumentCheckboxes[i]->setToggleState(allState, sendNotificationAsync);
        }
    };
    addAndMakeVisible(allCheckbox.get());

    // "All" label
    allLabel = std::make_unique<Label>("allLabel", "All");
    allLabel->setJustificationType(Justification::centredLeft);
    allLabel->setColour(Label::textColourId, Colour(0xffa887dc));  // Purple
    allLabel->attachToComponent(allCheckbox.get(), true);
    addAndMakeVisible(allLabel.get());

    // Create beat division sliders
    for (int i = 0; i < 4; ++i) {
        beatSliders[i] = std::make_unique<Slider>();
        beatSliders[i]->setSliderStyle(Slider::LinearVertical);
        beatSliders[i]->setRange(-1.0, 1.0, 0.01);
        beatSliders[i]->setValue(0.0);
        beatSliders[i]->setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
        beatSliders[i]->addListener(this);
        beatSliders[i]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        beatSliders[i]->setColour(Slider::trackColourId, Colour(0xff404040));
        beatSliders[i]->setColour(Slider::backgroundColourId, Colour(0xff2a2a2a));
        addAndMakeVisible(beatSliders[i].get());

        beatLabels[i] = std::make_unique<Label>("label" + String(i), beatDivisionLabels[i]);
        beatLabels[i]->setJustificationType(Justification::centred);
        beatLabels[i]->setColour(Label::textColourId, Colour(0xff909090));
        addAndMakeVisible(beatLabels[i].get());
    }

    // Create control buttons to match Save/Save As button style
    randomButton = std::make_unique<TextButton>("Random");
    randomButton->onClick = [this] { randomizeSliders(); };
    randomButton->setTooltip("Randomize all sliders");
    randomButton->setColour(TextButton::buttonColourId, Colour(0xff232323));
    randomButton->setColour(TextButton::textColourOffId, Colours::white);
    addAndMakeVisible(randomButton.get());

    resetButton = std::make_unique<TextButton>("Reset");
    resetButton->onClick = [this] { resetSliders(); };
    resetButton->setTooltip("Reset all sliders to center");
    resetButton->setColour(TextButton::buttonColourId, Colour(0xff232323));
    resetButton->setColour(TextButton::textColourOffId, Colours::white);
    addAndMakeVisible(resetButton.get());

    // Create parameter attachments for instrument checkboxes
    const std::array<String, 6> accentBenderInstrumentIDs = {
        "accent_bender_instrument_bd", "accent_bender_instrument_bd_acc",
        "accent_bender_instrument_sn", "accent_bender_instrument_sn_acc",
        "accent_bender_instrument_hh", "accent_bender_instrument_hh_acc"
    };
    for (int i = 0; i < 6; ++i) {
        instrumentAttachments[i] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, accentBenderInstrumentIDs[i], *instrumentCheckboxes[i]);
    }

    // NOTE: accent_bender_all removed from APVTS (v0.4.170) — no attachment needed.
    // "All" checkbox now operates locally (toggles all instrument checkboxes, no state saved).

    // Create parameter attachments for sliders
    const std::array<String, 4> accentBenderSliderIDs = {
        "accent_bender_slider_2n", "accent_bender_slider_4n",
        "accent_bender_slider_4nt", "accent_bender_slider_8n"
    };
    for (int i = 0; i < 4; ++i) {
        sliderAttachments[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            parameters, accentBenderSliderIDs[i], *beatSliders[i]);
    }

    // Register instrument name listener
    if (midiMappingManager)
        midiMappingManager->addInstrumentNameListener(this);
}

AccentBenderPanel::~AccentBenderPanel() {
    // Remove instrument name listener
    if (midiMappingManager)
        midiMappingManager->removeInstrumentNameListener(this);

    for (auto& slider : beatSliders) {
        if (slider) slider->removeListener(this);
    }
}

void AccentBenderPanel::resized() {
    auto bounds = getLocalBounds();
    const int totalHeight = bounds.getHeight();

    // Minimal top padding
    bounds.removeFromTop(8);

    // Header label at 25px from left border (matching BD, SN labels above)
    auto headerRow = bounds.removeFromTop(22);
    headerRow.removeFromLeft(25);  // 25px from left border
    headerLabel->setBounds(headerRow.removeFromLeft(160));

    // More spacing after header
    bounds.removeFromTop(8);

    // Main content area split into LEFT (checkboxes + buttons) and RIGHT (waveform + sliders)
    // Left column for checkboxes and buttons at 95px from left border
    auto leftColumn = bounds.removeFromLeft(188);
    leftColumn.removeFromLeft(95);  // 95px from left border

    // Per-instrument checkboxes (6 instruments) - aligned vertically
    const int checkboxHeight = 20;
    for (int i = 0; i < 6; ++i) {
        auto checkboxBounds = leftColumn.removeFromTop(checkboxHeight);
        instrumentCheckboxes[i]->setBounds(checkboxBounds.reduced(2));
    }

    // Small spacing before "All" checkbox
    leftColumn.removeFromTop(2);

    // "All" checkbox
    auto allBounds = leftColumn.removeFromTop(checkboxHeight);
    allCheckbox->setBounds(allBounds.reduced(2));

    // Space before buttons
    leftColumn.removeFromTop(8);

    // Random and Reset buttons: moved 50px left (from 95px to 45px), width 70px
    auto randomBounds = Rectangle<int>(45, leftColumn.getY(), 70, 26);
    randomButton->setBounds(randomBounds.reduced(2));

    leftColumn.removeFromTop(30);  // Skip space for button

    auto resetBounds = Rectangle<int>(45, leftColumn.getY(), 70, 26);
    resetButton->setBounds(resetBounds.reduced(2));

    // RIGHT SIDE: Waveform and sliders - moved 80px left, width 440px
    auto rightSide = bounds;
    rightSide.removeFromLeft(-65);  // Move 80px left from previous position (15 - 80 = -65)

    // Calculate waveform height - reduced by 20px from previous size
    // Reset button bottom is 160px from plugin window bottom
    // Current Y position of rightSide after removing top elements
    int currentY = 8 + 22 + 8;  // top padding + header + spacing (38px)
    int resetButtonBottom = totalHeight - 160;  // 160px from bottom (270px)
    int waveformHeight = resetButtonBottom - currentY - 20;  // Reduced by 20px: 212px instead of 232px

    // Get the waveform area with calculated height
    auto waveformBounds = rightSide.removeFromTop(waveformHeight);

    // Calculate the waveform's vertical center for slider alignment
    int waveformCenterY = waveformBounds.getHeight() / 2;

    // Split waveform area: left for waveform (440px), right for sliders
    auto waveformArea = waveformBounds.removeFromLeft(440);  // Increased from 380 to 440
    waveformDisplay->setBounds(waveformArea.reduced(5));

    // Sliders area on the right
    waveformBounds.removeFromLeft(20);  // Gap between waveform and sliders
    auto sliderArea = waveformBounds;

    // Slider dimensions - scale to match waveform height, with zero at center line
    const int labelHeight = 20;
    const int sliderHeight = waveformHeight - labelHeight - 10;

    int sliderWidth = sliderArea.getWidth() / 4;
    for (int i = 0; i < 4; ++i) {
        auto sliderColumn = sliderArea.removeFromLeft(sliderWidth);

        // Position sliders so their center (zero position) aligns with waveform center line
        int sliderY = waveformCenterY - (sliderHeight / 2);

        auto sliderBounds = Rectangle<int>(sliderColumn.getX(), sliderColumn.getY() + sliderY,
                                            sliderColumn.getWidth(), sliderHeight);
        beatSliders[i]->setBounds(sliderBounds.reduced(12, 0));

        // Label below slider
        auto labelBounds = Rectangle<int>(sliderColumn.getX(), sliderColumn.getY() + sliderY + sliderHeight + 2,
                                           sliderColumn.getWidth(), labelHeight);
        beatLabels[i]->setBounds(labelBounds);
    }
}

void AccentBenderPanel::updateAllCheckboxFromInstruments() {
    // Check if all instruments are enabled
    bool allEnabled = true;
    for (int i = 0; i < 6; ++i) {
        if (!instrumentCheckboxes[i]->getToggleState()) {
            allEnabled = false;
            break;
        }
    }

    // Update "All" checkbox state without triggering its callback
    allCheckbox->setToggleState(allEnabled, dontSendNotification);
}

void AccentBenderPanel::paint(Graphics& g) {
    // PRODUCTION: Standard panel background
    g.fillAll(Colour(0xff232323));  // PROD_PANEL_BG
}

void AccentBenderPanel::sliderValueChanged(Slider* slider) {
    // Update the controller when sliders change
    for (int i = 0; i < 4; ++i) {
        if (slider == beatSliders[i].get()) {
            accentController.setSliderValue(i, static_cast<float>(slider->getValue()));
            waveformDisplay->updateWaveform();
            break;
        }
    }
}

void AccentBenderPanel::randomizeSliders() {
    Random random;
    for (int i = 0; i < 4; ++i) {
        float value = random.nextFloat() * 2.0f - 1.0f;
        beatSliders[i]->setValue(value, sendNotificationAsync);
    }
}

void AccentBenderPanel::resetSliders() {
    for (int i = 0; i < 4; ++i) {
        beatSliders[i]->setValue(0.0, sendNotificationAsync);
    }
}

void AccentBenderPanel::instrumentNamesChanged() {
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<AccentBenderPanel>(this)]() {
        if (safeThis == nullptr) return;
        if (safeThis->midiMappingManager)
        {
            for (int i = 0; i < 6; ++i)
                safeThis->instrumentLabels[i]->setText(safeThis->midiMappingManager->getInstrumentNameLong(i), dontSendNotification);
        }
    });
}