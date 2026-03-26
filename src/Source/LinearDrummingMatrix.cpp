#include "LinearDrummingMatrix.h"
#include "DSP/Constants.h"
#include "PluginProcessor.h"

using namespace juce;

LinearDrummingMatrix::LinearDrummingMatrix(AudioProcessorValueTreeState& params, AugmaticGREProcessor& processor)
    : parameters(params), audioProcessor(processor)
{
    // Initialize all instruments to Priority 1 by default
    for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
        currentAssignments[i] = 0; // Priority 1 (index 0)
    }

    // Add parameter listeners for all linear drumming parameters
    const std::array<String, 6> linearPriorityIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };
    for (int i = 0; i < 6; ++i) {
        parameters.addParameterListener(linearPriorityIDs[i], this);
    }

    // Add parameter listeners for M1, M2, S1, S2 to update "All" row checkboxes on preset load
    for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
        String channelPrefix = String(CHANNEL_NAMES[i]) + PARAMETER_SEPARATOR;
        parameters.addParameterListener(channelPrefix + MUTE_PRE_SUFFIX, this);
        parameters.addParameterListener(channelPrefix + MUTE_POST_SUFFIX, this);
        parameters.addParameterListener(channelPrefix + SOLO_PRE_SUFFIX, this);
        parameters.addParameterListener(channelPrefix + SOLO_POST_SUFFIX, this);
    }

    // Create level 1 header labels (Pre, Priority Matrix, Post)
    preHeaderLabel = std::make_unique<Label>("preHeader", "Pre");
    preHeaderLabel->setJustificationType(Justification::centred);
    preHeaderLabel->setColour(Label::textColourId, Colours::white);
    preHeaderLabel->setFont(Font(16.0f, Font::bold));
    addAndMakeVisible(preHeaderLabel.get());

    priorityHeaderLabel = std::make_unique<Label>("priorityHeader", "Priority");
    priorityHeaderLabel->setJustificationType(Justification::centred);
    priorityHeaderLabel->setColour(Label::textColourId, Colours::white);
    priorityHeaderLabel->setFont(Font(16.0f, Font::bold));
    addAndMakeVisible(priorityHeaderLabel.get());

    postHeaderLabel = std::make_unique<Label>("postHeader", "Post");
    postHeaderLabel->setJustificationType(Justification::centred);
    postHeaderLabel->setColour(Label::textColourId, Colours::white);
    postHeaderLabel->setFont(Font(16.0f, Font::bold));
    addAndMakeVisible(postHeaderLabel.get());

    // Create level 2 column labels (M1, S1, 1-6, S2, M2)
    for (int col = 0; col < NUM_COLUMNS; ++col) {
        columnLabels[col] = std::make_unique<Label>("col_" + String(col), columnHeaders[col]);
        columnLabels[col]->setJustificationType(Justification::centred);
        columnLabels[col]->setColour(Label::textColourId, Colours::white);
        columnLabels[col]->setFont(Font(14.0f, Font::bold));
        addAndMakeVisible(columnLabels[col].get());
    }

    // Create "All" row label
    allRowLabel = std::make_unique<Label>("allRowLabel", "All");
    allRowLabel->setJustificationType(Justification::centredLeft);
    allRowLabel->setColour(Label::textColourId, Colours::white);
    allRowLabel->setFont(Font(14.0f, Font::bold));
    addAndMakeVisible(allRowLabel.get());

    // Create "All" row checkboxes for each column
    for (int col = 0; col < NUM_COLUMNS; ++col) {
        allRowButtons[col] = std::make_unique<ToggleButton>();
        allRowButtons[col]->setButtonText("");
        allRowButtons[col]->onClick = [this, col] { onAllRowButtonClicked(col); };
        allRowButtons[col]->setColour(ToggleButton::tickColourId, Colours::white);
        allRowButtons[col]->setColour(ToggleButton::tickDisabledColourId, Colours::darkgrey);
        addAndMakeVisible(allRowButtons[col].get());
    }

    // Create "100%" button for P1 column (All-P1 cell)
    p1All100Button = std::make_unique<TextButton>("100%");
    p1All100Button->onClick = [this]() {
        // Set all P1 sliders to 100%
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            p1Sliders[row]->setValue(100.0, sendNotification);
        }
    };
    p1All100Button->setColour(TextButton::buttonColourId, Colour(0xff404040));
    p1All100Button->setColour(TextButton::textColourOffId, Colours::white);
    addAndMakeVisible(p1All100Button.get());

    // Create "100%" button for P2 column (All-P2 cell)
    p2All100Button = std::make_unique<TextButton>("100%");
    p2All100Button->onClick = [this]() {
        // Set all P2 sliders to 100%
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            p2Sliders[row]->setValue(100.0, sendNotification);
        }
    };
    p2All100Button->setColour(TextButton::buttonColourId, Colour(0xff404040));
    p2All100Button->setColour(TextButton::textColourOffId, Colours::white);
    addAndMakeVisible(p2All100Button.get());

    // Create row labels (instrument names from MidiMappingManager) and matrix buttons
    auto* mappingMgr = processor.getMidiMappingManager();
    for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
        // Row labels — use long-form names from MidiMappingManager if available
        juce::String labelText = mappingMgr ? mappingMgr->getInstrumentNameLong(row) : instrumentNames[row];
        rowLabels[row] = std::make_unique<Label>("row_" + String(row), labelText);
        rowLabels[row]->setJustificationType(Justification::centredLeft);
        rowLabels[row]->setColour(Label::textColourId, Colours::white);
        rowLabels[row]->setFont(Font(14.0f, Font::bold));
        addAndMakeVisible(rowLabels[row].get());

        // P1 (Probability Pre) slider (before M1 column) - horizontal slider, 0-100%, default 100%
        p1Sliders[row] = std::make_unique<Slider>();
        p1Sliders[row]->setSliderStyle(Slider::LinearHorizontal);
        p1Sliders[row]->setRange(0.0, 100.0, 1.0);
        p1Sliders[row]->setValue(100.0);
        p1Sliders[row]->setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
        p1Sliders[row]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        p1Sliders[row]->setColour(Slider::trackColourId, Colour(0xff404040));
        p1Sliders[row]->setColour(Slider::backgroundColourId, Colour(0xff2a2a2a));
        addAndMakeVisible(p1Sliders[row].get());

        // Create P1 parameter attachment
        String p1ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + PROBABILITY_PRE_SUFFIX;
        p1Attachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            parameters, p1ParamName, *p1Sliders[row]);

        // M1 button (mute before priority) - mutually exclusive with M2
        m1Buttons[row] = std::make_unique<ToggleButton>();
        m1Buttons[row]->setButtonText("");
        m1Buttons[row]->setColour(ToggleButton::tickColourId, Colours::white);
        m1Buttons[row]->setColour(ToggleButton::tickDisabledColourId, Colours::darkgrey);
        m1Buttons[row]->onClick = [this, row]() {
            // If M1 is being enabled, disable M2
            if (m1Buttons[row]->getToggleState()) {
                String m2ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + MUTE_POST_SUFFIX;
                auto* m2Param = parameters.getRawParameterValue(m2ParamName);
                if (m2Param && m2Param->load() > 0.5f) {
                    m2Param->store(0.0f);
                    // Null check: m2Buttons may not exist yet during construction
                    if (m2Buttons[row])
                        m2Buttons[row]->setToggleState(false, dontSendNotification);
                }
            }
            // Update "All" row checkboxes
            updateAllRowCheckboxes();
        };
        addAndMakeVisible(m1Buttons[row].get());

        // Create M1 parameter attachment
        String m1ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + MUTE_PRE_SUFFIX;
        m1Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, m1ParamName, *m1Buttons[row]);

        // S1 button (solo before priority) - mutually exclusive with S2
        s1Buttons[row] = std::make_unique<ToggleButton>();
        s1Buttons[row]->setButtonText("");
        s1Buttons[row]->setColour(ToggleButton::tickColourId, Colours::white);
        s1Buttons[row]->setColour(ToggleButton::tickDisabledColourId, Colours::darkgrey);
        s1Buttons[row]->onClick = [this, row]() {
            // If S1 is being enabled, disable S2
            if (s1Buttons[row]->getToggleState()) {
                String s2ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + SOLO_POST_SUFFIX;
                auto* s2Param = parameters.getRawParameterValue(s2ParamName);
                if (s2Param && s2Param->load() > 0.5f) {
                    s2Param->store(0.0f);
                    // Null check: s2Buttons may not exist yet during construction
                    if (s2Buttons[row])
                        s2Buttons[row]->setToggleState(false, dontSendNotification);
                }
            }
            // Update "All" row checkboxes
            updateAllRowCheckboxes();
        };
        addAndMakeVisible(s1Buttons[row].get());

        // Create S1 parameter attachment
        String s1ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + SOLO_PRE_SUFFIX;
        s1Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, s1ParamName, *s1Buttons[row]);

        // Priority matrix buttons
        for (int col = 0; col < NUM_PRIORITIES; ++col) {
            matrixButtons[row][col] = std::make_unique<ToggleButton>();
            matrixButtons[row][col]->setButtonText("");
            matrixButtons[row][col]->onClick = [this, row, col] { onMatrixButtonClicked(row, col); };
            matrixButtons[row][col]->setColour(ToggleButton::tickColourId, Colours::white);
            matrixButtons[row][col]->setColour(ToggleButton::tickDisabledColourId, Colours::darkgrey);
            addAndMakeVisible(matrixButtons[row][col].get());
        }

        // P2 (Probability Post) slider (before S2 column) - horizontal slider, 0-100%, default 100%
        p2Sliders[row] = std::make_unique<Slider>();
        p2Sliders[row]->setSliderStyle(Slider::LinearHorizontal);
        p2Sliders[row]->setRange(0.0, 100.0, 1.0);
        p2Sliders[row]->setValue(100.0);
        p2Sliders[row]->setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
        p2Sliders[row]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        p2Sliders[row]->setColour(Slider::trackColourId, Colour(0xff404040));
        p2Sliders[row]->setColour(Slider::backgroundColourId, Colour(0xff2a2a2a));
        addAndMakeVisible(p2Sliders[row].get());

        // Create P2 parameter attachment
        String p2ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + PROBABILITY_POST_SUFFIX;
        p2Attachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            parameters, p2ParamName, *p2Sliders[row]);

        // S2 button (solo after priority) - mutually exclusive with S1
        s2Buttons[row] = std::make_unique<ToggleButton>();
        s2Buttons[row]->setButtonText("");
        s2Buttons[row]->setColour(ToggleButton::tickColourId, Colours::white);
        s2Buttons[row]->setColour(ToggleButton::tickDisabledColourId, Colours::darkgrey);
        s2Buttons[row]->onClick = [this, row]() {
            // If S2 is being enabled, disable S1
            if (s2Buttons[row]->getToggleState()) {
                String s1ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + SOLO_PRE_SUFFIX;
                auto* s1Param = parameters.getRawParameterValue(s1ParamName);
                if (s1Param && s1Param->load() > 0.5f) {
                    s1Param->store(0.0f);
                    s1Buttons[row]->setToggleState(false, dontSendNotification);
                }
            }
            // Update "All" row checkboxes
            updateAllRowCheckboxes();
        };
        addAndMakeVisible(s2Buttons[row].get());

        // Create S2 parameter attachment (uses SOLO_POST_SUFFIX)
        String s2ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + SOLO_POST_SUFFIX;
        s2Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, s2ParamName, *s2Buttons[row]);

        // M2 button (mute after priority) - mutually exclusive with M1
        m2Buttons[row] = std::make_unique<ToggleButton>();
        m2Buttons[row]->setButtonText("");
        m2Buttons[row]->setColour(ToggleButton::tickColourId, Colours::white);
        m2Buttons[row]->setColour(ToggleButton::tickDisabledColourId, Colours::darkgrey);
        m2Buttons[row]->onClick = [this, row]() {
            // If M2 is being enabled, disable M1
            if (m2Buttons[row]->getToggleState()) {
                String m1ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + MUTE_PRE_SUFFIX;
                auto* m1Param = parameters.getRawParameterValue(m1ParamName);
                if (m1Param && m1Param->load() > 0.5f) {
                    m1Param->store(0.0f);
                    m1Buttons[row]->setToggleState(false, dontSendNotification);
                }
            }
            // Update "All" row checkboxes
            updateAllRowCheckboxes();
        };
        addAndMakeVisible(m2Buttons[row].get());

        // Create M2 parameter attachment (uses MUTE_POST_SUFFIX)
        String m2ParamName = String(CHANNEL_NAMES[row]) + PARAMETER_SEPARATOR + MUTE_POST_SUFFIX;
        m2Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, m2ParamName, *m2Buttons[row]);
    }

    // Initialize from current parameter values (in case preset was already loaded)
    updateFromParameters();

    // Initialize "All" row checkboxes
    updateAllRowCheckboxes();

    // Register instrument name listener
    if (mappingMgr)
        mappingMgr->addInstrumentNameListener(this);
}

LinearDrummingMatrix::~LinearDrummingMatrix()
{
    // Remove instrument name listener
    if (auto* mappingMgr = audioProcessor.getMidiMappingManager())
        mappingMgr->removeInstrumentNameListener(this);

    // Remove parameter listeners
    const std::array<String, 6> linearPriorityIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };
    for (int i = 0; i < 6; ++i) {
        parameters.removeParameterListener(linearPriorityIDs[i], this);
    }

    // Remove M1, M2, S1, S2 parameter listeners
    for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
        String channelPrefix = String(CHANNEL_NAMES[i]) + PARAMETER_SEPARATOR;
        parameters.removeParameterListener(channelPrefix + MUTE_PRE_SUFFIX, this);
        parameters.removeParameterListener(channelPrefix + MUTE_POST_SUFFIX, this);
        parameters.removeParameterListener(channelPrefix + SOLO_PRE_SUFFIX, this);
        parameters.removeParameterListener(channelPrefix + SOLO_POST_SUFFIX, this);
    }
}

void LinearDrummingMatrix::parameterChanged(const String& parameterID, float newValue)
{
    ignoreUnused(newValue);

    // CRITICAL: Prevent recursive updates when we're batch-updating parameters from UI
    // This fixes the "All" button clicking one instrument at a time issue
    if (isUpdatingParameters) {
        return;
    }

    // When a linear drumming parameter changes (e.g., from preset loading),
    // update the UI to reflect the new values
    if (parameterID.startsWith(LINEAR_DRUMMING_PRIORITY_SUFFIX)) {
        // Direct call - parameterChanged is already on message thread via APVTS
        updateFromParameters();
    }
    // When M1, M2, S1, S2 parameters change (e.g., from preset loading),
    // update "All" row checkboxes to match the new state
    else if (parameterID.contains(MUTE_PRE_SUFFIX) || parameterID.contains(MUTE_POST_SUFFIX) ||
             parameterID.contains(SOLO_PRE_SUFFIX) || parameterID.contains(SOLO_POST_SUFFIX)) {
        // Direct call - parameterChanged is already on message thread via APVTS
        updateAllRowCheckboxes();
    }
}

void LinearDrummingMatrix::paint(Graphics& g)
{
    // Clip painting to component bounds to prevent overlapping other components
    g.reduceClipRegion(getLocalBounds());

    g.fillAll(Colour(0xff232323));

    // Draw grid lines
    g.setColour(Colour(0xff404040));

    const int totalWidth = getWidth();
    const int cellHeight = 30;
    const int level1HeaderHeight = 25;  // Level 1 header: Pre, Priority Matrix, Post
    const int level2HeaderHeight = 25;  // Level 2 header: M1, S1, 1-6, S2, M2
    const int totalHeaderHeight = level1HeaderHeight + level2HeaderHeight;

    // Custom width layout: Fixed 150px for P1 and P2 columns, remaining space for checkboxes
    const int baseWidth = totalWidth / 12;
    const int rowLabelWidth = baseWidth + 20;
    const int p1ColumnWidth = 150;  // Fixed width for P1 (Probability Pre) sliders
    const int p2ColumnWidth = 150;  // Fixed width for P2 (Probability Post) sliders
    const int remainingWidth = totalWidth - rowLabelWidth - p1ColumnWidth - p2ColumnWidth;
    const int checkboxColumnWidth = remainingWidth / 10;  // 10 checkbox columns share remaining space

    // Helper lambda to get column X position
    // Column layout: P1(0), M1(1), S1(2), Priorities 1-6(3-8), P2(9), S2(10), M2(11)
    auto getColumnX = [&](int col) -> int {
        if (col == 0) return rowLabelWidth;  // P1 column
        if (col <= 8) return rowLabelWidth + p1ColumnWidth + ((col - 1) * checkboxColumnWidth);  // M1, S1, Priorities 1-6
        if (col == 9) return rowLabelWidth + p1ColumnWidth + (8 * checkboxColumnWidth);  // P2 column
        return rowLabelWidth + p1ColumnWidth + (8 * checkboxColumnWidth) + p2ColumnWidth + ((col - 10) * checkboxColumnWidth);  // S2, M2
    };

    // Add space for "All" row
    const int allRowHeight = cellHeight;
    const int matrixBodyHeight = NUM_INSTRUMENTS * cellHeight;
    const int totalHeight = totalHeaderHeight + matrixBodyHeight + allRowHeight;

    // Draw complete border rectangle
    g.drawRect(0, 0, totalWidth, totalHeight, 1);

    // Draw horizontal line between level 1 and level 2 headers
    g.drawHorizontalLine(level1HeaderHeight, 0, totalWidth);

    // Draw horizontal line between headers and matrix body
    g.drawHorizontalLine(totalHeaderHeight, 0, totalWidth);

    // Draw vertical line between row labels and first column (Prob)
    g.drawVerticalLine(rowLabelWidth, 0, totalHeight);

    // Draw vertical lines for level 1 header sections (Pre | Priority Matrix | Post)
    // Pre section ends at S1 (column 2): P1 + M1 + S1
    int preEndX = getColumnX(3);  // Start of Priority column
    g.drawVerticalLine(preEndX, 0, level1HeaderHeight);  // Only in level 1 header

    // Priority Matrix section ends after column 8 (priority 6)
    int priorityEndX = getColumnX(9);  // Start of P2 column
    g.drawVerticalLine(priorityEndX, 0, level1HeaderHeight);  // Only in level 1 header

    // Draw vertical lines between columns (with selective removal)
    // Draw line after P1 (column 0), before M1 (column 1)
    int p1EndX = getColumnX(1);  // Start of M1 column
    g.drawVerticalLine(p1EndX, level1HeaderHeight, totalHeight);

    // REMOVED: Line between M1 (1) and S1 (2) - no line drawn
    // REMOVED: Lines between priorities 1-6 (columns 3-8) - no lines drawn

    // Draw line after S1 (column 2), before Priority 1 (column 3)
    int s1EndX = getColumnX(3);
    g.drawVerticalLine(s1EndX, level1HeaderHeight, totalHeight);

    // Draw line after Priority 6 (column 8), before P2 (column 9)
    int priority6EndX = getColumnX(9);
    g.drawVerticalLine(priority6EndX, level1HeaderHeight, totalHeight);

    // Draw line after P2 (column 9), before S2 (column 10)
    int p2EndX = getColumnX(10);  // Start of S2 column
    g.drawVerticalLine(p2EndX, level1HeaderHeight, totalHeight);

    // REMOVED: Line between S2 (10) and M2 (11) - no line drawn

    // Right border is handled by drawRect

    // Horizontal lines for instrument rows
    for (int row = 1; row < NUM_INSTRUMENTS; ++row) {
        int y = totalHeaderHeight + (row * cellHeight);
        g.drawHorizontalLine(y, 0, totalWidth);
    }

    // Draw horizontal line before "All" row
    int allRowY = totalHeaderHeight + matrixBodyHeight;
    g.drawHorizontalLine(allRowY, 0, totalWidth);
}

void LinearDrummingMatrix::resized()
{
    const int totalWidth = getWidth();
    const int cellHeight = 30;
    const int level1HeaderHeight = 25;  // Level 1 header: Pre, Priority Matrix, Post
    const int level2HeaderHeight = 25;  // Level 2 header: M1, S1, 1-6, S2, M2
    const int totalHeaderHeight = level1HeaderHeight + level2HeaderHeight;

    // Custom width layout: Fixed 150px for P1 and P2 columns, remaining space for checkboxes
    const int baseWidth = totalWidth / 12;
    const int rowLabelWidth = baseWidth + 20;
    const int p1ColumnWidth = 150;  // Fixed width for P1 (Probability Pre) sliders
    const int p2ColumnWidth = 150;  // Fixed width for P2 (Probability Post) sliders
    const int remainingWidth = totalWidth - rowLabelWidth - p1ColumnWidth - p2ColumnWidth;
    const int checkboxColumnWidth = remainingWidth / 10;  // 10 checkbox columns share remaining space

    const int checkboxSize = 24;
    const int allRowHeight = cellHeight;
    const int matrixBodyHeight = NUM_INSTRUMENTS * cellHeight;

    // Helper lambda to get column X position
    // Column layout: P1(0), M1(1), S1(2), Priorities 1-6(3-8), P2(9), S2(10), M2(11)
    auto getColumnX = [&](int col) -> int {
        if (col == 0) return rowLabelWidth;  // P1 column
        if (col <= 8) return rowLabelWidth + p1ColumnWidth + ((col - 1) * checkboxColumnWidth);  // M1, S1, Priorities 1-6
        if (col == 9) return rowLabelWidth + p1ColumnWidth + (8 * checkboxColumnWidth);  // P2 column
        return rowLabelWidth + p1ColumnWidth + (8 * checkboxColumnWidth) + p2ColumnWidth + ((col - 10) * checkboxColumnWidth);  // S2, M2
    };

    // Helper lambda to get column width
    auto getColumnWidth = [&](int col) -> int {
        if (col == 0) return p1ColumnWidth;  // P1 column
        if (col == 9) return p2ColumnWidth;  // P2 column
        return checkboxColumnWidth;  // All other columns
    };

    // Position level 1 header labels (Pre, Priority Matrix, Post)
    // Pre: spans P1 + M1 + S1 (columns 0-2)
    int preX = rowLabelWidth;
    int preWidth = p1ColumnWidth + (2 * checkboxColumnWidth);
    preHeaderLabel->setBounds(preX, 0, preWidth, level1HeaderHeight);

    // Priority Matrix: spans priorities 1-6 (columns 3-8)
    int priorityX = getColumnX(3);
    int priorityWidth = 6 * checkboxColumnWidth;
    priorityHeaderLabel->setBounds(priorityX, 0, priorityWidth, level1HeaderHeight);

    // Post: spans P2 + S2 + M2 (columns 9-11)
    int postX = getColumnX(9);
    int postWidth = p2ColumnWidth + (2 * checkboxColumnWidth);
    postHeaderLabel->setBounds(postX, 0, postWidth, level1HeaderHeight);

    // Position level 2 column labels (Prob, M1, S1, 1-6, S2, M2)
    for (int col = 0; col < NUM_COLUMNS; ++col) {
        columnLabels[col]->setBounds(getColumnX(col), level1HeaderHeight, getColumnWidth(col), level2HeaderHeight);
    }

    // Position row labels and all buttons (M1, S1, P1-P6, S2, M2) for instruments
    for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
        int y = totalHeaderHeight + (row * cellHeight);
        int checkboxXOffset = (checkboxColumnWidth - checkboxSize) / 2;
        int yOffset = (cellHeight - checkboxSize) / 2;

        // Row label
        rowLabels[row]->setBounds(5, y, rowLabelWidth - 10, cellHeight);

        // P1 slider (column 0) - horizontal slider with reduced padding
        p1Sliders[row]->setBounds(getColumnX(0) + 8, y + 2, p1ColumnWidth - 16, cellHeight - 4);

        // M1 button (column 1)
        m1Buttons[row]->setBounds(getColumnX(1) + checkboxXOffset, y + yOffset, checkboxSize, checkboxSize);

        // S1 button (column 2)
        s1Buttons[row]->setBounds(getColumnX(2) + checkboxXOffset, y + yOffset, checkboxSize, checkboxSize);

        // Priority matrix buttons (columns 3-8 = priorities 1-6)
        for (int col = 0; col < NUM_PRIORITIES; ++col) {
            int colIndex = col + 3;  // Columns 3-8
            matrixButtons[row][col]->setBounds(getColumnX(colIndex) + checkboxXOffset, y + yOffset, checkboxSize, checkboxSize);
        }

        // P2 slider (column 9) - horizontal slider with reduced padding
        p2Sliders[row]->setBounds(getColumnX(9) + 8, y + 2, p2ColumnWidth - 16, cellHeight - 4);

        // S2 button (column 10)
        s2Buttons[row]->setBounds(getColumnX(10) + checkboxXOffset, y + yOffset, checkboxSize, checkboxSize);

        // M2 button (column 11)
        m2Buttons[row]->setBounds(getColumnX(11) + checkboxXOffset, y + yOffset, checkboxSize, checkboxSize);
    }

    // Position "All" row label and checkboxes
    int allRowY = totalHeaderHeight + matrixBodyHeight;
    int allRowYOffset = (cellHeight - checkboxSize) / 2;
    int allRowCheckboxXOffset = (checkboxColumnWidth - checkboxSize) / 2;

    // All row label
    allRowLabel->setBounds(5, allRowY, rowLabelWidth - 10, cellHeight);

    // All row checkboxes - skip columns 0 (P1) and 9 (P2) - no "All" checkboxes for probability sliders
    // Hide the P1 and P2 column "All" buttons
    allRowButtons[0]->setVisible(false);  // P1
    allRowButtons[9]->setVisible(false);  // P2

    // Position checkboxes for columns 1-8 and 10-11 (M1, S1, priorities 1-6, S2, M2)
    for (int col = 1; col < NUM_COLUMNS; ++col) {
        if (col == 9) continue;  // Skip P2 column
        allRowButtons[col]->setBounds(getColumnX(col) + allRowCheckboxXOffset, allRowY + allRowYOffset, checkboxSize, checkboxSize);
    }

    // Position "100%" buttons for P1 and P2 columns in "All" row
    // P1 "100%" button (column 0) - centered in cell
    int p1ButtonWidth = 60;
    int p1ButtonHeight = 24;
    int p1ButtonX = getColumnX(0) + (p1ColumnWidth - p1ButtonWidth) / 2;
    int p1ButtonY = allRowY + (cellHeight - p1ButtonHeight) / 2;
    p1All100Button->setBounds(p1ButtonX, p1ButtonY, p1ButtonWidth, p1ButtonHeight);

    // P2 "100%" button (column 9) - centered in cell
    int p2ButtonWidth = 60;
    int p2ButtonHeight = 24;
    int p2ButtonX = getColumnX(9) + (p2ColumnWidth - p2ButtonWidth) / 2;
    int p2ButtonY = allRowY + (cellHeight - p2ButtonHeight) / 2;
    p2All100Button->setBounds(p2ButtonX, p2ButtonY, p2ButtonWidth, p2ButtonHeight);
}

void LinearDrummingMatrix::onMatrixButtonClicked(int instrument, int priority)
{
    // Each instrument must always have exactly one priority selected
    // Cannot uncheck - only move to a different priority

    // Do nothing if clicking the already selected priority
    if (currentAssignments[instrument] == priority) {
        return;
    }

    // Set new assignment
    currentAssignments[instrument] = priority;

    // Update visual state
    updateDisplayStates();

    // Update parameters
    updateParameterFromMatrix();

    // Update "All" row checkboxes
    updateAllRowCheckboxes();
}

void LinearDrummingMatrix::onAllRowButtonClicked(int column)
{
    // Determine if we should enable or disable based on column type
    bool shouldEnable = false;

    // Check the column to understand what we're working with
    // Columns 0 (P1) and 9 (P2) have no "All" buttons - skip them
    if (column == 1) {
        // M1 column - TOGGLE behavior: can disable all if all are enabled
        int enabledCount = 0;
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            if (m1Buttons[row]->getToggleState()) enabledCount++;
        }
        shouldEnable = (enabledCount < NUM_INSTRUMENTS);

        // CRITICAL: Toggle all off if all are enabled
        if (!shouldEnable && enabledCount == NUM_INSTRUMENTS) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                m1Buttons[row]->setToggleState(false, sendNotification);
            }
        }
        // Enable all M1 buttons if not all are enabled
        else if (shouldEnable) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                m1Buttons[row]->setToggleState(true, sendNotification);
            }
        }
    }
    else if (column == 2) {
        // S1 column - TOGGLE behavior: can disable all if all are enabled
        int enabledCount = 0;
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            if (s1Buttons[row]->getToggleState()) enabledCount++;
        }
        shouldEnable = (enabledCount < NUM_INSTRUMENTS);

        // CRITICAL: Toggle all off if all are enabled
        if (!shouldEnable && enabledCount == NUM_INSTRUMENTS) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                s1Buttons[row]->setToggleState(false, sendNotification);
            }
        }
        else if (shouldEnable) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                s1Buttons[row]->setToggleState(true, sendNotification);
            }
        }
    }
    else if (column >= 3 && column <= 8) {
        // Priority columns (1-6) - NO-OP behavior: do nothing if all enabled
        int priority = column - 3;  // Map column 3-8 to priority 0-5
        int enabledCount = 0;
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            if (currentAssignments[row] == priority) enabledCount++;
        }
        shouldEnable = (enabledCount < NUM_INSTRUMENTS);

        // If all are already enabled and user clicked, ignore (do nothing)
        if (!shouldEnable && enabledCount == NUM_INSTRUMENTS) {
            allRowButtons[column]->setToggleState(true, dontSendNotification);
            return;
        }

        // Enable all instruments to this priority if not all are enabled
        if (shouldEnable) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                currentAssignments[row] = priority;
            }
            updateDisplayStates();
            updateParameterFromMatrix();
        }
    }
    else if (column == 10) {
        // S2 column - TOGGLE behavior: can disable all if all are enabled
        int enabledCount = 0;
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            if (s2Buttons[row]->getToggleState()) enabledCount++;
        }
        shouldEnable = (enabledCount < NUM_INSTRUMENTS);

        // CRITICAL: Toggle all off if all are enabled
        if (!shouldEnable && enabledCount == NUM_INSTRUMENTS) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                s2Buttons[row]->setToggleState(false, sendNotification);
            }
        }
        else if (shouldEnable) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                s2Buttons[row]->setToggleState(true, sendNotification);
            }
        }
    }
    else if (column == 11) {
        // M2 column - TOGGLE behavior: can disable all if all are enabled
        int enabledCount = 0;
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            if (m2Buttons[row]->getToggleState()) enabledCount++;
        }
        shouldEnable = (enabledCount < NUM_INSTRUMENTS);

        // CRITICAL: Toggle all off if all are enabled
        if (!shouldEnable && enabledCount == NUM_INSTRUMENTS) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                m2Buttons[row]->setToggleState(false, sendNotification);
            }
        }
        else if (shouldEnable) {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
                m2Buttons[row]->setToggleState(true, sendNotification);
            }
        }
    }

    // Update "All" row checkboxes to reflect current state
    updateAllRowCheckboxes();
}


void LinearDrummingMatrix::updateDisplayStates()
{
    for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
        // Clear all buttons in this row
        for (int col = 0; col < NUM_PRIORITIES; ++col) {
            matrixButtons[row][col]->setToggleState(false, dontSendNotification);
        }

        // Set the appropriate priority button based on current assignment
        // Each instrument always has a priority (0-5)
        int assignedPriority = currentAssignments[row];
        if (assignedPriority >= 0 && assignedPriority < NUM_PRIORITIES) {
            matrixButtons[row][assignedPriority]->setToggleState(true, dontSendNotification);
        }
    }
}

void LinearDrummingMatrix::updateParameterFromMatrix()
{
    // CRITICAL: Set flag to prevent recursive parameterChanged() callbacks
    // When updating multiple parameters (e.g., "All" button click), we don't want
    // each parameter change to trigger updateFromParameters() which would overwrite
    // the assignments we're trying to set
    isUpdatingParameters = true;

    // NEW: Store each instrument's priority directly in its own parameter
    // This allows multiple instruments to have the same priority
    const std::array<String, 6> linearPriorityIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };

    for (int instrument = 0; instrument < NUM_INSTRUMENTS; ++instrument) {
        String paramName = linearPriorityIDs[instrument];

        // Use getParameter to properly update the parameter through APVTS
        if (auto* param = parameters.getParameter(paramName)) {
            // Convert priority index (0-5) to normalized value (0.0-1.0)
            float normalizedValue = currentAssignments[instrument] / 5.0f;
            param->setValueNotifyingHost(normalizedValue);
        }
    }

    // CRITICAL: Clear flag after all parameter updates complete
    isUpdatingParameters = false;

    // Remove thru mode parameters - they're no longer used
    // All instruments are always "linear" now (subject to priority logic)

    // CRITICAL FIX: Force Linear Drumming cache to update immediately
    audioProcessor.invalidateLinearDrummingCache();
}

void LinearDrummingMatrix::updateFromParameters()
{
    // Read each instrument's priority directly from its parameter
    const std::array<String, 6> linearPriorityIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };

    for (int instrument = 0; instrument < NUM_INSTRUMENTS; ++instrument) {
        String paramName = linearPriorityIDs[instrument];

        if (auto* param = parameters.getParameter(paramName)) {
            // Get normalized value (0.0-1.0) and convert to priority index (0-5)
            float normalizedValue = param->getValue();
            int priority = static_cast<int>(normalizedValue * 5.0f + 0.5f); // Round to nearest

            // Clamp to valid range
            if (priority < 0) priority = 0;
            if (priority >= NUM_PRIORITIES) priority = NUM_PRIORITIES - 1;
            currentAssignments[instrument] = priority;
        } else {
            // Default to Priority 1 if parameter not found
            currentAssignments[instrument] = 0;
        }
    }

    // Update visual state
    updateDisplayStates();

    // Update "All" row checkboxes to match current state (important for preset loading)
    updateAllRowCheckboxes();
}

std::array<int, 6> LinearDrummingMatrix::getCurrentAssignments() const
{
    return currentAssignments;
}

void LinearDrummingMatrix::updateAllRowCheckboxes()
{
    // Guard: Don't update if allRowButtons aren't initialized yet (prevents crash during construction)
    if (!allRowButtons[1] || !allRowButtons[2] || !allRowButtons[10] || !allRowButtons[11])
        return;

    // Update each column's "All" checkbox based on whether all instruments in that column are enabled
    // Columns 0 (P1) and 9 (P2) have no "All" checkboxes - skip them

    // M1 column (column 1)
    int m1EnabledCount = 0;
    for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
        if (m1Buttons[row] && m1Buttons[row]->getToggleState()) m1EnabledCount++;
    }
    allRowButtons[1]->setToggleState(m1EnabledCount == NUM_INSTRUMENTS, dontSendNotification);

    // S1 column (column 2)
    int s1EnabledCount = 0;
    for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
        if (s1Buttons[row] && s1Buttons[row]->getToggleState()) s1EnabledCount++;
    }
    allRowButtons[2]->setToggleState(s1EnabledCount == NUM_INSTRUMENTS, dontSendNotification);

    // Priority columns (3-8 = P1-P6)
    for (int col = 3; col <= 8; ++col) {
        if (!allRowButtons[col]) continue;
        int priority = col - 3;  // Map column 3-8 to priority 0-5
        int enabledCount = 0;
        for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
            if (currentAssignments[row] == priority) enabledCount++;
        }
        allRowButtons[col]->setToggleState(enabledCount == NUM_INSTRUMENTS, dontSendNotification);
    }

    // S2 column (column 10)
    int s2EnabledCount = 0;
    for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
        if (s2Buttons[row] && s2Buttons[row]->getToggleState()) s2EnabledCount++;
    }
    allRowButtons[10]->setToggleState(s2EnabledCount == NUM_INSTRUMENTS, dontSendNotification);

    // M2 column (column 11)
    int m2EnabledCount = 0;
    for (int row = 0; row < NUM_INSTRUMENTS; ++row) {
        if (m2Buttons[row] && m2Buttons[row]->getToggleState()) m2EnabledCount++;
    }
    allRowButtons[11]->setToggleState(m2EnabledCount == NUM_INSTRUMENTS, dontSendNotification);
}

void LinearDrummingMatrix::instrumentNamesChanged()
{
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<LinearDrummingMatrix>(this)]() {
        if (safeThis == nullptr) return;
        if (auto* mgr = safeThis->audioProcessor.getMidiMappingManager())
        {
            for (int row = 0; row < NUM_INSTRUMENTS; ++row)
                safeThis->rowLabels[row]->setText(mgr->getInstrumentNameLong(row), dontSendNotification);
        }
    });
}