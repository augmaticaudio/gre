#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "LEDIndicator.h"
#include "DSP/ClockDivider.h"
#include "DSP/Constants.h"
#include "UI/PresetPanel.h"
#include "UI/MidiMappingPanel.h"
#include "UI/XYPadAnimation.h"

using namespace juce;

// Custom LookAndFeel for ComboBox dropdown with correct background color
class CustomComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(juce::Colour(0xff2d2d2d)); // Force the #2d2d2d background color
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                          bool isSeparator, bool isActive, bool isHighlighted,
                          bool isTicked, bool hasSubMenu, const juce::String& text,
                          const juce::String& shortcutKeyText,
                          const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        if (isHighlighted)
            g.fillAll(juce::Colour(0xff404040)); // Highlighted item background
        else
            g.fillAll(juce::Colour(0xff2d2d2d)); // Normal item background

        // Draw text with proper color
        g.setColour(juce::Colours::white);
        g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft);
    }

    int getPopupMenuBorderSize() override
    {
        return 1;
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                   int standardMenuItemHeight, int& idealWidth, int& idealHeight) override
    {
        // Set fixed width to 60px for Shift and Clock menus
        idealWidth = 60;
        idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight : 20;
    }
};

// Custom LookAndFeel for Pattern Tab rotary knobs - thin arc with needle indicator
class PatternKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Get colors from slider
        auto fillColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        auto outlineColour = slider.findColour(juce::Slider::rotarySliderOutlineColourId);

        // Arc thickness - thin (50% of default)
        float arcThickness = 2.0f;

        // Draw full circle outline in grid background color (covers bottom gap too)
        g.setColour(juce::Colour(0xff101010));  // Match grid background
        g.drawEllipse(centreX - radius + arcThickness / 2.0f, centreY - radius + arcThickness / 2.0f,
                      (radius - arcThickness / 2.0f) * 2.0f, (radius - arcThickness / 2.0f) * 2.0f, arcThickness);

        // Draw knob interior circle (#313135)
        g.setColour(juce::Colour(0xff313135));
        g.fillEllipse(centreX - radius + arcThickness, centreY - radius + arcThickness,
                      (radius - arcThickness) * 2.0f, (radius - arcThickness) * 2.0f);

        // Draw value arc (active area) - thin colored
        if (sliderPosProportional > 0.0f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centreX, centreY, radius - arcThickness, radius - arcThickness,
                                    0.0f, rotaryStartAngle, angle, true);
            g.setColour(fillColour);
            g.strokePath(valueArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draw needle from center to edge (longer to close gap with arc)
        float needleThickness = 1.5f;
        juce::Path needle;
        auto needleLength = radius - arcThickness;  // Extend needle to meet the arc
        needle.addLineSegment(juce::Line<float>(centreX, centreY,
                                                 centreX + needleLength * std::sin(angle),
                                                 centreY - needleLength * std::cos(angle)), needleThickness);
        g.setColour(fillColour);
        g.strokePath(needle, juce::PathStrokeType(needleThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw small center dot (same diameter as needle thickness)
        float dotRadius = needleThickness / 2.0f;
        g.fillEllipse(centreX - dotRadius, centreY - dotRadius, needleThickness, needleThickness);
    }
};

// Custom LookAndFeel for bipolar knobs - value arc draws from center (12 o'clock)
class BipolarKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Center angle (12 o'clock position = 0.5 of range)
        auto centerAngle = rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle);

        // Get colors from slider
        auto fillColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        auto outlineColour = slider.findColour(juce::Slider::rotarySliderOutlineColourId);

        float arcThickness = 2.0f;

        // Draw full circle outline in grid background color (covers bottom gap too)
        g.setColour(juce::Colour(0xff101010));  // Match grid background
        g.drawEllipse(centreX - radius + arcThickness / 2.0f, centreY - radius + arcThickness / 2.0f,
                      (radius - arcThickness / 2.0f) * 2.0f, (radius - arcThickness / 2.0f) * 2.0f, arcThickness);

        // Draw knob interior circle (#313135)
        g.setColour(juce::Colour(0xff313135));
        g.fillEllipse(centreX - radius + arcThickness, centreY - radius + arcThickness,
                      (radius - arcThickness) * 2.0f, (radius - arcThickness) * 2.0f);

        // Draw value arc from center to current position - colored
        if (std::abs(sliderPosProportional - 0.5f) > 0.001f)
        {
            juce::Path valueArc;
            if (sliderPosProportional > 0.5f)
            {
                // Value is above center - draw arc from center to current
                valueArc.addCentredArc(centreX, centreY, radius - arcThickness, radius - arcThickness,
                                        0.0f, centerAngle, angle, true);
            }
            else
            {
                // Value is below center - draw arc from current to center
                valueArc.addCentredArc(centreX, centreY, radius - arcThickness, radius - arcThickness,
                                        0.0f, angle, centerAngle, true);
            }
            g.setColour(fillColour);
            g.strokePath(valueArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draw needle from center to edge (longer to close gap with arc)
        float needleThickness = 1.5f;
        juce::Path needle;
        auto needleLength = radius - arcThickness;  // Extend needle to meet the arc
        needle.addLineSegment(juce::Line<float>(centreX, centreY,
                                                 centreX + needleLength * std::sin(angle),
                                                 centreY - needleLength * std::cos(angle)), needleThickness);
        g.setColour(fillColour);
        g.strokePath(needle, juce::PathStrokeType(needleThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw small center dot (same diameter as needle thickness)
        float dotRadius = needleThickness / 2.0f;
        g.fillEllipse(centreX - dotRadius, centreY - dotRadius, needleThickness, needleThickness);
    }
};

// Custom LookAndFeel for tab buttons - matches grid background, equal width, bottom line for selected
class TabLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TabLookAndFeel() = default;

    int getTabButtonBestWidth(juce::TabBarButton& button, int /*tabDepth*/) override
    {
        if (totalBarWidth <= 0) return 100;  // Fallback

        // Settings tab (index 3, formerly MIDI) matches LED column width
        int tabIndex = button.getIndex();
        if (tabIndex == 3)
            return ledColumnWidth;

        // Remaining 3 tabs (PATTERN, LINEAR, VELOCITY) divide remaining space equally
        int remainingWidth = totalBarWidth - ledColumnWidth;
        return remainingWidth / 3;
    }

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
                       bool isMouseOver, bool /*isMouseDown*/) override
    {
        auto bounds = button.getLocalBounds();
        bool isFrontTab = button.isFrontTab();
        int tabIndex = button.getIndex();
        bool isSettingsTab = (tabIndex == 3);  // Settings tab (formerly MIDI)

        // Background - same as grid background
        g.setColour(juce::Colour(0xff101010));
        g.fillRect(bounds);

        // Selected tab indicator - wider for text tabs, matches width for icon tab
        if (isFrontTab)
        {
            g.setColour(juce::Colour(0xff595e5f));
            int lineThickness = 3;
            int gapBelow = 10;

            // For text tabs (PATTERN, VELOCITY, MIX): widen indicator by reducing left/right margins
            // For Settings tab: keep indicator width same as tab width
            int indicatorMargin = isSettingsTab ? 0 : static_cast<int>(bounds.getWidth() * 0.05f);
            g.fillRect(bounds.getX() + indicatorMargin, bounds.getBottom() - lineThickness - gapBelow,
                      bounds.getWidth() - (indicatorMargin * 2), lineThickness);
        }

        // Top padding 10px, bottom padding accounts for indicator (3px) + gap (10px)
        auto contentBounds = bounds.withTrimmedTop(10).withTrimmedBottom(23);

        // Set color - white for selected, gray for inactive
        if (isFrontTab)
            g.setColour(juce::Colours::white);
        else if (isMouseOver)
            g.setColour(juce::Colour(0xffcccccc));
        else
            g.setColour(juce::Colour(0xffb0b0b0));  // Same gray as header labels

        // Draw hamburger icon for Settings tab, text for others
        if (isSettingsTab)
        {
            // Draw hamburger menu icon (3 horizontal lines)
            float iconSize = currentFont.getHeight();  // Match text size
            float lineHeight = iconSize * 0.12f;  // Line thickness
            float lineWidth = iconSize * 0.8f;  // Line width
            float spacing = iconSize * 0.25f;  // Space between lines

            float centerX = contentBounds.getCentreX();
            float centerY = contentBounds.getCentreY();
            float startY = centerY - spacing;
            float startX = centerX - (lineWidth / 2.0f);

            // Draw 3 horizontal lines
            for (int i = 0; i < 3; ++i)
            {
                float y = startY + (i * spacing);
                g.fillRoundedRectangle(startX, y, lineWidth, lineHeight, lineHeight / 2.0f);
            }
        }
        else
        {
            // Draw text for PATTERN, VELOCITY, MIX tabs
            g.setFont(currentFont);
            g.drawText(button.getButtonText(), contentBounds, juce::Justification::centred);
        }
    }

    // Draw tab button text with proper colors
    void drawTabButtonText(juce::TabBarButton& button, juce::Graphics& g,
                           bool isMouseOver, bool /*isMouseDown*/) override
    {
        // Text/icon drawing is handled in drawTabButton(), not here
        // This method is called by JUCE but we override it to do nothing
        // since we already draw everything in drawTabButton()
        (void)button;
        (void)g;
        (void)isMouseOver;
    }

    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar& /*bar*/, juce::Graphics& /*g*/, int /*w*/, int /*h*/) override
    {
        // DO NOTHING - this is called AFTER buttons are painted (in paintOverChildren)
        // Filling here would paint OVER the inactive tab buttons!
        // Background is already handled by drawTabbedButtonBarBackground and drawTabButton
    }

    void drawTabbedButtonBarBackground(juce::TabbedButtonBar& /*bar*/, juce::Graphics& g) override
    {
        // Fill with grid background color
        g.setColour(juce::Colour(0xff101010));
        g.fillAll();
    }

    juce::Rectangle<int> getTabButtonExtraComponentBounds(const juce::TabBarButton& /*button*/,
                                                          juce::Rectangle<int>& /*textArea*/,
                                                          juce::Component& /*extraComp*/) override
    {
        return {};
    }

    void setFont(const juce::Font& font)
    {
        currentFont = font;
    }

    void setTabBarDimensions(int width, int tabs, int ledWidth)
    {
        totalBarWidth = width;
        numTabs = tabs;
        ledColumnWidth = ledWidth;
    }

private:
    juce::Font currentFont { 18.0f, juce::Font::plain };
    int totalBarWidth = 0;
    int numTabs = 4;  // PATTERN, VELOCITY, MIX, Settings
    int ledColumnWidth = 0;  // Width of LED column (Settings tab matches this)
};

// Scrollable number display - shows value as text, drag up/down to change
class ScrollableNumberDisplay : public juce::Component
{
public:
    ScrollableNumberDisplay()
    {
        // Ensure we receive mouse events
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        // Internal slider for value management (not visible)
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled(false);
        slider.setInterceptsMouseClicks(false, false);

        // Listen to slider value changes to repaint
        slider.onValueChange = [this]() { repaint(); };
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Draw the number value centered
        g.setColour(textColour);
        g.setFont(currentFont);
        g.drawText(juce::String(static_cast<int>(slider.getValue())),
                   bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        // Store starting position and value for drag
        dragStartY = event.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        // Calculate value change based on vertical drag (up = increase, down = decrease)
        float dragDistance = static_cast<float>(dragStartY - event.y);
        float sensitivity = 0.1f;  // Pixels per unit change (halved for more precise control)
        double valueDelta = dragDistance * sensitivity;
        double newValue = dragStartValue + valueDelta;

        // Clamp and set value
        newValue = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newValue);
        slider.setValue(std::round(newValue), juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel) override
    {
        // Scroll wheel also works - up increases, down decreases
        double delta = wheel.deltaY > 0 ? 1.0 : (wheel.deltaY < 0 ? -1.0 : 0.0);
        if (delta != 0.0)
        {
            double newValue = slider.getValue() + delta;
            slider.setValue(newValue, juce::sendNotificationSync);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& /*event*/) override
    {
        // Double-click returns to default value
        slider.setValue(defaultValue, juce::sendNotificationSync);
    }

    void setRange(double min, double max, double interval)
    {
        slider.setRange(min, max, interval);
    }

    void setDefaultValue(double value)
    {
        defaultValue = value;
    }

    void setTextColour(juce::Colour colour)
    {
        textColour = colour;
        repaint();
    }

    void setFont(const juce::Font& font)
    {
        currentFont = font;
        repaint();
    }

    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider;
    juce::Colour textColour = juce::Colours::white;
    juce::Font currentFont { 28.0f };
    int dragStartY = 0;
    double dragStartValue = 0.0;
    double defaultValue = 16.0;  // Default value for double-click reset

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableNumberDisplay)
};

// Scrollable Note Name Display - displays note names (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
class ScrollableNoteNameDisplay : public juce::Component
{
public:
    ScrollableNoteNameDisplay()
    {
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled(false);
        slider.setInterceptsMouseClicks(false, false);
        slider.setRange(0.0, 11.0, 1.0);  // 12 notes (C=0 to B=11)

        slider.onValueChange = [this]() {
            repaint();
            if (onValueChanged)
                onValueChanged(static_cast<int>(slider.getValue()));
        };
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        int noteIndex = static_cast<int>(slider.getValue());
        juce::String noteName = getNoteName(noteIndex);

        g.setColour(textColour);
        g.setFont(currentFont);
        g.drawText(noteName, bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartY = event.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        float dragDistance = static_cast<float>(dragStartY - event.y);
        float sensitivity = 0.075f;  // Halved for more precise control
        double valueDelta = dragDistance * sensitivity;
        double newValue = dragStartValue + valueDelta;
        newValue = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newValue);
        slider.setValue(std::round(newValue), juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel) override
    {
        double delta = wheel.deltaY > 0 ? 1.0 : (wheel.deltaY < 0 ? -1.0 : 0.0);
        if (delta != 0.0)
            slider.setValue(slider.getValue() + delta, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent& /*event*/) override
    {
        slider.setValue(defaultValue, juce::sendNotificationSync);
    }

    void setTextColour(juce::Colour colour)
    {
        textColour = colour;
        repaint();
    }

    void setFont(const juce::Font& font)
    {
        currentFont = font;
        repaint();
    }

    void setValue(int noteIndex, juce::NotificationType notification = juce::sendNotificationSync)
    {
        slider.setValue(static_cast<double>(noteIndex), notification);
    }

    int getValue() const
    {
        return static_cast<int>(slider.getValue());
    }

    void setDefaultValue(double value) { defaultValue = value; }

    // Callback when value changes
    std::function<void(int)> onValueChanged;

    static juce::String getNoteName(int noteIndex)
    {
        static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        if (noteIndex >= 0 && noteIndex < 12)
            return juce::String(noteNames[noteIndex]);
        return "?";
    }

private:
    juce::Slider slider;
    juce::Colour textColour = juce::Colours::white;
    juce::Font currentFont { 28.0f };
    int dragStartY = 0;
    double dragStartValue = 0.0;
    double defaultValue = 0.0;  // C

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableNoteNameDisplay)
};

// Scrollable Octave Display - displays octave numbers (-2 to 7)
class ScrollableOctaveDisplay : public juce::Component
{
public:
    ScrollableOctaveDisplay()
    {
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled(false);
        slider.setInterceptsMouseClicks(false, false);
        slider.setRange(-2.0, 7.0, 1.0);  // Octaves -2 to 7

        slider.onValueChange = [this]() {
            repaint();
            if (onValueChanged)
                onValueChanged(static_cast<int>(slider.getValue()));
        };
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        int octave = static_cast<int>(slider.getValue());

        g.setColour(textColour);
        g.setFont(currentFont);
        g.drawText(juce::String(octave), bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartY = event.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        float dragDistance = static_cast<float>(dragStartY - event.y);
        float sensitivity = 0.075f;  // Halved for more precise control
        double valueDelta = dragDistance * sensitivity;
        double newValue = dragStartValue + valueDelta;
        newValue = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newValue);
        slider.setValue(std::round(newValue), juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel) override
    {
        double delta = wheel.deltaY > 0 ? 1.0 : (wheel.deltaY < 0 ? -1.0 : 0.0);
        if (delta != 0.0)
            slider.setValue(slider.getValue() + delta, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent& /*event*/) override
    {
        slider.setValue(defaultValue, juce::sendNotificationSync);
    }

    void setTextColour(juce::Colour colour)
    {
        textColour = colour;
        repaint();
    }

    void setFont(const juce::Font& font)
    {
        currentFont = font;
        repaint();
    }

    void setValue(int octave, juce::NotificationType notification = juce::sendNotificationSync)
    {
        slider.setValue(static_cast<double>(octave), notification);
    }

    int getValue() const
    {
        return static_cast<int>(slider.getValue());
    }

    void setDefaultValue(double value) { defaultValue = value; }

    // Callback when value changes
    std::function<void(int)> onValueChanged;

private:
    juce::Slider slider;
    juce::Colour textColour = juce::Colours::white;
    juce::Font currentFont { 28.0f };
    int dragStartY = 0;
    double dragStartValue = 0.0;
    double defaultValue = 1.0;  // Octave 1 (default for drums)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableOctaveDisplay)
};

// ScrollableLabelDisplay - displays configurable text labels for integer parameter values
// Used for Note Duration and Linear Grid controls on the MIDI tab (drag/scroll to change)
class ScrollableLabelDisplay : public juce::Component
{
public:
    ScrollableLabelDisplay()
    {
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled(false);
        slider.setInterceptsMouseClicks(false, false);
        slider.setRange(0.0, 4.0, 1.0);  // Default range for AudioParameterChoice (0-indexed)

        slider.onValueChange = [this]() { repaint(); };
    }

    // Configure the value-to-label mapping
    // e.g., {{1,"4n"}, {2,"8n"}, {3,"16n"}, {4,"32n"}, {5,"64n"}}
    void setLabels(std::vector<std::pair<int, juce::String>> labelMap)
    {
        labels = std::move(labelMap);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        int val = static_cast<int>(slider.getValue());
        juce::String text = "?";
        for (const auto& pair : labels)
        {
            if (pair.first == val)
            {
                text = pair.second;
                break;
            }
        }
        g.setColour(textColour);
        g.setFont(currentFont);
        g.drawText(text, getLocalBounds().toFloat(), juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartY = event.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        float dragDistance = static_cast<float>(dragStartY - event.y);
        float sensitivity = 0.15f;
        double valueDelta = dragDistance * sensitivity;
        double newValue = dragStartValue + valueDelta;
        newValue = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newValue);
        slider.setValue(std::round(newValue), juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel) override
    {
        double delta = wheel.deltaY > 0 ? 1.0 : (wheel.deltaY < 0 ? -1.0 : 0.0);
        if (delta != 0.0)
            slider.setValue(slider.getValue() + delta, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent& /*event*/) override
    {
        slider.setValue(defaultValue, juce::sendNotificationSync);
    }

    void setTextColour(juce::Colour colour) { textColour = colour; repaint(); }
    void setFont(const juce::Font& font) { currentFont = font; repaint(); }
    void setDefaultValue(double value) { defaultValue = value; }
    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider;
    std::vector<std::pair<int, juce::String>> labels;
    juce::Colour textColour = juce::Colours::white;
    juce::Font currentFont { 28.0f };
    int dragStartY = 0;
    double dragStartValue = 0.0;
    double defaultValue = 2.0;  // 16n default (0-indexed)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableLabelDisplay)
};

// ScrollableBPMDisplay - displays BPM values (40-240) for standalone internal clock
class ScrollableBPMDisplay : public juce::Component
{
public:
    ScrollableBPMDisplay()
    {
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled(false);
        slider.setInterceptsMouseClicks(false, false);
        slider.setRange(40.0, 240.0, 1.0);  // BPM range 40-240

        slider.onValueChange = [this]() {
            repaint();
            if (onValueChanged)
                onValueChanged(static_cast<int>(slider.getValue()));
        };
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        int bpm = static_cast<int>(slider.getValue());

        g.setColour(textColour);
        g.setFont(currentFont);
        g.drawText(juce::String(bpm), bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartY = event.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        float dragDistance = static_cast<float>(dragStartY - event.y);
        float sensitivity = 0.1f;  // 10 pixels per BPM step (matches other scrollable displays)
        double valueDelta = dragDistance * sensitivity;
        double newValue = dragStartValue + valueDelta;
        newValue = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newValue);
        slider.setValue(std::round(newValue), juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel) override
    {
        double delta = wheel.deltaY > 0 ? 1.0 : (wheel.deltaY < 0 ? -1.0 : 0.0);
        if (delta != 0.0)
            slider.setValue(slider.getValue() + delta, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent& /*event*/) override
    {
        slider.setValue(defaultValue, juce::sendNotificationSync);
    }

    void setTextColour(juce::Colour colour) { textColour = colour; repaint(); }
    void setFont(const juce::Font& font) { currentFont = font; repaint(); }

    void setValue(int bpm, juce::NotificationType notification = juce::sendNotificationSync)
    {
        slider.setValue(static_cast<double>(bpm), notification);
    }

    int getValue() const { return static_cast<int>(slider.getValue()); }
    void setDefaultValue(double value) { defaultValue = value; }

    std::function<void(int)> onValueChanged;

private:
    juce::Slider slider;
    juce::Colour textColour = juce::Colours::white;
    juce::Font currentFont { 28.0f };
    int dragStartY = 0;
    double dragStartValue = 0.0;
    double defaultValue = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableBPMDisplay)
};

// ScrollableChannelDisplay - displays MIDI channel numbers (1-16)
class ScrollableChannelDisplay : public juce::Component
{
public:
    ScrollableChannelDisplay()
    {
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled(false);
        slider.setInterceptsMouseClicks(false, false);
        slider.setRange(1.0, 16.0, 1.0);  // MIDI channels 1-16

        slider.onValueChange = [this]() {
            repaint();
            if (onValueChanged)
                onValueChanged(static_cast<int>(slider.getValue()));
        };
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        int channel = static_cast<int>(slider.getValue());

        g.setColour(textColour);
        g.setFont(currentFont);
        g.drawText(juce::String(channel), bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartY = event.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        float dragDistance = static_cast<float>(dragStartY - event.y);
        float sensitivity = 0.1f;  // 10 pixels per channel step
        double valueDelta = dragDistance * sensitivity;
        double newValue = dragStartValue + valueDelta;
        newValue = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newValue);
        slider.setValue(std::round(newValue), juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel) override
    {
        double delta = wheel.deltaY > 0 ? 1.0 : (wheel.deltaY < 0 ? -1.0 : 0.0);
        if (delta != 0.0)
            slider.setValue(slider.getValue() + delta, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent& /*event*/) override
    {
        slider.setValue(defaultValue, juce::sendNotificationSync);
    }

    void setTextColour(juce::Colour colour) { textColour = colour; repaint(); }
    void setFont(const juce::Font& font) { currentFont = font; repaint(); }

    void setValue(int channel, juce::NotificationType notification = juce::sendNotificationSync)
    {
        slider.setValue(static_cast<double>(channel), notification);
    }

    int getValue() const { return static_cast<int>(slider.getValue()); }
    void setDefaultValue(double value) { defaultValue = value; }

    std::function<void(int)> onValueChanged;

private:
    juce::Slider slider;
    juce::Colour textColour = juce::Colours::white;
    juce::Font currentFont { 28.0f };
    int dragStartY = 0;
    double dragStartValue = 0.0;
    double defaultValue = 10.0;  // Channel 10 (GM Drums)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableChannelDisplay)
};

// MidiOutputDropdown — custom dropdown matching MappingDropdownMenu style
// Rendered as a child component (not a JUCE PopupMenu window) for full visual control
class MidiOutputDropdown : public juce::Component
{
public:
    static constexpr int kRowHeight = 25;

    struct Row { juce::String name; bool isSelected = false; };
    std::vector<Row> rows;
    int hoveredRow = -1;
    std::function<void(int)> onRowClicked;
    std::function<void()> onDismiss;

    void show(juce::Component* parent, juce::Rectangle<int> anchorBounds)
    {
        if (parent == nullptr) return;
        int w = anchorBounds.getWidth();
        int h = static_cast<int>(rows.size()) * kRowHeight + 4;
        int maxH = parent->getHeight() - anchorBounds.getBottom() - 5;
        if (h > maxH) h = maxH;
        setBounds(anchorBounds.getX(), anchorBounds.getBottom() - 2, w, h);
        parent->addAndMakeVisible(this);
        enterModalState(false, nullptr, false);
        toFront(true);
    }

    void dismiss()
    {
        exitModalState(0);
        setVisible(false);
        if (auto* p = getParentComponent())
            p->removeChildComponent(this);
        if (onDismiss)
        {
            auto cb = onDismiss;
            juce::MessageManager::callAsync(cb);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour(juce::Colour(0xff101010));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff595e5f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 6.0f, 2.0f);

        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        {
            auto rowBounds = juce::Rectangle<int>(2, i * kRowHeight + 2, bounds.getWidth() - 4, kRowHeight);
            if (rows[i].isSelected)
                g.setColour(juce::Colour(0xff505050));
            else if (i == hoveredRow)
                g.setColour(juce::Colour(0xff404040));
            else
                g.setColour(juce::Colour(0xff101010));
            g.fillRect(rowBounds);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(14.0f));
            g.drawText(rows[i].name, rowBounds.reduced(12, 0), juce::Justification::centredLeft);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        int idx = (e.getPosition().getY() - 2) / kRowHeight;
        if (idx >= 0 && idx < static_cast<int>(rows.size()))
        {
            if (onRowClicked) onRowClicked(idx);
        }
        dismiss();
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        int idx = (e.getPosition().getY() - 2) / kRowHeight;
        if (idx < 0 || idx >= static_cast<int>(rows.size())) idx = -1;
        if (idx != hoveredRow) { hoveredRow = idx; repaint(); }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredRow != -1) { hoveredRow = -1; repaint(); }
    }

    void inputAttemptWhenModal() override { dismiss(); }
};

// MidiOutputPanel - displays MIDI output device selector
// Uses custom dropdown (MidiOutputDropdown) instead of JUCE PopupMenu for visual consistency
class MidiOutputPanel : public juce::Component
{
public:
    MidiOutputPanel(AugmaticGREProcessor& proc) : audioProcessor(proc)
    {
        setInterceptsMouseClicks(true, false);
        updateDisplay();
    }

    ~MidiOutputPanel() override = default;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background — dark gray matching knob interior (same as MappingPanel)
        g.setColour(juce::Colour(0xff101010));
        g.fillRoundedRectangle(bounds, 6.0f);

        // Border — thin gray outline (same as MappingPanel)
        g.setColour(juce::Colour(0xff595e5f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);

        // Draw device name centered (white text, same as MappingPanel)
        g.setColour(juce::Colours::white);
        g.setFont(currentFont);
        g.drawText(currentDeviceName, bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        showDeviceMenu();
    }

    void updateDisplay()
    {
        currentDeviceName = audioProcessor.getCurrentMidiOutputDevice();
        repaint();
    }

    void setFont(const juce::Font& font)
    {
        currentFont = font;
        repaint();
    }

private:
    void selectDeviceByMenuIndex(int menuIndex)
    {
        int deviceIndex = menuIndex == 0 ? -1 : menuIndex - 1;
        audioProcessor.setMidiOutputDevice(deviceIndex);
        updateDisplay();
    }

    void showDeviceMenu()
    {
        if (dropdown != nullptr) { dropdown->dismiss(); return; }

        auto devices = audioProcessor.getAvailableMidiOutputDevices();
        int currentIndex = audioProcessor.getCurrentMidiOutputDeviceIndex();
        int menuIndex = (currentIndex == -1) ? 0 : currentIndex + 1;

        dropdown = std::make_unique<MidiOutputDropdown>();
        for (int i = 0; i < devices.size(); ++i)
            dropdown->rows.push_back({ devices[i], i == menuIndex });

        dropdown->onRowClicked = [this](int idx) { selectDeviceByMenuIndex(idx); };
        dropdown->onDismiss = [this]() { dropdown.reset(); };

        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr) { dropdown.reset(); return; }
        auto boundsInTopLevel = topLevel->getLocalArea(this, getLocalBounds());
        dropdown->show(topLevel, boundsInTopLevel);
    }

    AugmaticGREProcessor& audioProcessor;
    juce::String currentDeviceName = "Plugin MIDI Output (Default)";
    juce::Font currentFont { 14.0f };
    std::unique_ptr<MidiOutputDropdown> dropdown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiOutputPanel)
};

// Scrollable Clock Ratio Display - displays clock division/multiplication values (/8 to x8)
class ScrollableClockDisplay : public juce::Component
{
public:
    ScrollableClockDisplay()
    {
        // Ensure we receive mouse events
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        // Internal slider for value management (not visible)
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled(false);
        slider.setInterceptsMouseClicks(false, false);
        slider.setRange(0.0, 17.0, 1.0);  // 18 clock ratio values (indices 0-17)

        // Listen to slider value changes to repaint
        slider.onValueChange = [this]() { repaint(); };
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Get the clock ratio string for the current value
        int ratioIndex = static_cast<int>(slider.getValue());
        juce::String ratioString = getRatioString(ratioIndex);

        // Draw the ratio string centered
        g.setColour(textColour);
        g.setFont(currentFont);
        g.drawText(ratioString, bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        // Store starting position and value for drag
        dragStartY = event.y;
        dragStartValue = slider.getValue();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        // Calculate value change based on vertical drag (up = increase, down = decrease)
        float dragDistance = static_cast<float>(dragStartY - event.y);
        float sensitivity = 0.15f;  // Pixels per unit change (slightly less sensitive for 18 values)
        double valueDelta = dragDistance * sensitivity;
        double newValue = dragStartValue + valueDelta;

        // Clamp and set value
        newValue = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newValue);
        slider.setValue(std::round(newValue), juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel) override
    {
        // Scroll wheel - up increases (faster), down decreases (slower)
        double delta = wheel.deltaY > 0 ? 1.0 : (wheel.deltaY < 0 ? -1.0 : 0.0);
        if (delta != 0.0)
        {
            double newValue = slider.getValue() + delta;
            slider.setValue(newValue, juce::sendNotificationSync);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& /*event*/) override
    {
        // Double-click returns to default (x1 = index 8)
        slider.setValue(8.0, juce::sendNotificationSync);
    }

    void setTextColour(juce::Colour colour)
    {
        textColour = colour;
        repaint();
    }

    void setFont(const juce::Font& font)
    {
        currentFont = font;
        repaint();
    }

    juce::Slider& getSlider() { return slider; }

private:
    // Get ratio string for a given index (matches ClockDivider logic)
    juce::String getRatioString(int index) const
    {
        // Ratio values matching ClockDivider
        static constexpr float ratioValues[18] = {
            8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.5f,  // Division (0-7)
            1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f  // Multiplication (8-17)
        };
        static constexpr int multiplicationStartIndex = 8;

        if (index < 0 || index >= 18) return "-";  // Default to x1 (displayed as dash)

        float ratio = ratioValues[index];
        bool isDiv = index < multiplicationStartIndex;

        if (isDiv) {
            if (ratio == 1.5f) return "/1.5";
            return "/" + juce::String(static_cast<int>(ratio));
        } else {
            if (ratio == 1.0f) return "-";  // x1 displayed as dash (no change)
            if (ratio == 1.5f) return "x1.5";
            if (ratio == 2.5f) return "x2.5";
            return "x" + juce::String(static_cast<int>(ratio));
        }
    }

    juce::Slider slider;
    juce::Colour textColour = juce::Colours::white;
    juce::Font currentFont { 28.0f };
    int dragStartY = 0;
    double dragStartValue = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableClockDisplay)
};

// Custom LED for Pattern Tab - thin circle outline, transparent when off, filled when lit
// v0.4.087: Now clickable - triggers MIDI note on click
class PatternLED : public juce::Component, private juce::Timer
{
public:
    PatternLED()
    {
        setInterceptsMouseClicks(true, false);
    }
    ~PatternLED() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();

        // Line thickness matching knobs (2px)
        float lineThickness = 2.0f;

        if (isLit)
        {
            // When lit: fill with color
            g.setColour(ledColour);
            g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
        }
        else
        {
            // When off: just draw thin circle outline
            g.setColour(ledColour);
            g.drawEllipse(centreX - radius + lineThickness / 2.0f,
                         centreY - radius + lineThickness / 2.0f,
                         (radius - lineThickness / 2.0f) * 2.0f,
                         (radius - lineThickness / 2.0f) * 2.0f,
                         lineThickness);
        }
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        trigger();  // Immediate visual feedback (v0.4.123)
        if (onClicked)
            onClicked();
    }

    void trigger()
    {
        bool needsRepaint = !isLit;
        isLit = true;
        if (isTimerRunning())
            stopTimer();
        startTimer(50);
        if (needsRepaint)
            repaint();
    }

    void timerCallback() override
    {
        if (isLit)
        {
            isLit = false;
            repaint();
        }
        stopTimer();
    }

    void setLEDColour(juce::Colour colour)
    {
        ledColour = colour;
        repaint();
    }

    // Callback for click events
    std::function<void()> onClicked;

private:
    bool isLit = false;
    juce::Colour ledColour = juce::Colours::green;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternLED)
};

// XY Pad Component - square pad for controlling Map X and Map Y parameters
// Purple dot (same size as LED dots) can be dragged within the pad
// Uses ParameterAttachment for direct APVTS parameter binding
// Includes animated ripple visualization (XYPadAnimation) as child overlay
class XYPadComponent : public juce::Component
{
public:
    XYPadComponent(juce::RangedAudioParameter& xParam, juce::RangedAudioParameter& yParam,
                   juce::AudioProcessorValueTreeState& apvts)
    {
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::CrosshairCursor);

        xAttachment = std::make_unique<juce::ParameterAttachment>(xParam, [this](float val) {
            xValue = val;
            repaint();
        });
        yAttachment = std::make_unique<juce::ParameterAttachment>(yParam, [this](float val) {
            yValue = val;
            repaint();
        });

        xAttachment->sendInitialUpdate();
        yAttachment->sendInitialUpdate();

        // Animation overlay — transparent, passes clicks through
        animation = std::make_unique<XYPadAnimation>(apvts);
        addAndMakeVisible(animation.get());
    }

    ~XYPadComponent() override = default;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background matches plugin background
        g.setColour(juce::Colour(0xff101010));
        g.fillRoundedRectangle(bounds, 6.0f);

        // Border (2px)
        g.setColour(juce::Colour(0xff595e5f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);

        // Dot is drawn in paintOverChildren() so it renders above the animation
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Calculate dot position from parameter values
        float padLeft = bounds.getX() + dotRadius;
        float padRight = bounds.getRight() - dotRadius;
        float padTop = bounds.getY() + dotRadius;
        float padBottom = bounds.getBottom() - dotRadius;

        float normX = xValue / 255.0f;
        float normY = yValue / 255.0f;

        // X: left=0, right=255; Y: top=255, bottom=0 (inverted)
        float dotX = padLeft + normX * (padRight - padLeft);
        float dotY = padTop + (1.0f - normY) * (padBottom - padTop);

        // Dark radial shadow behind dot for visibility over animation
        juce::ColourGradient shadow(
            juce::Colour(0xff101010).withAlpha(0.8f), dotX, dotY,
            juce::Colour(0xff101010).withAlpha(0.0f), dotX + dotRadius * 2.5f, dotY,
            true);
        g.setGradientFill(shadow);
        g.fillEllipse(dotX - dotRadius * 2.5f, dotY - dotRadius * 2.5f,
                       dotRadius * 5.0f, dotRadius * 5.0f);

        // Draw purple dot
        g.setColour(juce::Colour(0xff9b74f6));
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }

    void resized() override
    {
        if (animation)
            animation->setBounds(getLocalBounds());
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        xAttachment->beginGesture();
        yAttachment->beginGesture();
        updateValuesFromMouse(event.position);
        lastRippleTime = juce::Time::getMillisecondCounter();
        if (animation) animation->triggerUserRipple(event.position.x, event.position.y);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        updateValuesFromMouse(event.position);

        // Throttle ripples to every 100ms during drag
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastRippleTime >= 100)
        {
            if (animation) animation->triggerUserRipple(event.position.x, event.position.y);
            lastRippleTime = now;
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        xAttachment->endGesture();
        yAttachment->endGesture();

        // Final ripple when user stops dragging
        if (animation) animation->triggerUserRipple(event.position.x, event.position.y);
    }

    void setDotRadius(float radius)
    {
        dotRadius = radius;
        if (animation) animation->setDotRadius(radius);
        repaint();
    }

    // Forward note trigger to animation overlay
    void triggerNoteAnimation(int channelIndex, uint8_t velocity, double ppqPosition, double bpm)
    {
        if (animation)
            animation->triggerNote(channelIndex, velocity, ppqPosition, bpm);
    }

    // Public access to animation for editor callbacks
    std::unique_ptr<XYPadAnimation> animation;

private:
    void updateValuesFromMouse(juce::Point<float> pos)
    {
        auto bounds = getLocalBounds().toFloat();

        float padLeft = bounds.getX() + dotRadius;
        float padRight = bounds.getRight() - dotRadius;
        float padTop = bounds.getY() + dotRadius;
        float padBottom = bounds.getBottom() - dotRadius;

        float normX = juce::jlimit(0.0f, 1.0f, (pos.x - padLeft) / (padRight - padLeft));
        float normY = juce::jlimit(0.0f, 1.0f, 1.0f - (pos.y - padTop) / (padBottom - padTop));

        xAttachment->setValueAsPartOfGesture(juce::roundToInt(normX * 255.0f));
        yAttachment->setValueAsPartOfGesture(juce::roundToInt(normY * 255.0f));
    }

    std::unique_ptr<juce::ParameterAttachment> xAttachment;
    std::unique_ptr<juce::ParameterAttachment> yAttachment;
    float xValue = 128.0f;
    float yValue = 128.0f;
    float dotRadius = 12.5f;  // 25px diameter (matches LED dot size)
    juce::int64 lastRippleTime = 0;  // For throttling ripples during drag

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYPadComponent)
};

// Chaos Slider Component - horizontal slider with purple dot indicator
// Matches the XY pad visual style (purple dot, gray track)
// Uses ParameterAttachment for direct APVTS parameter binding
class ChaosSliderComponent : public juce::Component
{
public:
    ChaosSliderComponent(juce::RangedAudioParameter& param)
    {
        setInterceptsMouseClicks(true, false);
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);

        attachment = std::make_unique<juce::ParameterAttachment>(param, [this](float val) {
            chaosValue = val;
            repaint();
        });

        attachment->sendInitialUpdate();
    }

    ~ChaosSliderComponent() override = default;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float trackHeight = 2.0f;
        float trackY = bounds.getCentreY() - trackHeight / 2.0f;

        // Track spans the full component width (aligns with XY pad edges above)
        float trackLeft = bounds.getX();
        float trackRight = bounds.getRight();
        float trackWidth = trackRight - trackLeft;

        // Dot center moves within padded range so the dot doesn't clip edges
        float dotLeft = bounds.getX() + dotRadius;
        float dotRight = bounds.getRight() - dotRadius;
        float dotRange = dotRight - dotLeft;

        float normValue = chaosValue / 127.0f;

        // Gray track background
        g.setColour(juce::Colour(0xff555555));
        g.fillRoundedRectangle(trackLeft, trackY, trackWidth, trackHeight, trackHeight / 2.0f);

        // Purple filled portion (from left edge to dot center)
        float dotX = dotLeft + normValue * dotRange;
        float filledWidth = dotX - trackLeft;
        if (filledWidth > 0)
        {
            g.setColour(juce::Colour(0xff9b74f6));
            g.fillRoundedRectangle(trackLeft, trackY, filledWidth, trackHeight, trackHeight / 2.0f);
        }

        // Purple dot at current position
        float dotY = bounds.getCentreY();
        g.setColour(juce::Colour(0xff9b74f6));
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        attachment->beginGesture();
        updateValueFromMouse(event.position);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        updateValueFromMouse(event.position);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        attachment->endGesture();
    }

    void setDotRadius(float radius) { dotRadius = radius; repaint(); }

private:
    void updateValueFromMouse(juce::Point<float> pos)
    {
        auto bounds = getLocalBounds().toFloat();
        float trackLeft = bounds.getX() + dotRadius;
        float trackRight = bounds.getRight() - dotRadius;

        float normValue = juce::jlimit(0.0f, 1.0f, (pos.x - trackLeft) / (trackRight - trackLeft));
        attachment->setValueAsPartOfGesture(juce::roundToInt(normValue * 127.0f));
    }

    std::unique_ptr<juce::ParameterAttachment> attachment;
    float chaosValue = 0.0f;
    float dotRadius = 12.5f;  // 25px diameter (matches XY pad dot)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaosSliderComponent)
};

// Velocity Bender instrument selection button - rounded frame, colored/gray based on state
class VelocityBenderButton : public juce::Component
{
public:
    VelocityBenderButton()
    {
        setInterceptsMouseClicks(true, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Make it square using the smaller dimension
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        float x = bounds.getX() + (bounds.getWidth() - size) / 2.0f;
        float y = bounds.getY() + (bounds.getHeight() - size) / 2.0f;
        juce::Rectangle<float> squareBounds(x, y, size, size);

        float cornerRadius = size * 0.2f;  // Rounded corners
        float frameThickness = 2.5f;

        // Interior color (same as knob interior)
        juce::Colour interiorColour(0xff313135);
        // Frame color: instrument color when ON, same as knob background when OFF
        juce::Colour frameColour = isOn ? buttonColour : juce::Colour(0xff313135);
        // Letter color: instrument color when ON, light gray when OFF (same as MixMuteButton)
        juce::Colour letterColour = isOn ? buttonColour : juce::Colour(0xffb0b0b0);

        // Draw interior (filled rounded rect)
        g.setColour(interiorColour);
        g.fillRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius);

        // Draw frame (rounded rect outline)
        g.setColour(frameColour);
        g.drawRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius, frameThickness);

        // Draw "B" letter - same size as "M" on MixMuteButton
        float fontSize = size * 0.45f;
        g.setFont(juce::Font(fontSize, juce::Font::plain));
        g.setColour(letterColour);
        g.drawText("B", squareBounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        // Toggle the internal button - this triggers the ButtonAttachment to update parameter
        // and fires onStateChange callback which calls syncWithToggleButton to update visual state
        internalToggle.setToggleState(!internalToggle.getToggleState(), juce::sendNotificationSync);
    }

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (isOn != shouldBeOn)
        {
            isOn = shouldBeOn;
            repaint();

            if (notification != juce::dontSendNotification && onStateChange)
                onStateChange(isOn);
        }
    }

    bool getToggleState() const { return isOn; }

    void setButtonColour(juce::Colour colour)
    {
        buttonColour = colour;
        repaint();
    }

    // Callback when state changes (for parameter binding)
    std::function<void(bool)> onStateChange;

    // Get reference to internal toggle button for parameter attachment
    juce::ToggleButton& getToggleButton() { return internalToggle; }

    // Sync visual state with internal toggle button
    void syncWithToggleButton()
    {
        setToggleState(internalToggle.getToggleState(), juce::dontSendNotification);
    }

    void parentHierarchyChanged() override
    {
        // Sync when added to parent
        syncWithToggleButton();
    }

private:
    bool isOn = false;
    juce::Colour buttonColour = juce::Colours::green;

    // Internal toggle button for parameter attachment (hidden)
    juce::ToggleButton internalToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelocityBenderButton)
};

// AnimationButton - Toggle button for XY pad animation control
// Shows "ON" with purple frame when enabled, "OFF" with no frame when disabled
class AnimationButton : public juce::Component
{
public:
    AnimationButton()
    {
        setInterceptsMouseClicks(true, false);
        buttonColour = juce::Colour(0xff9b74f6);  // Purple
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Make it square using the smaller dimension
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        float x = bounds.getX() + (bounds.getWidth() - size) / 2.0f;
        float y = bounds.getY() + (bounds.getHeight() - size) / 2.0f;
        juce::Rectangle<float> squareBounds(x, y, size, size);

        float cornerRadius = size * 0.2f;
        float frameThickness = 2.5f;

        // Interior color (same as knob interior)
        juce::Colour interiorColour(0xff313135);
        // Frame color: purple when ON, background color when OFF
        juce::Colour frameColour = isOn ? buttonColour : juce::Colour(0xff313135);
        // Text color: purple when ON, light gray when OFF
        juce::Colour textColour = isOn ? buttonColour : juce::Colour(0xffb0b0b0);

        // Draw interior (filled rounded rect)
        g.setColour(interiorColour);
        g.fillRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius);

        // Draw frame (rounded rect outline)
        g.setColour(frameColour);
        g.drawRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius, frameThickness);

        // Draw "ON" or "OFF" text
        float fontSize = size * 0.28f;  // Smaller for two-letter text
        g.setFont(juce::Font(fontSize, juce::Font::bold));
        g.setColour(textColour);
        g.drawText(isOn ? "ON" : "OFF", squareBounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        setToggleState(!isOn, juce::sendNotificationSync);
    }

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (isOn != shouldBeOn)
        {
            isOn = shouldBeOn;
            repaint();

            if (notification != juce::dontSendNotification && onStateChange)
                onStateChange(isOn);
        }
    }

    bool getToggleState() const { return isOn; }

    // Callback when state changes
    std::function<void(bool)> onStateChange;

private:
    bool isOn = true;  // Default: enabled
    juce::Colour buttonColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnimationButton)
};

// MixMuteButton - Similar to VelocityBenderButton but with "M" letter inside
// Default state is OFF. When OFF, frame and letter are gray. When ON, frame and letter show instrument color.
class MixMuteButton : public juce::Component, private juce::Button::Listener
{
public:
    MixMuteButton()
    {
        setInterceptsMouseClicks(true, false);
        // Listen for internal toggle state changes (from click or parameter attachment)
        internalToggle.addListener(this);
    }

    ~MixMuteButton() override
    {
        internalToggle.removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Make it square using the smaller dimension
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        float x = bounds.getX() + (bounds.getWidth() - size) / 2.0f;
        float y = bounds.getY() + (bounds.getHeight() - size) / 2.0f;
        juce::Rectangle<float> squareBounds(x, y, size, size);

        float cornerRadius = size * 0.2f;  // Rounded corners
        float frameThickness = 2.5f;

        // Interior color (same as knob interior)
        juce::Colour interiorColour(0xff313135);
        // Frame color: instrument color when ON, same as knob background when OFF
        juce::Colour frameColour = isOn ? buttonColour : juce::Colour(0xff313135);
        // Letter color: instrument color when ON, light gray when OFF
        juce::Colour letterColour = isOn ? buttonColour : juce::Colour(0xffb0b0b0);

        // Draw interior (filled rounded rect)
        g.setColour(interiorColour);
        g.fillRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius);

        // Draw frame (rounded rect outline)
        g.setColour(frameColour);
        g.drawRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius, frameThickness);

        // Draw "M" letter - slightly smaller than instrument labels
        float fontSize = size * 0.45f;  // Slightly smaller than row labels (0.38f ratio)
        g.setFont(juce::Font(fontSize, juce::Font::plain));
        g.setColour(letterColour);
        g.drawText("M", squareBounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        // Toggle the internal button - this triggers the ButtonAttachment to update parameter
        internalToggle.setToggleState(!internalToggle.getToggleState(), juce::sendNotificationSync);
    }

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (isOn != shouldBeOn)
        {
            isOn = shouldBeOn;
            repaint();

            if (notification != juce::dontSendNotification && onStateChange)
                onStateChange(isOn);
        }
    }

    bool getToggleState() const { return isOn; }

    void setButtonColour(juce::Colour colour)
    {
        buttonColour = colour;
        repaint();
    }

    // Callback when state changes (for parameter binding)
    std::function<void(bool)> onStateChange;

    // Get reference to internal toggle button for parameter attachment
    juce::ToggleButton& getToggleButton() { return internalToggle; }

    // Sync visual state with internal toggle button
    void syncWithToggleButton()
    {
        setToggleState(internalToggle.getToggleState(), juce::dontSendNotification);
    }

    void parentHierarchyChanged() override
    {
        // Sync when added to parent
        syncWithToggleButton();
    }

private:
    // Button::Listener callbacks
    void buttonClicked(juce::Button*) override
    {
        // Sync visual state when internal toggle is clicked (via mouseDown or parameter change)
        syncWithToggleButton();
    }

    void buttonStateChanged(juce::Button*) override
    {
        // Not used for toggle state, but required by Button::Listener interface
    }

    bool isOn = false;
    juce::Colour buttonColour = juce::Colours::white;

    // Internal toggle button for parameter attachment (hidden)
    juce::ToggleButton internalToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixMuteButton)
};

// MixSoloButton - Same as MixMuteButton but with "S" letter inside
// Default state is OFF. When OFF, frame and letter are gray. When ON, frame and letter show instrument color.
class MixSoloButton : public juce::Component, private juce::Button::Listener
{
public:
    MixSoloButton()
    {
        setInterceptsMouseClicks(true, false);
        internalToggle.addListener(this);
    }

    ~MixSoloButton() override
    {
        internalToggle.removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        float x = bounds.getX() + (bounds.getWidth() - size) / 2.0f;
        float y = bounds.getY() + (bounds.getHeight() - size) / 2.0f;
        juce::Rectangle<float> squareBounds(x, y, size, size);

        float cornerRadius = size * 0.2f;
        float frameThickness = 2.5f;

        juce::Colour interiorColour(0xff313135);
        // Frame color: instrument color when ON, same as knob background when OFF
        juce::Colour frameColour = isOn ? buttonColour : juce::Colour(0xff313135);
        // Letter color: instrument color when ON, light gray when OFF
        juce::Colour letterColour = isOn ? buttonColour : juce::Colour(0xffb0b0b0);

        g.setColour(interiorColour);
        g.fillRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius);

        g.setColour(frameColour);
        g.drawRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius, frameThickness);

        float fontSize = size * 0.45f;
        g.setFont(juce::Font(fontSize, juce::Font::plain));
        g.setColour(letterColour);
        g.drawText("S", squareBounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        internalToggle.setToggleState(!internalToggle.getToggleState(), juce::sendNotificationSync);
    }

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (isOn != shouldBeOn)
        {
            isOn = shouldBeOn;
            repaint();

            if (notification != juce::dontSendNotification && onStateChange)
                onStateChange(isOn);
        }
    }

    bool getToggleState() const { return isOn; }

    void setButtonColour(juce::Colour colour)
    {
        buttonColour = colour;
        repaint();
    }

    std::function<void(bool)> onStateChange;

    juce::ToggleButton& getToggleButton() { return internalToggle; }

    void syncWithToggleButton()
    {
        setToggleState(internalToggle.getToggleState(), juce::dontSendNotification);
    }

    void parentHierarchyChanged() override
    {
        syncWithToggleButton();
    }

private:
    void buttonClicked(juce::Button*) override
    {
        syncWithToggleButton();
    }

    void buttonStateChanged(juce::Button*) override {}

    bool isOn = false;
    juce::Colour buttonColour = juce::Colours::white;
    juce::ToggleButton internalToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixSoloButton)
};

// TransportButton - Play/Stop button for standalone mode
// Shows play icon (▶) when stopped, stop icon (■) when playing
// Green frame when playing, gray frame when stopped
class TransportButton : public juce::Component, private juce::Timer
{
public:
    TransportButton()
    {
        setInterceptsMouseClicks(true, false);
    }

    ~TransportButton() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Make it square using the smaller dimension
        float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        float x = bounds.getX() + (bounds.getWidth() - size) / 2.0f;
        float y = bounds.getY() + (bounds.getHeight() - size) / 2.0f;
        juce::Rectangle<float> squareBounds(x, y, size, size);

        float cornerRadius = size * 0.2f;
        float frameThickness = 2.5f;

        // Check if audio system is ready (if callback is set)
        bool audioReady = !isAudioActive || isAudioActive();

        // Interior color (same as knob interior)
        juce::Colour interiorColour(0xff313135);

        // WARNING STATE: If playing but audio not ready, show red warning
        juce::Colour frameColour;
        juce::Colour iconColour;

        if (isPlaying && !audioReady)
        {
            // Warning state: red border and icon to indicate audio isn't running
            frameColour = juce::Colour(0xffff4444);  // Red warning
            iconColour = juce::Colour(0xffff4444);
        }
        else if (isPlaying)
        {
            // Normal playing state: purple
            frameColour = juce::Colour(0xff9b74f6);
            iconColour = juce::Colour(0xff9b74f6);
        }
        else
        {
            // Stopped state: dark frame, purple play icon
            frameColour = juce::Colour(0xff313135);
            iconColour = juce::Colour(0xff9b74f6);
        }

        // Draw interior (filled rounded rect)
        g.setColour(interiorColour);
        g.fillRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius);

        // Draw frame (rounded rect outline)
        g.setColour(frameColour);
        g.drawRoundedRectangle(squareBounds.reduced(frameThickness / 2.0f), cornerRadius, frameThickness);

        // Draw icon in center
        float iconSize = size * 0.35f;
        float iconX = squareBounds.getCentreX();
        float iconY = squareBounds.getCentreY();

        g.setColour(iconColour);

        if (isPlaying)
        {
            // Stop icon: filled square
            float stopSize = iconSize * 0.8f;
            g.fillRect(iconX - stopSize / 2.0f, iconY - stopSize / 2.0f, stopSize, stopSize);
        }
        else
        {
            // Play icon: triangle pointing right
            juce::Path triangle;
            float triHeight = iconSize;
            float triWidth = iconSize * 0.866f;  // equilateral triangle ratio
            // Shift slightly right for visual centering
            float offsetX = iconSize * 0.1f;
            triangle.addTriangle(
                iconX - triWidth / 2.0f + offsetX, iconY - triHeight / 2.0f,
                iconX - triWidth / 2.0f + offsetX, iconY + triHeight / 2.0f,
                iconX + triWidth / 2.0f + offsetX, iconY
            );
            g.fillPath(triangle);
        }
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        if (onToggle)
            onToggle(!isPlaying);
    }

    void setPlaying(bool playing)
    {
        if (isPlaying != playing)
        {
            isPlaying = playing;
            repaint();
        }
    }

    bool getIsPlaying() const { return isPlaying; }

    // Start timer to sync with audio thread state
    void startSyncTimer(int intervalMs = 100)
    {
        startTimer(intervalMs);
    }

    // Callback when toggled - passes the NEW state (what we want to become)
    std::function<void(bool)> onToggle;

    // Callback to query current playing state from audio thread
    std::function<bool()> getPlayingState;

    // Callback to check if audio callback is running
    std::function<bool()> isAudioActive;

private:
    void timerCallback() override
    {
        bool needsRepaint = false;

        if (getPlayingState)
        {
            bool newState = getPlayingState();
            if (newState != isPlaying)
            {
                isPlaying = newState;
                needsRepaint = true;
            }
        }

        // Check audio state for warning display
        if (isAudioActive)
        {
            bool audioActive = isAudioActive();

            // Track audio state changes for repaint
            if (audioActive != lastAudioState)
            {
                lastAudioState = audioActive;
                needsRepaint = true;
            }

            // Diagnostic logging
            if (isPlaying && !audioActive && !audioWarningLogged)
            {
                audioWarningLogged = true;
            }
            else if (audioActive && audioWarningLogged)
            {
                audioWarningLogged = false;
            }
        }

        if (needsRepaint)
            repaint();
    }

    bool isPlaying = false;
    bool lastAudioState = false;
    bool audioWarningLogged = false;  // Prevent log spam

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportButton)
};

// PriorityLEDButton - LED-style button for Priority Matrix in MIX tab
// Looks like PatternLED: filled circle when ON, outline circle when OFF
// Clickable for user interaction, syncs with parameter via onClick callback
class PriorityLEDButton : public juce::Component
{
public:
    PriorityLEDButton()
    {
        setInterceptsMouseClicks(true, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();

        // Line thickness matching LEDs (2px)
        float lineThickness = 2.0f;

        if (isOn)
        {
            // When ON: fill with instrument color (LED lit)
            g.setColour(ledColour);
            g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
        }
        else
        {
            // When OFF: draw circle outline in knob interior color (same as knob background)
            g.setColour(juce::Colour(0xff313135));
            g.drawEllipse(centreX - radius + lineThickness / 2.0f,
                         centreY - radius + lineThickness / 2.0f,
                         (radius - lineThickness / 2.0f) * 2.0f,
                         (radius - lineThickness / 2.0f) * 2.0f,
                         lineThickness);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.getNumberOfClicks() == 2)
        {
            // Double-click: select all in column
            if (onDoubleClick)
                onDoubleClick();
        }
        else
        {
            // Single click: normal selection (radio button behavior)
            if (onClick)
                onClick();
        }
    }

    void setToggleState(bool shouldBeOn)
    {
        if (isOn != shouldBeOn)
        {
            isOn = shouldBeOn;
            repaint();
        }
    }

    bool getToggleState() const { return isOn; }

    void setLEDColour(juce::Colour colour)
    {
        ledColour = colour;
        repaint();
    }

    // Callback when clicked (single click)
    std::function<void()> onClick;

    // Callback when double-clicked (select all in column)
    std::function<void()> onDoubleClick;

private:
    bool isOn = false;
    juce::Colour ledColour = juce::Colours::green;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PriorityLEDButton)
};

// VB Random/Reset button - rectangular frame that flashes purple on click, displays RANDOM/RESET text
class VBRandomResetButton : public juce::Component, private juce::Timer
{
public:
    VBRandomResetButton()
    {
        setInterceptsMouseClicks(true, false);
    }

    ~VBRandomResetButton() override
    {
        stopTimer();
    }

    void setText(const juce::String& text)
    {
        displayText = text;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        float cornerRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.2f;
        float frameThickness = 2.5f;

        // Interior color (same as knob interior)
        juce::Colour interiorColour(0xff313135);
        // Frame and text color: purple (always purple, matching VB theme)
        juce::Colour purpleColour(0xff9b74f6);
        // Frame color: purple when flashing, same as knob background otherwise
        juce::Colour frameColour = isFlashing ? purpleColour : juce::Colour(0xff313135);

        // Draw interior (filled rounded rect)
        g.setColour(interiorColour);
        g.fillRoundedRectangle(bounds.reduced(frameThickness / 2.0f), cornerRadius);

        // Draw frame (rounded rect outline)
        g.setColour(frameColour);
        g.drawRoundedRectangle(bounds.reduced(frameThickness / 2.0f), cornerRadius, frameThickness);

        // Draw RANDOM/RESET text - purple color
        float fontSize = bounds.getHeight() * 0.35f;
        g.setFont(juce::Font(fontSize, juce::Font::plain));
        g.setColour(purpleColour);
        g.drawText(displayText, bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& /*event*/) override
    {
        // Start flash effect
        isFlashing = true;
        repaint();
        startTimer(100);  // Flash for 100ms

        // Call the onClick callback
        if (onClick)
            onClick();
    }

    // Callback when button is clicked
    std::function<void()> onClick;

private:
    void timerCallback() override
    {
        // End flash effect
        isFlashing = false;
        stopTimer();
        repaint();
    }

    bool isFlashing = false;
    juce::String displayText = "RANDOM";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VBRandomResetButton)
};

// Get a random drum onomatopoeia word (shared utility function)
static inline juce::String getRandomDrumWord()
{
    static const char* drumWords[] = {
        "BOOM", "THUMP", "DOOM", "BUH", "PUH", "UMPH", "TAP", "TSS",
        "TCHAK", "TUH", "TSS", "TCH", "SSS", "CHIK", "TING", "TSIK",
        "KSS", "DUM", "BAP", "GUH", "TOH", "DUG", "KSH", "KLAK", "TUM",
        "BWOOM", "THUD", "DHOM", "GLOOMP", "BUNK", "WHUMP", "DHOMPF",
        "BDOOM", "GOMP", "THWUM", "PAKK", "TAKK", "BRAP", "DRAP",
        "TCHACK", "PRAT", "KRAT", "TWAP", "CHRAP", "DATT", "TSSSHT",
        "TZZT", "TZINK", "TCHSSS", "SHTIK", "TZIFF", "KSSSHT", "TZANG",
        "TZING", "SSSK", "TIK", "TUK", "KLIP", "KLIK", "TCHK", "KTIK",
        "PTIK", "TUP", "KTAP", "TLICK", "CLANG", "CLONK", "TINGK",
        "TLONG", "KLANG", "PLING", "TLONK", "CHLONG", "KLING", "TZONG",
        "FSHH", "HFF", "PFFF", "SHFF", "KFFF", "FSSHT", "HUSHK",
        "FROOSH", "SWOOMP", "KHRSH", "TSA", "PLUM", "PUK", "STUK",
        "BUM", "TRZASK"
    };
    static const int numWords = sizeof(drumWords) / sizeof(drumWords[0]);
    return juce::String(drumWords[juce::Random::getSystemRandom().nextInt(numWords)]);
}

// Responsive layout constants for tab components
static constexpr int kResponsiveBreakpointStart = 900;
static constexpr float kResponsiveBreakpointRange = 400.0f;

// PATTERN Tab component with dynamic 12x6 grid layout + header row
class PatternTabComponent : public Component, public Slider::Listener,
                            public MidiMappingManager::InstrumentNameListener
{
public:
    static constexpr int NUM_COLS = 12;
    static constexpr int NUM_ROWS = 6;
    static constexpr int MIN_CELL_SIZE = 40;
    static constexpr float HEADER_ROW_RATIO = 0.333f;  // Header row is 1/3 height of normal rows

    // Row to channel mapping: BD=0, SN=1, HH=2, BD'=3, SN'=4, HH'=5
    static constexpr int rowToChannel[NUM_ROWS] = {0, 1, 2, 3, 4, 5};

    PatternTabComponent(AudioProcessorValueTreeState& apvts, AugmaticGREProcessor& proc)
        : audioProcessor(proc)
    {
        auto* mappingMgr = proc.getMidiMappingManager();

        // Header row labels (font set in resized() for scalability)
        densityHeaderLabel = std::make_unique<Label>("densityHeader", "DENSITY");
        densityHeaderLabel->setJustificationType(Justification::centred);
        densityHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(densityHeaderLabel.get());

        chaosHeaderLabel = std::make_unique<Label>("chaosHeader", "CHAOS");
        chaosHeaderLabel->setJustificationType(Justification::centred);
        chaosHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(chaosHeaderLabel.get());

        blendHeaderLabel = std::make_unique<Label>("blendHeader", "BLEND");
        blendHeaderLabel->setJustificationType(Justification::centred);
        blendHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(blendHeaderLabel.get());

        stepsHeaderLabel = std::make_unique<Label>("stepsHeader", "STEPS");
        stepsHeaderLabel->setJustificationType(Justification::centred);
        stepsHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(stepsHeaderLabel.get());

        pulsesHeaderLabel = std::make_unique<Label>("pulsesHeader", "PULSES");
        pulsesHeaderLabel->setJustificationType(Justification::centred);
        pulsesHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(pulsesHeaderLabel.get());

        startOnHeaderLabel = std::make_unique<Label>("startOnHeader", "START ON");
        startOnHeaderLabel->setJustificationType(Justification::centred);
        startOnHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(startOnHeaderLabel.get());

        swingHeaderLabel = std::make_unique<Label>("swingHeader", "SWING");
        swingHeaderLabel->setJustificationType(Justification::centred);
        swingHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(swingHeaderLabel.get());

        humanizeHeaderLabel = std::make_unique<Label>("humanizeHeader", "HUMANIZE");
        humanizeHeaderLabel->setJustificationType(Justification::centred);
        humanizeHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(humanizeHeaderLabel.get());

        shiftHeaderLabel = std::make_unique<Label>("shiftHeader", "SHIFT");
        shiftHeaderLabel->setJustificationType(Justification::centred);
        shiftHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(shiftHeaderLabel.get());

        clockHeaderLabel = std::make_unique<Label>("clockHeader", "CLOCK");
        clockHeaderLabel->setJustificationType(Justification::centred);
        clockHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(clockHeaderLabel.get());

        // LED column header - displays random drum word
        ledHeaderLabel = std::make_unique<Label>("ledHeader", getRandomDrumWord());
        ledHeaderLabel->setJustificationType(Justification::centred);
        ledHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(ledHeaderLabel.get());

        // Row colors from colors.md: BD=Red, SN=Blue, HH=Yellow, BD'=Pink, SN'=Green, HH'=Orange
        const Colour rowColours[] = {
            Colour(0xffff4757),  // BD - Red
            Colour(0xff5f9fec),  // SN - Blue
            Colour(0xfffeca57),  // HH - Yellow
            Colour(0xffff6b9d),  // BD' - Pink
            Colour(0xff1dd1a1),  // SN' - Green
            Colour(0xffff9f43)   // HH' - Orange
        };

        // Parameter IDs for density knobs (BD, SN, HH, BD', SN', HH')
        const char* densityParamIds[] = {
            "density_bd",
            "density_sd",
            "density_hh",
            "density_bd_acc",
            "density_sn_acc",
            "density_hh_acc"
        };

        // Parameter IDs for chaos knobs (BD, SN, HH, BD', SN', HH')
        const char* chaosParamIds[] = {
            "bd_chaos",
            "sn_chaos",
            "hh_chaos",
            "bd_acc_chaos",
            "sn_acc_chaos",
            "hh_acc_chaos"
        };

        // Parameter IDs for blend knobs - engine probability (0=Grids, 1=Euclidean, 0.5=50/50)
        const char* blendParamIds[] = {
            "bd_engine_probability",
            "sn_engine_probability",
            "hh_engine_probability",
            "bd_acc_engine_probability",
            "sn_acc_engine_probability",
            "hh_acc_engine_probability"
        };

        // Parameter IDs for steps displays - Euclidean pattern length (2-32)
        const char* stepsParamIds[] = {
            "bd_euclidean_steps",
            "sn_euclidean_steps",
            "hh_euclidean_steps",
            "bd_acc_euclidean_steps",
            "sn_acc_euclidean_steps",
            "hh_acc_euclidean_steps"
        };

        // Parameter IDs for pulses displays - Euclidean pulse count (0-steps)
        const char* pulsesParamIds[] = {
            "bd_euclidean_pulses",
            "sn_euclidean_pulses",
            "hh_euclidean_pulses",
            "bd_acc_euclidean_pulses",
            "sn_acc_euclidean_pulses",
            "hh_acc_euclidean_pulses"
        };

        // Parameter IDs for start-on displays - Euclidean start step (1 to steps)
        const char* startOnParamIds[] = {
            "bd_euclidean_start",
            "sn_euclidean_start",
            "hh_euclidean_start",
            "bd_acc_euclidean_start",
            "sn_acc_euclidean_start",
            "hh_acc_euclidean_start"
        };

        // Parameter IDs for swing knobs - bipolar (-99 to +99)
        const char* swingParamIds[] = {
            "bd_swing",
            "sn_swing",
            "hh_swing",
            "bd_acc_swing",
            "sn_acc_swing",
            "hh_acc_swing"
        };

        // Parameter IDs for humanize displays (0-127)
        const char* humanizeParamIds[] = {
            "bd_humanize",
            "sn_humanize",
            "hh_humanize",
            "bd_acc_humanize",
            "sn_acc_humanize",
            "hh_acc_humanize"
        };

        // Parameter IDs for shift knobs - bipolar (0-126, 63=center/default)
        const char* shiftParamIds[] = {
            "bd_shift",
            "sn_shift",
            "hh_shift",
            "bd_acc_shift",
            "sn_acc_shift",
            "hh_acc_shift"
        };

        // Parameter IDs for clock ratio displays (0-17, 8=x1 default)
        const char* clockParamIds[] = {
            "bd_clock_ratio",
            "sn_clock_ratio",
            "hh_clock_ratio",
            "bd_acc_clock_ratio",
            "sn_acc_clock_ratio",
            "hh_acc_clock_ratio"
        };

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            // Create row label (column 1) with row-specific color - regular font, size set in resized()
            juce::String labelText = mappingMgr ? mappingMgr->getInstrumentName(row) : juce::String(MidiMappingManager::DEFAULT_INSTRUMENT_NAMES[row]);
            rowLabels[row] = std::make_unique<Label>("rowLabel" + String(row), labelText);
            rowLabels[row]->setJustificationType(Justification::centred);
            rowLabels[row]->setFont(Font(28.0f, Font::plain));
            rowLabels[row]->setColour(Label::textColourId, rowColours[row]);
            addAndMakeVisible(rowLabels[row].get());

            // Create LED indicator (column 12) - thin circle outline, fills when lit
            // v0.4.087: Clickable - triggers MIDI note on click
            leds[row] = std::make_unique<PatternLED>();
            leds[row]->setLEDColour(rowColours[row]);
            leds[row]->onClicked = [this, row]() {
                int channelIdx = rowToChannel[row];
                audioProcessor.triggerNoteFromUI(channelIdx, 100);
                updateRandomDrumWord();
            };
            addAndMakeVisible(leds[row].get());

            // Create density knob (column 2) with row-specific color and custom look
            densityKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            densityKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            densityKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            densityKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            densityKnobs[row]->setLookAndFeel(&knobLookAndFeel);
            addAndMakeVisible(densityKnobs[row].get());

            // Attach to parameter first, then override double-click behavior
            densityAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, densityParamIds[row], *densityKnobs[row]);
            densityKnobs[row]->setDoubleClickReturnValue(true, 0.0);

            // Create chaos knob (column 3) with row-specific color and custom look
            chaosKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            chaosKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            chaosKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            chaosKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            chaosKnobs[row]->setLookAndFeel(&knobLookAndFeel);
            addAndMakeVisible(chaosKnobs[row].get());

            // Attach to parameter first, then override double-click behavior
            chaosAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, chaosParamIds[row], *chaosKnobs[row]);
            chaosKnobs[row]->setDoubleClickReturnValue(true, 0.0);

            // Create blend knob (column 4) - bipolar, 0 = 100% Grids, 1 = 100% Euclidean
            blendKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            blendKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            blendKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            blendKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            blendKnobs[row]->setLookAndFeel(&bipolarKnobLookAndFeel);
            addAndMakeVisible(blendKnobs[row].get());

            // Attach to parameter first, then override double-click to return to 0 (100% Grids)
            blendAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, blendParamIds[row], *blendKnobs[row]);
            blendKnobs[row]->setDoubleClickReturnValue(true, 0.0);

            // Create steps display (column 5) - scrollable number, range 2-32, default 16
            stepsDisplays[row] = std::make_unique<ScrollableNumberDisplay>();
            stepsDisplays[row]->setRange(2.0, 32.0, 1.0);
            stepsDisplays[row]->setTextColour(rowColours[row]);
            addAndMakeVisible(stepsDisplays[row].get());

            // Attach to parameter
            stepsAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, stepsParamIds[row], stepsDisplays[row]->getSlider());

            // Create pulses display (column 6) - scrollable number, range 0-16 (default, will be updated based on steps)
            pulsesDisplays[row] = std::make_unique<ScrollableNumberDisplay>();
            pulsesDisplays[row]->setRange(0.0, 16.0, 1.0);  // Initial range, will update when steps changes
            pulsesDisplays[row]->setDefaultValue(4.0);  // Default pulses = 4
            pulsesDisplays[row]->setTextColour(rowColours[row]);
            addAndMakeVisible(pulsesDisplays[row].get());

            // Attach to parameter
            pulsesAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, pulsesParamIds[row], pulsesDisplays[row]->getSlider());

            // Create start-on display (column 7) - scrollable number, range 1 to 16 (default, will be updated based on steps)
            startOnDisplays[row] = std::make_unique<ScrollableNumberDisplay>();
            startOnDisplays[row]->setRange(1.0, 16.0, 1.0);  // Initial range, will update when steps changes
            startOnDisplays[row]->setDefaultValue(1.0);  // Default START ON = 1
            startOnDisplays[row]->setTextColour(rowColours[row]);
            addAndMakeVisible(startOnDisplays[row].get());

            // Attach to parameter
            startOnAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, startOnParamIds[row], startOnDisplays[row]->getSlider());

            // Create swing knob (column 8) - bipolar, -99 to +99, default 0 (12 o'clock)
            swingKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            swingKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            swingKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            swingKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            swingKnobs[row]->setLookAndFeel(&bipolarKnobLookAndFeel);
            addAndMakeVisible(swingKnobs[row].get());

            // Attach to parameter first, then override double-click to return to 0 (no swing)
            swingAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, swingParamIds[row], *swingKnobs[row]);
            swingKnobs[row]->setDoubleClickReturnValue(true, 0.0);

            // Create humanize knob (column 10) - 0-127, default 0
            humanizeKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            humanizeKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            humanizeKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            humanizeKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            humanizeKnobs[row]->setLookAndFeel(&knobLookAndFeel);
            addAndMakeVisible(humanizeKnobs[row].get());

            // Attach to parameter first, then override double-click to return to 0 (no humanize)
            humanizeAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, humanizeParamIds[row], *humanizeKnobs[row]);
            humanizeKnobs[row]->setDoubleClickReturnValue(true, 0.0);

            // Create shift knob (column 9) - bipolar, 0-126, 63=center (12 o'clock)
            shiftKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            shiftKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            shiftKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            shiftKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            shiftKnobs[row]->setLookAndFeel(&bipolarKnobLookAndFeel);
            addAndMakeVisible(shiftKnobs[row].get());

            // Attach to parameter first, then override double-click to return to 63 (center/off)
            shiftAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, shiftParamIds[row], *shiftKnobs[row]);
            shiftKnobs[row]->setDoubleClickReturnValue(true, 63.0);

            // Create clock display (column 11) - scrollable clock ratio (/8 to x8)
            clockDisplays[row] = std::make_unique<ScrollableClockDisplay>();
            clockDisplays[row]->setTextColour(rowColours[row]);
            addAndMakeVisible(clockDisplays[row].get());

            // Attach to parameter
            clockAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, clockParamIds[row], clockDisplays[row]->getSlider());

            // Set up dependency: when steps changes, update pulses and start-on ranges
            stepsDisplays[row]->getSlider().addListener(this);
        }

        // Initialize ranges based on current steps values
        for (int row = 0; row < NUM_ROWS; ++row) {
            updateDependentRanges(row);
        }

        // Register instrument name listener
        if (mappingMgr)
            mappingMgr->addInstrumentNameListener(this);
    }

    // InstrumentNameListener callback
    void instrumentNamesChanged() override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<PatternTabComponent>(this)]() {
            if (safeThis == nullptr) return;
            if (auto* mgr = safeThis->audioProcessor.getMidiMappingManager())
            {
                for (int row = 0; row < NUM_ROWS; ++row)
                    safeThis->rowLabels[row]->setText(mgr->getInstrumentName(row), dontSendNotification);
            }
        });
    }

    // Slider::Listener callback - handle steps changes to update dependent ranges
    void sliderValueChanged(Slider* slider) override
    {
        for (int row = 0; row < NUM_ROWS; ++row) {
            if (slider == &stepsDisplays[row]->getSlider()) {
                updateDependentRanges(row);
                break;
            }
        }
    }

    // Update pulses and start-on ranges based on current steps value (v0.4.125)
    // CRITICAL: setValue() MUST be called BEFORE setRange().
    // setRange() clamps the slider silently (dontSendNotification), so the
    // SliderAttachment is never informed and the APVTS parameter keeps the old
    // unclamped value. By calling setValue() first while the slider still has its
    // old (wider) range, the value change fires the attachment and propagates to
    // the APVTS parameter. setRange() then constrains the visual range afterwards.
    void updateDependentRanges(int row)
    {
        if (row < 0 || row >= NUM_ROWS) return;

        int steps = static_cast<int>(stepsDisplays[row]->getSlider().getValue());

        // --- PULSES: clamp value first, then constrain range ---
        double currentPulses = pulsesDisplays[row]->getSlider().getValue();
        if (currentPulses > steps) {
            pulsesDisplays[row]->getSlider().setValue(static_cast<double>(steps), juce::sendNotificationSync);
        }
        pulsesDisplays[row]->setRange(0.0, static_cast<double>(steps), 1.0);

        // --- START ON: clamp value first, then constrain range ---
        double currentStartOn = startOnDisplays[row]->getSlider().getValue();
        double clampedStartOn = juce::jlimit(1.0, static_cast<double>(steps), currentStartOn);
        if (clampedStartOn != currentStartOn) {
            startOnDisplays[row]->getSlider().setValue(clampedStartOn, juce::sendNotificationSync);
        }
        startOnDisplays[row]->setRange(1.0, static_cast<double>(steps), 1.0);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Calculate cell sizes: header row is 1/3 height of normal rows
        int cellWidth = std::max(MIN_CELL_SIZE, bounds.getWidth() / NUM_COLS);

        // Total height = headerHeight + NUM_ROWS * cellHeight
        // headerHeight = cellHeight * HEADER_ROW_RATIO
        // So: totalHeight = cellHeight * (HEADER_ROW_RATIO + NUM_ROWS)
        float totalRowUnits = HEADER_ROW_RATIO + static_cast<float>(NUM_ROWS);
        int cellHeight = std::max(MIN_CELL_SIZE, static_cast<int>(bounds.getHeight() / totalRowUnits));
        int headerHeight = static_cast<int>(cellHeight * HEADER_ROW_RATIO);

        // Position header labels
        densityHeaderLabel->setBounds(1 * cellWidth, 0, cellWidth, headerHeight);
        chaosHeaderLabel->setBounds(2 * cellWidth, 0, cellWidth, headerHeight);
        blendHeaderLabel->setBounds(3 * cellWidth, 0, cellWidth, headerHeight);
        stepsHeaderLabel->setBounds(4 * cellWidth, 0, cellWidth, headerHeight);
        pulsesHeaderLabel->setBounds(5 * cellWidth, 0, cellWidth, headerHeight);
        startOnHeaderLabel->setBounds(6 * cellWidth, 0, cellWidth, headerHeight);
        swingHeaderLabel->setBounds(7 * cellWidth, 0, cellWidth, headerHeight);
        shiftHeaderLabel->setBounds(8 * cellWidth, 0, cellWidth, headerHeight);
        humanizeHeaderLabel->setBounds(9 * cellWidth, 0, cellWidth, headerHeight);
        clockHeaderLabel->setBounds(10 * cellWidth, 0, cellWidth, headerHeight);
        ledHeaderLabel->setBounds(11 * cellWidth, 0, cellWidth, headerHeight);

        // Set scalable fonts for header labels (~55% of header height, minimum 11pt)
        float headerFontSize = std::max(11.0f, static_cast<float>(headerHeight) * 0.55f);
        Font headerFont(headerFontSize, Font::plain);
        densityHeaderLabel->setFont(headerFont);
        chaosHeaderLabel->setFont(headerFont);
        blendHeaderLabel->setFont(headerFont);
        stepsHeaderLabel->setFont(headerFont);
        pulsesHeaderLabel->setFont(headerFont);
        startOnHeaderLabel->setFont(headerFont);
        swingHeaderLabel->setFont(headerFont);
        shiftHeaderLabel->setFont(headerFont);
        humanizeHeaderLabel->setFont(headerFont);
        clockHeaderLabel->setFont(headerFont);
        ledHeaderLabel->setFont(headerFont);

        // Position data rows below header
        int dataRowStartY = headerHeight;

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            int rowY = dataRowStartY + row * cellHeight;

            // Position label in column 1 (index 0) with scalable font
            rowLabels[row]->setBounds(0, rowY, cellWidth, cellHeight);
            float fontSize = static_cast<float>(cellHeight) * 0.38f;  // Scale font to ~38% of cell height (reduced 15% from 45%)
            rowLabels[row]->setFont(Font(fontSize, Font::plain));

            // Position LED in column 12 (index 11) - 25px at ≤900px, scales gradually beyond
            int ledSize;
            if (bounds.getWidth() <= 900) {
                ledSize = 25;
            } else {
                // Gradual scaling: 25px at 900px, max is 60% of knob size
                int knobMaxSize = std::min(cellWidth, cellHeight) - 10;
                int maxSize = static_cast<int>(knobMaxSize * 0.6f);
                float progress = static_cast<float>(bounds.getWidth() - kResponsiveBreakpointStart) / kResponsiveBreakpointRange; // Full size at ~1300px
                progress = std::min(1.0f, progress);
                ledSize = 25 + static_cast<int>((maxSize - 25) * progress);
            }
            int ledX = 11 * cellWidth + (cellWidth - ledSize) / 2;
            int ledY = rowY + (cellHeight - ledSize) / 2;
            leds[row]->setBounds(ledX, ledY, ledSize, ledSize);

            // Position knobs with 5px padding
            int knobPadding = 5;
            int knobSize = std::min(cellWidth, cellHeight) - knobPadding * 2;
            int knobY = rowY + (cellHeight - knobSize) / 2;

            // Position density knob in column 2 (index 1)
            int densityKnobX = 1 * cellWidth + (cellWidth - knobSize) / 2;
            densityKnobs[row]->setBounds(densityKnobX, knobY, knobSize, knobSize);

            // Position chaos knob in column 3 (index 2)
            int chaosKnobX = 2 * cellWidth + (cellWidth - knobSize) / 2;
            chaosKnobs[row]->setBounds(chaosKnobX, knobY, knobSize, knobSize);

            // Position blend knob in column 4 (index 3)
            int blendKnobX = 3 * cellWidth + (cellWidth - knobSize) / 2;
            blendKnobs[row]->setBounds(blendKnobX, knobY, knobSize, knobSize);

            // Position steps display in column 5 (index 4) - same size as label font
            stepsDisplays[row]->setBounds(4 * cellWidth, rowY, cellWidth, cellHeight);
            stepsDisplays[row]->setFont(Font(fontSize, Font::plain));

            // Position pulses display in column 6 (index 5)
            pulsesDisplays[row]->setBounds(5 * cellWidth, rowY, cellWidth, cellHeight);
            pulsesDisplays[row]->setFont(Font(fontSize, Font::plain));

            // Position start-on display in column 7 (index 6)
            startOnDisplays[row]->setBounds(6 * cellWidth, rowY, cellWidth, cellHeight);
            startOnDisplays[row]->setFont(Font(fontSize, Font::plain));

            // Position swing knob in column 8 (index 7)
            int swingKnobX = 7 * cellWidth + (cellWidth - knobSize) / 2;
            swingKnobs[row]->setBounds(swingKnobX, knobY, knobSize, knobSize);

            // Position shift knob in column 9 (index 8)
            int shiftKnobX = 8 * cellWidth + (cellWidth - knobSize) / 2;
            shiftKnobs[row]->setBounds(shiftKnobX, knobY, knobSize, knobSize);

            // Position humanize knob in column 10 (index 9)
            int humanizeKnobX = 9 * cellWidth + (cellWidth - knobSize) / 2;
            humanizeKnobs[row]->setBounds(humanizeKnobX, knobY, knobSize, knobSize);

            // Position clock display in column 11 (index 10)
            clockDisplays[row]->setBounds(10 * cellWidth, rowY, cellWidth, cellHeight);
            clockDisplays[row]->setFont(Font(fontSize, Font::plain));
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff101010));

        // Calculate cell sizes
        auto bounds = getLocalBounds();
        int cellWidth = std::max(MIN_CELL_SIZE, bounds.getWidth() / NUM_COLS);
        float totalRowUnits = HEADER_ROW_RATIO + static_cast<float>(NUM_ROWS);
        int cellHeight = std::max(MIN_CELL_SIZE, static_cast<int>(bounds.getHeight() / totalRowUnits));
        int headerHeight = static_cast<int>(cellHeight * HEADER_ROW_RATIO);
        int dataRowStartY = headerHeight;

        // Grid lines - same color as background (invisible)
        g.setColour(Colour(0xff101010));

        // Vertical lines (full height)
        for (int col = 1; col < NUM_COLS; ++col)
            g.drawVerticalLine(col * cellWidth, 0.0f, static_cast<float>(bounds.getHeight()));

        // Horizontal line below header row
        g.drawHorizontalLine(headerHeight, 0.0f, static_cast<float>(bounds.getWidth()));

        // Horizontal lines for data rows
        for (int row = 1; row < NUM_ROWS; ++row)
            g.drawHorizontalLine(dataRowStartY + row * cellHeight, 0.0f, static_cast<float>(bounds.getWidth()));
    }

    // Trigger LED by row index (0=BD, 1=BD', 2=SN, 3=SN', 4=HH, 5=HH')
    void triggerLED(int rowIndex)
    {
        if (rowIndex >= 0 && rowIndex < NUM_ROWS && leds[rowIndex])
            leds[rowIndex]->trigger();
    }

    // Map channel index to row: BD=0, SN=1, HH=2, BD_Acc=3, SN_Acc=4, HH_Acc=5
    // LED order matches row order now (sequential)
    // Our rows: 0=BD, 1=SN, 2=HH, 3=BD', 4=SN', 5=HH'
    void triggerLEDForChannel(int channelIndex)
    {
        // Channel index now maps directly to row index (sequential order)
        // Channels: 0=BD, 1=SN, 2=HH, 3=BD_Acc, 4=SN_Acc, 5=HH_Acc
        // Rows: 0=BD, 1=SN, 2=HH, 3=BD', 4=SN', 5=HH'
        static const int channelToRow[] = {0, 1, 2, 3, 4, 5};
        if (channelIndex >= 0 && channelIndex < 6)
            triggerLED(channelToRow[channelIndex]);
    }

    // Update LED header with random drum word (called on tab switch)
    void updateRandomDrumWord()
    {
        if (ledHeaderLabel)
            ledHeaderLabel->setText(getRandomDrumWord(), juce::dontSendNotification);
    }

    ~PatternTabComponent() override
    {
        // Remove instrument name listener
        if (auto* mappingMgr = audioProcessor.getMidiMappingManager())
            mappingMgr->removeInstrumentNameListener(this);

        // Clear LookAndFeel before destroying knobs
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            // Remove slider listeners before destroying
            if (stepsDisplays[row])
                stepsDisplays[row]->getSlider().removeListener(this);

            // Clear LookAndFeel before destroying knobs
            if (densityKnobs[row])
                densityKnobs[row]->setLookAndFeel(nullptr);
            if (chaosKnobs[row])
                chaosKnobs[row]->setLookAndFeel(nullptr);
            if (blendKnobs[row])
                blendKnobs[row]->setLookAndFeel(nullptr);
            if (swingKnobs[row])
                swingKnobs[row]->setLookAndFeel(nullptr);
            if (shiftKnobs[row])
                shiftKnobs[row]->setLookAndFeel(nullptr);
            if (humanizeKnobs[row])
                humanizeKnobs[row]->setLookAndFeel(nullptr);
        }
    }

private:
    // Custom LookAndFeel for thin arc knobs with needle
    PatternKnobLookAndFeel knobLookAndFeel;
    BipolarKnobLookAndFeel bipolarKnobLookAndFeel;

    // Header row labels
    std::unique_ptr<Label> densityHeaderLabel;
    std::unique_ptr<Label> chaosHeaderLabel;
    std::unique_ptr<Label> blendHeaderLabel;
    std::unique_ptr<Label> stepsHeaderLabel;
    std::unique_ptr<Label> pulsesHeaderLabel;
    std::unique_ptr<Label> startOnHeaderLabel;
    std::unique_ptr<Label> swingHeaderLabel;
    std::unique_ptr<Label> shiftHeaderLabel;
    std::unique_ptr<Label> humanizeHeaderLabel;
    std::unique_ptr<Label> clockHeaderLabel;
    std::unique_ptr<Label> ledHeaderLabel;  // Random drum word header

    // Data row components
    std::array<std::unique_ptr<Label>, NUM_ROWS> rowLabels;
    std::array<std::unique_ptr<PatternLED>, NUM_ROWS> leds;

    // Density knobs for all 6 channels (C2)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> densityKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> densityAttachments;

    // Chaos knobs for all 6 channels (C3)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> chaosKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> chaosAttachments;

    // Blend knobs for all 6 channels (C4) - Grids/Euclidean balance (0=Grids, 1=Euclidean)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> blendKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> blendAttachments;

    // Steps displays for all 6 channels (C5) - Euclidean pattern length (2-32)
    std::array<std::unique_ptr<ScrollableNumberDisplay>, NUM_ROWS> stepsDisplays;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> stepsAttachments;

    // Pulses displays for all 6 channels (C6) - Euclidean pulse count (0 to steps)
    std::array<std::unique_ptr<ScrollableNumberDisplay>, NUM_ROWS> pulsesDisplays;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> pulsesAttachments;

    // Start On displays for all 6 channels (C7) - Euclidean start step (1 to steps)
    std::array<std::unique_ptr<ScrollableNumberDisplay>, NUM_ROWS> startOnDisplays;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> startOnAttachments;

    // Swing knobs for all 6 channels (C8) - bipolar (-99 to +99)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> swingKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> swingAttachments;

    // Shift knobs for all 6 channels (C9) - bipolar (0-126, 63=center/off)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> shiftKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> shiftAttachments;

    // Humanize knobs for all 6 channels (C10) - timing randomization (0-127)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> humanizeKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> humanizeAttachments;

    // Clock displays for all 6 channels (C11) - clock ratio (/8 to x8)
    std::array<std::unique_ptr<ScrollableClockDisplay>, NUM_ROWS> clockDisplays;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> clockAttachments;

    // Processor reference for manual note triggering (clickable LEDs)
    AugmaticGREProcessor& audioProcessor;
};

// Velocity Bender waveform display - shows the accent bender curve with grid background
class VelocityBenderDisplay : public juce::Component, private juce::Timer
{
public:
    VelocityBenderDisplay(AccentBenderController& controller)
        : accentController(controller)
    {
        startTimerHz(30);  // Update at 30 FPS
        updateWaveform();
    }

    ~VelocityBenderDisplay() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        // Background - same as grid background
        g.fillAll(juce::Colour(0xff101010));

        float width = static_cast<float>(getWidth());
        float height = static_cast<float>(getHeight());

        // Main vertical grid lines at beat divisions (4 divisions for one bar)
        g.setColour(juce::Colour(0xff595e5f));
        for (int i = 1; i < 4; ++i)
        {
            float x = (width / 4.0f) * i;
            g.drawLine(x, 0, x, height, 0.5f);
        }

        // Additional subdivision lines (8th notes)
        g.setColour(juce::Colour(0xff4a4e4f));
        for (int i = 1; i < 8; ++i)
        {
            if (i % 2 != 0)
            {
                float x = (width / 8.0f) * i;
                g.drawLine(x, 0, x, height, 0.4f);
            }
        }

        // Finer subdivision lines (16th notes)
        g.setColour(juce::Colour(0xff3a3e3f));
        for (int i = 1; i < 16; ++i)
        {
            if (i % 2 != 0)
            {
                float x = (width / 16.0f) * i;
                g.drawLine(x, 0, x, height, 0.3f);
            }
        }

        // Horizontal center line
        g.drawLine(0, height * 0.5f, width, height * 0.5f, 0.5f);

        // Draw waveform in violet
        g.setColour(juce::Colour(0xff9b74f6));
        g.strokePath(waveformPath, juce::PathStrokeType(2.0f));

        // Border
        g.setColour(juce::Colour(0xff595e5f));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        updateWaveform();
    }

private:
    void timerCallback() override
    {
        updateWaveform();
    }

    void updateWaveform()
    {
        waveformPath.clear();

        if (getWidth() <= 0 || getHeight() <= 0) return;

        const int numPoints = getWidth();
        waveformData = accentController.getWaveformForDisplay(numPoints);

        if (!waveformData.empty())
        {
            float height = static_cast<float>(getHeight());

            for (int i = 0; i < numPoints; ++i)
            {
                float x = static_cast<float>(i);
                float y = height * (1.0f - waveformData[i]);

                if (i == 0)
                    waveformPath.startNewSubPath(x, y);
                else
                    waveformPath.lineTo(x, y);
            }
        }

        repaint();
    }

    AccentBenderController& accentController;
    std::vector<float> waveformData;
    juce::Path waveformPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelocityBenderDisplay)
};

// VELOCITY Tab component - same grid structure as PATTERN tab
class VelocityTabComponent : public Component,
                             public MidiMappingManager::InstrumentNameListener
{
public:
    static constexpr int NUM_COLS = 12;
    static constexpr int NUM_ROWS = 6;
    static constexpr int MIN_CELL_SIZE = 40;
    static constexpr float HEADER_ROW_RATIO = 0.333f;

    // Row to channel mapping: BD=0, SN=1, HH=2, BD'=3, SN'=4, HH'=5
    static constexpr int ROW_TO_CHANNEL[NUM_ROWS] = {0, 1, 2, 3, 4, 5};

    VelocityTabComponent(AudioProcessorValueTreeState& apvts, AccentBenderController* accentController, AugmaticGREProcessor& proc)
        : audioProcessor(proc)
    {
        auto* mappingMgr = proc.getMidiMappingManager();

        // Create Velocity Bender display if controller is available
        if (accentController != nullptr)
        {
            velocityBenderDisplay = std::make_unique<VelocityBenderDisplay>(*accentController);
            addAndMakeVisible(velocityBenderDisplay.get());
        }

        // Row colors from colors.md: BD=Red, SN=Blue, HH=Yellow, BD'=Pink, SN'=Green, HH'=Orange
        // Store in member array for dynamic updates (SCALE/LEVEL color toggling)
        rowColours = {
            Colour(0xffff4757),  // BD - Red
            Colour(0xff5f9fec),  // SN - Blue
            Colour(0xfffeca57),  // HH - Yellow
            Colour(0xffff6b9d),  // BD' - Pink
            Colour(0xff1dd1a1),  // SN' - Green
            Colour(0xffff9f43)   // HH' - Orange
        };

        // Channel names for parameter IDs (matches CHANNEL_NAMES in Constants.h)
        const char* channelNames[] = {"bd", "sn", "hh", "bd_acc", "sn_acc", "hh_acc"};

        // Header labels
        levelHeaderLabel = std::make_unique<Label>("levelHeader", "LEVEL");
        levelHeaderLabel->setJustificationType(Justification::centred);
        levelHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(levelHeaderLabel.get());

        randomHeaderLabel = std::make_unique<Label>("randomHeader", "RANDOM");
        randomHeaderLabel->setJustificationType(Justification::centred);
        randomHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(randomHeaderLabel.get());

        benderHeaderLabel = std::make_unique<Label>("benderHeader", "BENDER");
        benderHeaderLabel->setJustificationType(Justification::centred);
        benderHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(benderHeaderLabel.get());

        // VB beat division header labels (C7-C10)
        const std::array<String, 4> vbLabelTexts = {"2N", "4N", "4NT", "8N"};
        for (int i = 0; i < 4; ++i)
        {
            vbHeaderLabels[i] = std::make_unique<Label>("vbHeader" + String(i), vbLabelTexts[i]);
            vbHeaderLabels[i]->setJustificationType(Justification::centred);
            vbHeaderLabels[i]->setColour(Label::textColourId, Colour(0xffb0b0b0));
            addAndMakeVisible(vbHeaderLabels[i].get());
        }

        // LED column header - displays random drum word
        ledHeaderLabel = std::make_unique<Label>("ledHeader", getRandomDrumWord());
        ledHeaderLabel->setJustificationType(Justification::centred);
        ledHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(ledHeaderLabel.get());

        // First pass: Create ALL components (knobs must exist before toggle callbacks fire)
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            // Create row label (column 1) with row-specific color
            juce::String labelText = mappingMgr ? mappingMgr->getInstrumentName(row) : juce::String(MidiMappingManager::DEFAULT_INSTRUMENT_NAMES[row]);
            rowLabels[row] = std::make_unique<Label>("rowLabel" + String(row), labelText);
            rowLabels[row]->setJustificationType(Justification::centred);
            rowLabels[row]->setFont(Font(28.0f, Font::plain));
            rowLabels[row]->setColour(Label::textColourId, rowColours[row]);
            addAndMakeVisible(rowLabels[row].get());

            // Create LED indicator (column 12)
            // v0.4.087: Clickable - triggers MIDI note on click
            leds[row] = std::make_unique<PatternLED>();
            leds[row]->setLEDColour(rowColours[row]);
            leds[row]->onClicked = [this, row]() {
                int channelIdx = ROW_TO_CHANNEL[row];
                audioProcessor.triggerNoteFromUI(channelIdx, 100);
                updateRandomDrumWord();
            };
            addAndMakeVisible(leds[row].get());

            // Create LEVEL knob (column 2) - vel_value parameter (1 to 127)
            levelKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            levelKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            levelKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            levelKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            levelKnobs[row]->setLookAndFeel(&knobLookAndFeel);
            addAndMakeVisible(levelKnobs[row].get());

            // Create RANDOM knob (column 5) - vel_randomize parameter (0 to 100)
            randomKnobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            randomKnobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            randomKnobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            randomKnobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            randomKnobs[row]->setLookAndFeel(&knobLookAndFeel);
            addAndMakeVisible(randomKnobs[row].get());

            // Create BENDER button (column 6) - velocity bender instrument enable
            benderButtons[row] = std::make_unique<VelocityBenderButton>();
            benderButtons[row]->setButtonColour(rowColours[row]);
            addAndMakeVisible(benderButtons[row].get());
        }

        // Second pass: Set up parameter attachments (callbacks may fire during attachment creation)
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            // Map row index to channel index for parameter names
            int channelIdx = ROW_TO_CHANNEL[row];

            // LEVEL attachment
            String levelParamId = String(channelNames[channelIdx]) + "_vel_value";
            levelAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, levelParamId, *levelKnobs[row]);
            levelKnobs[row]->setDoubleClickReturnValue(true, 120.0);  // Default value = 120

            // RANDOM attachment
            String randomParamId = String(channelNames[channelIdx]) + "_vel_randomize";
            randomAttachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, randomParamId, *randomKnobs[row]);
            randomKnobs[row]->setDoubleClickReturnValue(true, 0.0);  // Default randomize = 0

            // BENDER button attachment - sync callback and parameter attachment
            benderButtons[row]->getToggleButton().onStateChange = [this, row]() {
                benderButtons[row]->syncWithToggleButton();
            };
            String benderParamId = "accent_bender_instrument_" + String(channelNames[channelIdx]);
            benderAttachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
                apvts, benderParamId, benderButtons[row]->getToggleButton());
            benderButtons[row]->syncWithToggleButton();
        }

        // Create VB (Velocity Bender) knobs in R2 (first data row), C7-C10
        // These are bipolar knobs for 2N, 4N, 4NT, 8N beat divisions
        const juce::Colour vbPurple(0xff9b74f6);  // Same purple as VB visualizer
        const std::array<String, 4> vbParamIDs = {
            "accent_bender_slider_2n", "accent_bender_slider_4n",
            "accent_bender_slider_4nt", "accent_bender_slider_8n"
        };

        for (int i = 0; i < 4; ++i)
        {
            vbKnobs[i] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            vbKnobs[i]->setColour(Slider::rotarySliderFillColourId, vbPurple);
            vbKnobs[i]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            vbKnobs[i]->setColour(Slider::thumbColourId, vbPurple);
            vbKnobs[i]->setLookAndFeel(&bipolarKnobLookAndFeel);
            addAndMakeVisible(vbKnobs[i].get());

            vbAttachments[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                apvts, vbParamIDs[i], *vbKnobs[i]);
            vbKnobs[i]->setDoubleClickReturnValue(true, 0.0);  // Default = center (no modulation)

            // Add listener to update Random/Reset label and AccentBenderController when knob value changes
            vbKnobs[i]->onValueChange = [this, i]() {
                updateVBRandomResetLabel();
                // Directly update AccentBenderController for immediate waveform display refresh (v0.4.123)
                if (auto* abc = audioProcessor.getAccentBenderController())
                    abc->setSliderValue(i, static_cast<float>(vbKnobs[i]->getValue()));
            };
        }

        // Create VB Random/Reset button (R2, C10-C11)
        vbRandomResetButton = std::make_unique<VBRandomResetButton>();
        vbRandomResetButton->onClick = [this]() {
            juce::Random random;
            if (areAllVBKnobsAtCenter())
            {
                // Randomize all VB knobs
                for (int i = 0; i < 4; ++i)
                {
                    float value = random.nextFloat() * 2.0f - 1.0f;  // -1.0 to 1.0
                    vbKnobs[i]->setValue(value, juce::sendNotificationAsync);
                }
            }
            else
            {
                // Reset all VB knobs to center
                for (int i = 0; i < 4; ++i)
                {
                    vbKnobs[i]->setValue(0.0, juce::sendNotificationAsync);
                }
            }
            updateVBRandomResetLabel();
        };
        addAndMakeVisible(vbRandomResetButton.get());

        // Initialize label based on current VB state
        updateVBRandomResetLabel();

        // Register instrument name listener
        if (mappingMgr)
            mappingMgr->addInstrumentNameListener(this);
    }

    // InstrumentNameListener callback
    void instrumentNamesChanged() override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<VelocityTabComponent>(this)]() {
            if (safeThis == nullptr) return;
            if (auto* mgr = safeThis->audioProcessor.getMidiMappingManager())
            {
                for (int row = 0; row < NUM_ROWS; ++row)
                    safeThis->rowLabels[row]->setText(mgr->getInstrumentName(row), dontSendNotification);
            }
        });
    }

    ~VelocityTabComponent() override
    {
        // Remove instrument name listener
        if (auto* mappingMgr = audioProcessor.getMidiMappingManager())
            mappingMgr->removeInstrumentNameListener(this);

        // Clear LookAndFeel before destroying knobs
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            if (levelKnobs[row])
                levelKnobs[row]->setLookAndFeel(nullptr);
            if (randomKnobs[row])
                randomKnobs[row]->setLookAndFeel(nullptr);
        }
        // Clear LookAndFeel for VB knobs
        for (int i = 0; i < 4; ++i)
        {
            if (vbKnobs[i])
                vbKnobs[i]->setLookAndFeel(nullptr);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Calculate cell sizes
        int cellWidth = std::max(MIN_CELL_SIZE, bounds.getWidth() / NUM_COLS);
        float totalRowUnits = HEADER_ROW_RATIO + static_cast<float>(NUM_ROWS);
        int cellHeight = std::max(MIN_CELL_SIZE, static_cast<int>(bounds.getHeight() / totalRowUnits));
        int headerHeight = static_cast<int>(cellHeight * HEADER_ROW_RATIO);
        int dataRowStartY = headerHeight;

        // Position header labels
        float headerFontSize = std::max(11.0f, static_cast<float>(headerHeight) * 0.55f);
        Font headerFont(headerFontSize, Font::plain);

        levelHeaderLabel->setBounds(1 * cellWidth, 0, cellWidth, headerHeight);   // C2
        levelHeaderLabel->setFont(headerFont);

        randomHeaderLabel->setBounds(2 * cellWidth, 0, cellWidth, headerHeight);  // C3
        randomHeaderLabel->setFont(headerFont);

        benderHeaderLabel->setBounds(3 * cellWidth, 0, cellWidth, headerHeight);  // C4
        benderHeaderLabel->setFont(headerFont);

        // VB beat division header labels (C6-C9)
        for (int i = 0; i < 4; ++i)
        {
            int colIndex = 5 + i;  // C6, C7, C8, C9 (indices 5, 6, 7, 8)
            vbHeaderLabels[i]->setBounds(colIndex * cellWidth, 0, cellWidth, headerHeight);
            vbHeaderLabels[i]->setFont(headerFont);
        }

        // LED column header (C12, index 11)
        ledHeaderLabel->setBounds(11 * cellWidth, 0, cellWidth, headerHeight);
        ledHeaderLabel->setFont(headerFont);

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            int rowY = dataRowStartY + row * cellHeight;

            // Position label in column 1 (index 0) with scalable font
            rowLabels[row]->setBounds(0, rowY, cellWidth, cellHeight);
            float fontSize = static_cast<float>(cellHeight) * 0.38f;
            rowLabels[row]->setFont(Font(fontSize, Font::plain));

            // Calculate LED/toggle size (same scaling logic as PATTERN tab)
            int ledSize;
            if (bounds.getWidth() <= 900) {
                ledSize = 25;
            } else {
                int knobMaxSize = std::min(cellWidth, cellHeight) - 10;
                int maxSize = static_cast<int>(knobMaxSize * 0.6f);
                float progress = static_cast<float>(bounds.getWidth() - kResponsiveBreakpointStart) / kResponsiveBreakpointRange;
                progress = std::min(1.0f, progress);
                ledSize = 25 + static_cast<int>((maxSize - 25) * progress);
            }

            // Position LED in column 12 (index 11)
            int ledX = 11 * cellWidth + (cellWidth - ledSize) / 2;
            int ledY = rowY + (cellHeight - ledSize) / 2;
            leds[row]->setBounds(ledX, ledY, ledSize, ledSize);

            // Knob sizing (same as PATTERN tab)
            int knobPadding = 5;
            int knobSize = std::min(cellWidth, cellHeight) - knobPadding * 2;
            int knobY = rowY + (cellHeight - knobSize) / 2;

            // Position LEVEL knob in column 2 (index 1)
            int levelKnobX = 1 * cellWidth + (cellWidth - knobSize) / 2;
            levelKnobs[row]->setBounds(levelKnobX, knobY, knobSize, knobSize);

            // Position RANDOM knob in column 3 (index 2)
            int randomKnobX = 2 * cellWidth + (cellWidth - knobSize) / 2;
            randomKnobs[row]->setBounds(randomKnobX, knobY, knobSize, knobSize);

            // Position BENDER button in column 4 (index 3) - 90% of knob size
            int benderSize = static_cast<int>(knobSize * 0.9f);
            int benderX = 3 * cellWidth + (cellWidth - benderSize) / 2;
            int benderY = rowY + (cellHeight - benderSize) / 2;
            benderButtons[row]->setBounds(benderX, benderY, benderSize, benderSize);
        }

        // Position VB (Velocity Bender) knobs in R2 (first data row), C6-C9 (indices 5-8)
        {
            int knobPadding = 5;
            int vbKnobSize = std::min(cellWidth, cellHeight) - knobPadding * 2;
            int vbRowY = dataRowStartY;  // First data row (R2/BD row)
            int vbKnobY = vbRowY + (cellHeight - vbKnobSize) / 2;

            for (int i = 0; i < 4; ++i)
            {
                int colIndex = 5 + i;  // C6, C7, C8, C9 (indices 5, 6, 7, 8)
                int vbKnobX = colIndex * cellWidth + (cellWidth - vbKnobSize) / 2;
                vbKnobs[i]->setBounds(vbKnobX, vbKnobY, vbKnobSize, vbKnobSize);
            }

            // Position VB Random/Reset button in R2, C10-C11 (indices 9-10) — spans 2 columns
            int vbButtonHeight = static_cast<int>(vbKnobSize * 0.9f);
            int vbButtonX = 9 * cellWidth;
            int vbButtonWidth = 2 * cellWidth;
            int vbButtonY = vbRowY + (cellHeight - vbButtonHeight) / 2;
            vbRandomResetButton->setBounds(vbButtonX, vbButtonY, vbButtonWidth, vbButtonHeight);
        }

        // Position Velocity Bender display in C6-R3 to C11-R7 (columns 5-10, rows 2-6)
        if (velocityBenderDisplay)
        {
            const int vbPadding = 10;  // 10px padding around VB display
            int displayX = 5 * cellWidth + vbPadding;  // C6 (index 5) + padding
            int displayY = dataRowStartY + cellHeight + vbPadding;  // Start at second data row (R3) + padding
            int displayWidth = 6 * cellWidth - vbPadding * 2;  // C6 to C11 (6 columns) - padding on both sides
            int displayHeight = (NUM_ROWS - 1) * cellHeight - vbPadding * 2;  // 5 data rows (R3-R7) - padding on top/bottom
            velocityBenderDisplay->setBounds(displayX, displayY, displayWidth, displayHeight);
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff101010));

        // Calculate cell sizes
        auto bounds = getLocalBounds();
        int cellWidth = std::max(MIN_CELL_SIZE, bounds.getWidth() / NUM_COLS);
        float totalRowUnits = HEADER_ROW_RATIO + static_cast<float>(NUM_ROWS);
        int cellHeight = std::max(MIN_CELL_SIZE, static_cast<int>(bounds.getHeight() / totalRowUnits));
        int headerHeight = static_cast<int>(cellHeight * HEADER_ROW_RATIO);
        int dataRowStartY = headerHeight;

        // Grid lines - same as background (invisible) to match PATTERN tab
        g.setColour(Colour(0xff101010));

        // Vertical lines (full height)
        for (int col = 1; col < NUM_COLS; ++col)
            g.drawVerticalLine(col * cellWidth, 0.0f, static_cast<float>(bounds.getHeight()));

        // Horizontal line below header row
        g.drawHorizontalLine(headerHeight, 0.0f, static_cast<float>(bounds.getWidth()));

        // Horizontal lines for data rows
        for (int row = 1; row < NUM_ROWS; ++row)
            g.drawHorizontalLine(dataRowStartY + row * cellHeight, 0.0f, static_cast<float>(bounds.getWidth()));
    }

    // Trigger LED by row index
    void triggerLED(int rowIndex)
    {
        if (rowIndex >= 0 && rowIndex < NUM_ROWS && leds[rowIndex])
            leds[rowIndex]->trigger();
    }

    void triggerLEDForChannel(int channelIndex)
    {
        static const int channelToRow[] = {0, 1, 2, 3, 4, 5};
        if (channelIndex >= 0 && channelIndex < 6)
            triggerLED(channelToRow[channelIndex]);
    }

    // Update LED header with random drum word (called on tab switch)
    void updateRandomDrumWord()
    {
        if (ledHeaderLabel)
            ledHeaderLabel->setText(getRandomDrumWord(), juce::dontSendNotification);
    }

private:
    // Check if all VB knobs are at center position (0.0)
    bool areAllVBKnobsAtCenter() const
    {
        const float tolerance = 0.001f;
        for (int i = 0; i < 4; ++i)
        {
            if (vbKnobs[i] && std::abs(vbKnobs[i]->getValue()) > tolerance)
                return false;
        }
        return true;
    }

    // Update the VB Random/Reset button text based on knob positions
    void updateVBRandomResetLabel()
    {
        if (vbRandomResetButton)
        {
            if (areAllVBKnobsAtCenter())
                vbRandomResetButton->setText("RANDOM");
            else
                vbRandomResetButton->setText("RESET");
        }
    }

    // Custom LookAndFeel for knobs
    PatternKnobLookAndFeel knobLookAndFeel;
    BipolarKnobLookAndFeel bipolarKnobLookAndFeel;  // For VB knobs (center = 0)

    // Row colors stored for dynamic updates
    std::array<juce::Colour, NUM_ROWS> rowColours;

    // Header labels
    std::unique_ptr<Label> levelHeaderLabel;
    std::unique_ptr<Label> randomHeaderLabel;
    std::unique_ptr<Label> benderHeaderLabel;
    std::array<std::unique_ptr<Label>, 4> vbHeaderLabels;  // 2N, 4N, 4NT, 8N (C6-C9)
    std::unique_ptr<Label> ledHeaderLabel;  // Random drum word header

    // Row labels and LEDs
    std::array<std::unique_ptr<Label>, NUM_ROWS> rowLabels;
    std::array<std::unique_ptr<PatternLED>, NUM_ROWS> leds;

    // LEVEL knobs (C2) - velocity level (1 to 127)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> levelKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> levelAttachments;

    // RANDOM knobs (C3) - velocity randomization (0 to 100)
    std::array<std::unique_ptr<Slider>, NUM_ROWS> randomKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> randomAttachments;

    // BENDER buttons (C4) - velocity bender instrument enable
    std::array<std::unique_ptr<VelocityBenderButton>, NUM_ROWS> benderButtons;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_ROWS> benderAttachments;

    // VB knobs (C6-C9, R2 only) - velocity bender beat division controls (2N, 4N, 4NT, 8N)
    std::array<std::unique_ptr<Slider>, 4> vbKnobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 4> vbAttachments;

    // VB Random/Reset button (C10-C11)
    std::unique_ptr<VBRandomResetButton> vbRandomResetButton;

    // Velocity Bender display (C6-C11, R3-R7)
    std::unique_ptr<VelocityBenderDisplay> velocityBenderDisplay;

    // Processor reference for manual note triggering (clickable LEDs)
    AugmaticGREProcessor& audioProcessor;
};

// MIX Tab component - grid with PROB knobs, MUTE buttons, and LEDs
class MixTabComponent : public Component,
                        public AudioProcessorValueTreeState::Listener,
                        public MidiMappingManager::InstrumentNameListener
{
public:
    static constexpr int NUM_COLS = 12;
    static constexpr int NUM_ROWS = 6;
    static constexpr int MIN_CELL_SIZE = 40;
    static constexpr float HEADER_ROW_RATIO = 0.333f;

    // Row to channel mapping: BD=0, SN=1, HH=2, BD'=3, SN'=4, HH'=5
    static constexpr int rowToChannel[NUM_ROWS] = {0, 1, 2, 3, 4, 5};

    // Priority Matrix constants
    static constexpr int NUM_PRIORITIES = 6;

    MixTabComponent(AudioProcessorValueTreeState& apvts, AugmaticGREProcessor& proc)
        : parameters(apvts), audioProcessor(proc)
    {
        auto* mappingMgr = proc.getMidiMappingManager();

        // Header labels - left group (C2, C3, C4)
        probHeader1 = std::make_unique<Label>("probHeader1", "PROB");
        probHeader1->setJustificationType(Justification::centred);
        probHeader1->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(probHeader1.get());

        muteHeader1 = std::make_unique<Label>("muteHeader1", "MUTE");
        muteHeader1->setJustificationType(Justification::centred);
        muteHeader1->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(muteHeader1.get());

        soloHeader1 = std::make_unique<Label>("soloHeader1", "SOLO");
        soloHeader1->setJustificationType(Justification::centred);
        soloHeader1->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(soloHeader1.get());

        // CENTER header - LINEAR DRUMMING (spans C5-C8, indices 4-7)
        linearDrummingHeader = std::make_unique<Label>("linearDrummingHeader", "LINEAR DRUMMING");
        linearDrummingHeader->setJustificationType(Justification::centred);
        linearDrummingHeader->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(linearDrummingHeader.get());

        // Header labels - right group (C9, C10, C11)
        probHeader2 = std::make_unique<Label>("probHeader2", "PROB");
        probHeader2->setJustificationType(Justification::centred);
        probHeader2->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(probHeader2.get());

        muteHeader2 = std::make_unique<Label>("muteHeader2", "MUTE");
        muteHeader2->setJustificationType(Justification::centred);
        muteHeader2->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(muteHeader2.get());

        soloHeader2 = std::make_unique<Label>("soloHeader2", "SOLO");
        soloHeader2->setJustificationType(Justification::centred);
        soloHeader2->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(soloHeader2.get());

        // LED column header - displays random drum word
        ledHeaderLabel = std::make_unique<Label>("ledHeader", getRandomDrumWord());
        ledHeaderLabel->setJustificationType(Justification::centred);
        ledHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(ledHeaderLabel.get());

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            // Row label (column 1) — names from MidiMappingManager
            juce::String labelText = mappingMgr ? mappingMgr->getInstrumentName(row) : juce::String(MidiMappingManager::DEFAULT_INSTRUMENT_NAMES[row]);
            rowLabels[row] = std::make_unique<Label>("rowLabel" + String(row), labelText);
            rowLabels[row]->setJustificationType(Justification::centred);
            rowLabels[row]->setFont(Font(28.0f, Font::plain));
            rowLabels[row]->setColour(Label::textColourId, rowColours[row]);
            addAndMakeVisible(rowLabels[row].get());

            // LED indicator (column 12)
            // v0.4.087: Clickable - triggers MIDI note on click
            leds[row] = std::make_unique<PatternLED>();
            leds[row]->setLEDColour(rowColours[row]);
            leds[row]->onClicked = [this, row]() {
                int channelIdx = rowToChannel[row];
                audioProcessor.triggerNoteFromUI(channelIdx, 100);
                updateRandomDrumWord();
            };
            addAndMakeVisible(leds[row].get());

            // Get channel index for this row
            int channelIdx = rowToChannel[row];

            // P1 knob (column 2 - PROB left) - linked to probability_pre
            p1Knobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            p1Knobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            p1Knobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            p1Knobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            p1Knobs[row]->setLookAndFeel(&knobLookAndFeel);
            addAndMakeVisible(p1Knobs[row].get());

            String p1ParamId = String(CHANNEL_NAMES[channelIdx]) + PARAMETER_SEPARATOR + PROBABILITY_PRE_SUFFIX;
            p1Attachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                parameters, p1ParamId, *p1Knobs[row]);
            p1Knobs[row]->setDoubleClickReturnValue(true, 100.0);  // Default 100%

            // M1 mute button (column 3 - MUTE left) - linked to mute_pre
            m1Buttons[row] = std::make_unique<MixMuteButton>();
            m1Buttons[row]->setButtonColour(rowColours[row]);
            addAndMakeVisible(m1Buttons[row].get());

            String m1ParamId = String(CHANNEL_NAMES[channelIdx]) + PARAMETER_SEPARATOR + MUTE_PRE_SUFFIX;
            m1Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
                parameters, m1ParamId, m1Buttons[row]->getToggleButton());
            m1Buttons[row]->syncWithToggleButton();

            // S1 solo button (column 4 - SOLO left) - linked to solo_pre
            s1Buttons[row] = std::make_unique<MixSoloButton>();
            s1Buttons[row]->setButtonColour(rowColours[row]);
            addAndMakeVisible(s1Buttons[row].get());

            String s1ParamId = String(CHANNEL_NAMES[channelIdx]) + PARAMETER_SEPARATOR + SOLO_PRE_SUFFIX;
            s1Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
                parameters, s1ParamId, s1Buttons[row]->getToggleButton());
            s1Buttons[row]->syncWithToggleButton();

            // Mutual exclusivity: M1 and S1 cannot both be ON (new feature for MIX tab)
            m1Buttons[row]->getToggleButton().onClick = [this, row]() {
                if (m1Buttons[row]->getToggleButton().getToggleState() && s1Buttons[row]) {
                    s1Buttons[row]->getToggleButton().setToggleState(false, sendNotificationSync);
                }
            };
            s1Buttons[row]->getToggleButton().onClick = [this, row]() {
                if (s1Buttons[row]->getToggleButton().getToggleState() && m1Buttons[row]) {
                    m1Buttons[row]->getToggleButton().setToggleState(false, sendNotificationSync);
                }
            };

            // P2 knob (column 9 - PROB right) - linked to probability_post
            p2Knobs[row] = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            p2Knobs[row]->setColour(Slider::rotarySliderFillColourId, rowColours[row]);
            p2Knobs[row]->setColour(Slider::rotarySliderOutlineColourId, Colour(0xff3a3a3a));
            p2Knobs[row]->setColour(Slider::thumbColourId, rowColours[row]);
            p2Knobs[row]->setLookAndFeel(&knobLookAndFeel);
            addAndMakeVisible(p2Knobs[row].get());

            String p2ParamId = String(CHANNEL_NAMES[channelIdx]) + PARAMETER_SEPARATOR + PROBABILITY_POST_SUFFIX;
            p2Attachments[row] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                parameters, p2ParamId, *p2Knobs[row]);
            p2Knobs[row]->setDoubleClickReturnValue(true, 100.0);  // Default 100%

            // M2 mute button (column 10 - MUTE right) - linked to mute_post
            m2Buttons[row] = std::make_unique<MixMuteButton>();
            m2Buttons[row]->setButtonColour(rowColours[row]);
            addAndMakeVisible(m2Buttons[row].get());

            String m2ParamId = String(CHANNEL_NAMES[channelIdx]) + PARAMETER_SEPARATOR + MUTE_POST_SUFFIX;
            m2Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
                parameters, m2ParamId, m2Buttons[row]->getToggleButton());
            m2Buttons[row]->syncWithToggleButton();

            // S2 solo button (column 11 - SOLO right) - linked to solo_post
            s2Buttons[row] = std::make_unique<MixSoloButton>();
            s2Buttons[row]->setButtonColour(rowColours[row]);
            addAndMakeVisible(s2Buttons[row].get());

            String s2ParamId = String(CHANNEL_NAMES[channelIdx]) + PARAMETER_SEPARATOR + SOLO_POST_SUFFIX;
            s2Attachments[row] = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
                parameters, s2ParamId, s2Buttons[row]->getToggleButton());
            s2Buttons[row]->syncWithToggleButton();

            // Mutual exclusivity: M2 and S2 cannot both be ON (new feature for MIX tab)
            m2Buttons[row]->getToggleButton().onClick = [this, row]() {
                if (m2Buttons[row]->getToggleButton().getToggleState() && s2Buttons[row]) {
                    s2Buttons[row]->getToggleButton().setToggleState(false, sendNotificationSync);
                }
            };
            s2Buttons[row]->getToggleButton().onClick = [this, row]() {
                if (s2Buttons[row]->getToggleButton().getToggleState() && m2Buttons[row]) {
                    m2Buttons[row]->getToggleButton().setToggleState(false, sendNotificationSync);
                }
            };
        }

        // Initialize Priority Matrix (6x6 LED-style buttons in LINEAR DRUMMING area)
        initializePriorityMatrix();

        // Listen for external parameter changes (preset loads, DAW automation)
        // so the priority matrix UI stays in sync
        for (const auto& paramId : priorityParamIDs)
            parameters.addParameterListener(paramId, this);

        // Register instrument name listener
        if (mappingMgr)
            mappingMgr->addInstrumentNameListener(this);
    }

    // InstrumentNameListener callback
    void instrumentNamesChanged() override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MixTabComponent>(this)]() {
            if (safeThis == nullptr) return;
            if (auto* mgr = safeThis->audioProcessor.getMidiMappingManager())
            {
                for (int row = 0; row < NUM_ROWS; ++row)
                    safeThis->rowLabels[row]->setText(mgr->getInstrumentName(row), dontSendNotification);
            }
        });
    }

    ~MixTabComponent() override
    {
        // Remove instrument name listener
        if (auto* mappingMgr = audioProcessor.getMidiMappingManager())
            mappingMgr->removeInstrumentNameListener(this);

        // Remove parameter listeners
        for (const auto& paramId : priorityParamIDs)
            parameters.removeParameterListener(paramId, this);

        // Clear look and feel before destroying sliders
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            if (p1Knobs[row]) p1Knobs[row]->setLookAndFeel(nullptr);
            if (p2Knobs[row]) p2Knobs[row]->setLookAndFeel(nullptr);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        int cellWidth = std::max(MIN_CELL_SIZE, bounds.getWidth() / NUM_COLS);
        float totalRowUnits = HEADER_ROW_RATIO + static_cast<float>(NUM_ROWS);
        int cellHeight = std::max(MIN_CELL_SIZE, static_cast<int>(bounds.getHeight() / totalRowUnits));
        int headerHeight = static_cast<int>(cellHeight * HEADER_ROW_RATIO);

        // Header font size
        float headerFontSize = std::max(11.0f, static_cast<float>(headerHeight) * 0.55f);
        Font headerFont(headerFontSize, Font::plain);

        // Position left group headers (C2=index 1, C3=index 2, C4=index 3)
        probHeader1->setBounds(1 * cellWidth, 0, cellWidth, headerHeight);
        probHeader1->setFont(headerFont);
        muteHeader1->setBounds(2 * cellWidth, 0, cellWidth, headerHeight);
        muteHeader1->setFont(headerFont);
        soloHeader1->setBounds(3 * cellWidth, 0, cellWidth, headerHeight);
        soloHeader1->setFont(headerFont);

        // Position LINEAR DRUMMING header spanning C5-C8 (indices 4-7, width = 4 cells)
        linearDrummingHeader->setBounds(4 * cellWidth, 0, 4 * cellWidth, headerHeight);
        linearDrummingHeader->setFont(headerFont);

        // Position right group headers (C9=index 8, C10=index 9, C11=index 10)
        probHeader2->setBounds(8 * cellWidth, 0, cellWidth, headerHeight);
        probHeader2->setFont(headerFont);
        muteHeader2->setBounds(9 * cellWidth, 0, cellWidth, headerHeight);
        muteHeader2->setFont(headerFont);
        soloHeader2->setBounds(10 * cellWidth, 0, cellWidth, headerHeight);
        soloHeader2->setFont(headerFont);

        // LED column header (C12, index 11)
        ledHeaderLabel->setBounds(11 * cellWidth, 0, cellWidth, headerHeight);
        ledHeaderLabel->setFont(headerFont);

        // Position data rows below header
        int dataRowStartY = headerHeight;

        // Calculate knob/button size (same logic as PATTERN tab)
        int knobMaxSize = std::min(cellWidth, cellHeight) - 10;

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            int rowY = dataRowStartY + row * cellHeight;

            // Row label in column 1 (index 0)
            rowLabels[row]->setBounds(0, rowY, cellWidth, cellHeight);
            float fontSize = static_cast<float>(cellHeight) * 0.38f;
            rowLabels[row]->setFont(Font(fontSize, Font::plain));

            // P1 knob in column 2 (index 1)
            int knobX1 = 1 * cellWidth + (cellWidth - knobMaxSize) / 2;
            int knobY = rowY + (cellHeight - knobMaxSize) / 2;
            p1Knobs[row]->setBounds(knobX1, knobY, knobMaxSize, knobMaxSize);

            // M1 button in column 3 (index 2)
            int buttonSize = static_cast<int>(knobMaxSize * 0.9f);  // Same size as BENDER buttons
            int buttonX1 = 2 * cellWidth + (cellWidth - buttonSize) / 2;
            int buttonY = rowY + (cellHeight - buttonSize) / 2;
            m1Buttons[row]->setBounds(buttonX1, buttonY, buttonSize, buttonSize);

            // S1 button in column 4 (index 3)
            int s1ButtonX = 3 * cellWidth + (cellWidth - buttonSize) / 2;
            s1Buttons[row]->setBounds(s1ButtonX, buttonY, buttonSize, buttonSize);

            // P2 knob in column 9 (index 8)
            int knobX2 = 8 * cellWidth + (cellWidth - knobMaxSize) / 2;
            p2Knobs[row]->setBounds(knobX2, knobY, knobMaxSize, knobMaxSize);

            // M2 button in column 10 (index 9)
            int buttonX2 = 9 * cellWidth + (cellWidth - buttonSize) / 2;
            m2Buttons[row]->setBounds(buttonX2, buttonY, buttonSize, buttonSize);

            // S2 button in column 11 (index 10)
            int s2ButtonX = 10 * cellWidth + (cellWidth - buttonSize) / 2;
            s2Buttons[row]->setBounds(s2ButtonX, buttonY, buttonSize, buttonSize);

            // LED in column 12 (index 11)
            int ledSize;
            if (bounds.getWidth() <= 900) {
                ledSize = 25;
            } else {
                int maxSize = static_cast<int>(knobMaxSize * 0.6f);
                float progress = static_cast<float>(bounds.getWidth() - kResponsiveBreakpointStart) / kResponsiveBreakpointRange;
                progress = std::min(1.0f, progress);
                ledSize = 25 + static_cast<int>((maxSize - 25) * progress);
            }
            int ledX = 11 * cellWidth + (cellWidth - ledSize) / 2;
            int ledY = rowY + (cellHeight - ledSize) / 2;
            leds[row]->setBounds(ledX, ledY, ledSize, ledSize);
        }

        // Position Priority Matrix in LINEAR DRUMMING area (columns 4-7, 6 rows)
        // The merged area spans 4 columns but contains 6 priority columns
        int matrixAreaX = 4 * cellWidth;
        int matrixAreaWidth = 4 * cellWidth;
        int matrixAreaY = dataRowStartY;
        int matrixAreaHeight = NUM_ROWS * cellHeight;

        // Calculate internal grid for 6x6 matrix within the merged area
        int priorityCellWidth = matrixAreaWidth / NUM_PRIORITIES;
        int priorityCellHeight = matrixAreaHeight / NUM_ROWS;

        // LED size for priority buttons - same minimum size as column LEDs (25px)
        int priorityLedSize;
        const int minLedSize = 25;  // Same as column LEDs
        if (bounds.getWidth() <= 900) {
            priorityLedSize = minLedSize;
        } else {
            int maxPriorityLedSize = std::min(priorityCellWidth, priorityCellHeight) - 6;
            float progress = static_cast<float>(bounds.getWidth() - kResponsiveBreakpointStart) / kResponsiveBreakpointRange;
            progress = std::min(1.0f, progress);
            priorityLedSize = minLedSize + static_cast<int>((maxPriorityLedSize - minLedSize) * progress);
        }
        priorityLedSize = std::max(minLedSize, std::min(priorityLedSize, std::min(priorityCellWidth, priorityCellHeight) - 4));

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            for (int priority = 0; priority < NUM_PRIORITIES; ++priority)
            {
                int buttonX = matrixAreaX + priority * priorityCellWidth + (priorityCellWidth - priorityLedSize) / 2;
                int buttonY = matrixAreaY + row * priorityCellHeight + (priorityCellHeight - priorityLedSize) / 2;
                priorityButtons[row][priority]->setBounds(buttonX, buttonY, priorityLedSize, priorityLedSize);
            }
        }
    }

    void paint(Graphics& g) override
    {
        // Fill background only - no grid lines, matching PATTERN and VELOCITY tabs
        g.fillAll(Colour(0xff101010));
    }

    // Trigger LED by row index
    void triggerLED(int rowIndex)
    {
        if (rowIndex >= 0 && rowIndex < NUM_ROWS && leds[rowIndex])
            leds[rowIndex]->trigger();
    }

    // Map channel index to row (same mapping as PATTERN tab)
    void triggerLEDForChannel(int channelIndex)
    {
        static const int channelToRow[] = {0, 1, 2, 3, 4, 5};
        if (channelIndex >= 0 && channelIndex < 6)
            triggerLED(channelToRow[channelIndex]);
    }

    // Update LED header with random drum word (called on tab switch)
    void updateRandomDrumWord()
    {
        if (ledHeaderLabel)
            ledHeaderLabel->setText(getRandomDrumWord(), juce::dontSendNotification);
    }

    // APVTS listener callback - called when linear_priority_* parameters change externally
    // (e.g. preset load, DAW automation). May be called from any thread.
    void parameterChanged(const String& /*parameterID*/, float /*newValue*/) override
    {
        // Marshal to message thread since we're updating GUI components
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MixTabComponent>(this)]() {
            if (safeThis != nullptr)
                safeThis->updatePriorityMatrixFromParameters();
        });
    }

private:
    AudioProcessorValueTreeState& parameters;

    // Instrument colors (same as PATTERN tab)
    const Colour rowColours[NUM_ROWS] = {
        Colour(0xffff4757),  // BD - Red
        Colour(0xff5f9fec),  // SN - Blue
        Colour(0xfffeca57),  // HH - Yellow
        Colour(0xffff6b9d),  // BD' - Pink
        Colour(0xff1dd1a1),  // SN' - Green
        Colour(0xffff9f43)   // HH' - Orange
    };

    // Knob look and feel
    PatternKnobLookAndFeel knobLookAndFeel;

    // Header labels - left group
    std::unique_ptr<Label> probHeader1;
    std::unique_ptr<Label> muteHeader1;
    std::unique_ptr<Label> soloHeader1;

    // Header label - center (merged C5-C8)
    std::unique_ptr<Label> linearDrummingHeader;

    // Header labels - right group
    std::unique_ptr<Label> probHeader2;
    std::unique_ptr<Label> muteHeader2;
    std::unique_ptr<Label> soloHeader2;
    std::unique_ptr<Label> ledHeaderLabel;  // Random drum word header

    // Row labels (column 1)
    std::array<std::unique_ptr<Label>, NUM_ROWS> rowLabels;

    // LED indicators (column 12)
    std::array<std::unique_ptr<PatternLED>, NUM_ROWS> leds;

    // P1 knobs (column 2) - probability_pre
    std::array<std::unique_ptr<Slider>, NUM_ROWS> p1Knobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> p1Attachments;

    // M1 mute buttons (column 3) - mute_pre
    std::array<std::unique_ptr<MixMuteButton>, NUM_ROWS> m1Buttons;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_ROWS> m1Attachments;

    // P2 knobs (column 9) - probability_post
    std::array<std::unique_ptr<Slider>, NUM_ROWS> p2Knobs;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, NUM_ROWS> p2Attachments;

    // M2 mute buttons (column 10) - mute_post
    std::array<std::unique_ptr<MixMuteButton>, NUM_ROWS> m2Buttons;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_ROWS> m2Attachments;

    // S1 solo buttons (column 4) - solo_pre
    std::array<std::unique_ptr<MixSoloButton>, NUM_ROWS> s1Buttons;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_ROWS> s1Attachments;

    // S2 solo buttons (column 11) - solo_post
    std::array<std::unique_ptr<MixSoloButton>, NUM_ROWS> s2Buttons;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, NUM_ROWS> s2Attachments;

    // Priority Matrix (6x6 LED buttons in LINEAR DRUMMING area)
    std::array<std::array<std::unique_ptr<PriorityLEDButton>, NUM_PRIORITIES>, NUM_ROWS> priorityButtons;
    std::array<int, NUM_ROWS> currentPriorityAssignments{0, 1, 2, 3, 4, 5}; // Default: each row has unique priority

    // Processor reference for cache invalidation
    AugmaticGREProcessor& audioProcessor;

    // Priority parameter IDs (same as LinearDrummingMatrix)
    const std::array<String, 6> priorityParamIDs = {
        "linear_priority_bd", "linear_priority_sn", "linear_priority_hh",
        "linear_priority_bd_acc", "linear_priority_sn_acc", "linear_priority_hh_acc"
    };

    // Initialize Priority Matrix buttons and sync with parameters
    void initializePriorityMatrix()
    {
        // Create 6x6 grid of LED buttons
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            for (int priority = 0; priority < NUM_PRIORITIES; ++priority)
            {
                priorityButtons[row][priority] = std::make_unique<PriorityLEDButton>();
                priorityButtons[row][priority]->setLEDColour(rowColours[row]);
                priorityButtons[row][priority]->onClick = [this, row, priority]() {
                    onPriorityButtonClicked(row, priority);
                };
                priorityButtons[row][priority]->onDoubleClick = [this, priority]() {
                    onPriorityColumnDoubleClicked(priority);
                };
                addAndMakeVisible(priorityButtons[row][priority].get());
            }
        }

        // Sync with current parameter values
        updatePriorityMatrixFromParameters();
    }

    // Handle priority button click (radio behavior - one per row)
    void onPriorityButtonClicked(int row, int priority)
    {
        // Only respond if clicking a different priority
        if (currentPriorityAssignments[row] == priority)
            return;

        // Update assignment
        currentPriorityAssignments[row] = priority;

        // Update visual state
        updatePriorityButtonStates();

        // Update parameter
        updatePriorityParameterForRow(row);
    }

    // Handle double-click on priority button (select all in column)
    void onPriorityColumnDoubleClicked(int priority)
    {
        // Set all rows to this priority
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            currentPriorityAssignments[row] = priority;
        }

        // Update visual state
        updatePriorityButtonStates();

        // Update all parameters
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            int channelIdx = rowToChannel[row];
            String paramName = priorityParamIDs[channelIdx];

            if (auto* param = parameters.getParameter(paramName))
            {
                float normalizedValue = priority / 5.0f;
                param->setValueNotifyingHost(normalizedValue);
            }
        }

        // Invalidate cache once after all updates
        audioProcessor.invalidateLinearDrummingCache();
    }

    // Update visual state of all priority buttons
    void updatePriorityButtonStates()
    {
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            for (int priority = 0; priority < NUM_PRIORITIES; ++priority)
            {
                bool isSelected = (currentPriorityAssignments[row] == priority);
                priorityButtons[row][priority]->setToggleState(isSelected);
            }
        }
    }

    // Update parameter for a specific row
    void updatePriorityParameterForRow(int row)
    {
        // Map row to channel index
        int channelIdx = rowToChannel[row];
        String paramName = priorityParamIDs[channelIdx];

        if (auto* param = parameters.getParameter(paramName))
        {
            // Convert priority index (0-5) to normalized value (0.0-1.0)
            float normalizedValue = currentPriorityAssignments[row] / 5.0f;
            param->setValueNotifyingHost(normalizedValue);
        }

        // Invalidate cache to apply changes immediately
        audioProcessor.invalidateLinearDrummingCache();
    }

    // Sync priority matrix with current parameter values
    void updatePriorityMatrixFromParameters()
    {
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            int channelIdx = rowToChannel[row];
            String paramName = priorityParamIDs[channelIdx];

            if (auto* param = parameters.getParameter(paramName))
            {
                // Get normalized value (0.0-1.0) and convert to priority index (0-5)
                float normalizedValue = param->getValue();
                int priority = static_cast<int>(normalizedValue * 5.0f + 0.5f);
                priority = juce::jlimit(0, 5, priority);
                currentPriorityAssignments[row] = priority;
            }
        }

        updatePriorityButtonStates();
    }
};

// About overlay window that appears when clicking the Augmatic GRE title
class AboutOverlay : public Component
{
public:
    AboutOverlay()
    {
        setAlwaysOnTop(true);
        setInterceptsMouseClicks(true, false);
    }

    void paint(Graphics& g) override
    {
        // Semi-transparent black background for the full window
        g.fillAll(Colours::black.withAlpha(0.7f));

        // Get bounds for the centered overlay box (40% of parent size for better visibility)
        auto parentBounds = getParentComponent()->getLocalBounds();
        auto overlayWidth = parentBounds.getWidth() / 2.5;
        auto overlayHeight = parentBounds.getHeight() / 2.5;
        auto overlayBounds = Rectangle<int>(
            (parentBounds.getWidth() - overlayWidth) / 2,
            (parentBounds.getHeight() - overlayHeight) / 2,
            overlayWidth,
            overlayHeight
        );

        // Draw solid black box
        g.setColour(Colours::black);
        g.fillRect(overlayBounds);

        // Add border
        g.setColour(Colour(0xff9b74f6));  // Purple border
        g.drawRect(overlayBounds, 2);

        // Draw content
        int yPos = overlayBounds.getY() + 20;
        int lineHeight = 30;

        // "Augmatic GRE" title
        g.setColour(Colour(0xff9b74f6));  // Purple color #9b74f6
        g.setFont(Font(32.0f, Font::bold));
        g.drawText("Augmatic GRE", overlayBounds.getX(), yPos, overlayBounds.getWidth(), lineHeight,
                   Justification::centred);
        yPos += lineHeight + 5;

        // Version number
        g.setColour(Colours::white);
        g.setFont(Font(16.0f, Font::plain));
        g.drawText("v" + String(JucePlugin_VersionString),
                   overlayBounds.getX(), yPos, overlayBounds.getWidth(), 20,
                   Justification::centred);
        yPos += 25;

// REDACTED
// REDACTED
                   overlayBounds.getX(), yPos, overlayBounds.getWidth(), 20,
                   Justification::centred);
        yPos += 30;

        // "Includes code of Grids by Mutable Instruments" (clickable link)
        auto linkBounds = Rectangle<int>(overlayBounds.getX(), yPos, overlayBounds.getWidth(), 20);
        linkTextBounds = linkBounds;  // Store for click detection

        // Draw link text in blue (standard link color)
        g.setColour(Colour(0xff6495ed));  // Cornflower blue
        g.drawText("Includes code of Grids by Mutable Instruments",
                   linkBounds, Justification::centred);

        // Draw underline if hovering
        if (isMouseOverLink)
        {
            auto textWidth = g.getCurrentFont().getStringWidth("Includes code of Grids by Mutable Instruments");
            int underlineX = overlayBounds.getCentreX() - textWidth / 2;
            g.drawLine(underlineX, yPos + 18, underlineX + textWidth, yPos + 18, 1.0f);
        }
    }

    void mouseDown(const MouseEvent& e) override
    {
        // Check if clicked on the link
        if (linkTextBounds.contains(e.getPosition()))
        {
            URL("https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/").launchInDefaultBrowser();
            return;
        }

        // Check if clicked outside the overlay box
        auto parentBounds = getParentComponent()->getLocalBounds();
        auto overlayWidth = parentBounds.getWidth() / 2.5;
        auto overlayHeight = parentBounds.getHeight() / 2.5;
        auto overlayBounds = Rectangle<int>(
            (parentBounds.getWidth() - overlayWidth) / 2,
            (parentBounds.getHeight() - overlayHeight) / 2,
            overlayWidth,
            overlayHeight
        );

        if (!overlayBounds.contains(e.getPosition()))
        {
            setVisible(false);
        }
    }

    void mouseMove(const MouseEvent& e) override
    {
        bool wasOverLink = isMouseOverLink;
        isMouseOverLink = linkTextBounds.contains(e.getPosition());

        if (wasOverLink != isMouseOverLink)
        {
            repaint();
            setMouseCursor(isMouseOverLink ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
        }
    }

    bool keyPressed(const KeyPress& key) override
    {
        if (key.getKeyCode() == KeyPress::escapeKey)
        {
            setVisible(false);
            return true;
        }
        return false;
    }

private:
    Rectangle<int> linkTextBounds;
    bool isMouseOverLink = false;
};

// MIDI Tab component - grid structure with visible gridlines
class MIDITabComponent : public Component,
                         public AudioProcessorValueTreeState::Listener,
                         public MidiMappingManager::InstrumentNameListener
{
public:
    static constexpr int NUM_COLS = 12;
    static constexpr int NUM_ROWS = 6;
    static constexpr int MIN_CELL_SIZE = 40;
    static constexpr float HEADER_ROW_RATIO = 0.333f;

    // Row to channel mapping: BD=0, SN=1, HH=2, BD'=3, SN'=4, HH'=5
    static constexpr int rowToChannel[NUM_ROWS] = {0, 1, 2, 3, 4, 5};

    MIDITabComponent(AudioProcessorValueTreeState& apvts, AugmaticGREProcessor& proc, XYPadComponent* xyPadPtr = nullptr)
        : audioProcessor(proc), parameters(apvts), xyPadRef(xyPadPtr)
    {
        // Row labels for column 1 — initialized from MidiMappingManager if available
        auto* mappingMgr = proc.getMidiMappingManager();

        // Row colors from colors.md: BD=Red, SN=Blue, HH=Yellow, BD'=Pink, SN'=Green, HH'=Orange
        rowColours = {
            Colour(0xffff4757),  // BD - Red
            Colour(0xff5f9fec),  // SN - Blue
            Colour(0xfffeca57),  // HH - Yellow
            Colour(0xffff6b9d),  // BD' - Pink
            Colour(0xff1dd1a1),  // SN' - Green
            Colour(0xffff9f43)   // HH' - Orange
        };

        // MIDI note parameter IDs for each row
        // Row order: BD, SN, HH, BD', SN', HH'
        noteParamIds = {"bd_note", "sn_note", "hh_note", "bd_acc_note", "sn_acc_note", "hh_acc_note"};

        // Header labels
        noteHeaderLabel = std::make_unique<Label>("noteHeader", "NOTE");
        noteHeaderLabel->setJustificationType(Justification::centred);
        noteHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(noteHeaderLabel.get());

        octaveHeaderLabel = std::make_unique<Label>("octaveHeader", "OCTAVE");
        octaveHeaderLabel->setJustificationType(Justification::centred);
        octaveHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(octaveHeaderLabel.get());

        // LED column header - displays random drum word
        ledHeaderLabel = std::make_unique<Label>("ledHeader", getRandomDrumWord());
        ledHeaderLabel->setJustificationType(Justification::centred);
        ledHeaderLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(ledHeaderLabel.get());

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            // Create row label (column 1) with row-specific color
            juce::String labelText = mappingMgr ? mappingMgr->getInstrumentName(row) : juce::String(MidiMappingManager::DEFAULT_INSTRUMENT_NAMES[row]);
            rowLabels[row] = std::make_unique<Label>("rowLabel" + String(row), labelText);
            rowLabels[row]->setJustificationType(Justification::centred);
            rowLabels[row]->setFont(Font(28.0f, Font::plain));
            rowLabels[row]->setColour(Label::textColourId, rowColours[row]);
            rowLabels[row]->addMouseListener(this, false);
            addAndMakeVisible(rowLabels[row].get());

            // Create note name display (column 2)
            noteNameDisplays[row] = std::make_unique<ScrollableNoteNameDisplay>();
            noteNameDisplays[row]->setTextColour(rowColours[row]);
            noteNameDisplays[row]->setDefaultValue(0.0);  // C
            addAndMakeVisible(noteNameDisplays[row].get());

            // Create octave display (column 3)
            octaveDisplays[row] = std::make_unique<ScrollableOctaveDisplay>();
            octaveDisplays[row]->setTextColour(rowColours[row]);
            octaveDisplays[row]->setDefaultValue(1.0);  // Octave 1
            addAndMakeVisible(octaveDisplays[row].get());

            // Initialize displays from current parameter value
            int midiNote = static_cast<int>(parameters.getRawParameterValue(noteParamIds[row])->load());
            int noteIndex = midiNote % 12;
            int octave = (midiNote / 12) - 2;
            noteNameDisplays[row]->setValue(noteIndex, juce::dontSendNotification);
            octaveDisplays[row]->setValue(octave, juce::dontSendNotification);

            // Set up callbacks to update parameter when note name or octave changes
            noteNameDisplays[row]->onValueChanged = [this, row](int /*noteIndex*/) {
                updateMidiNoteParameter(row);
            };
            octaveDisplays[row]->onValueChanged = [this, row](int /*octave*/) {
                updateMidiNoteParameter(row);
            };

            // Create LED indicator (column 12) - clickable
            leds[row] = std::make_unique<PatternLED>();
            leds[row]->setLEDColour(rowColours[row]);
            leds[row]->onClicked = [this, row]() {
                int channelIdx = rowToChannel[row];
                audioProcessor.triggerNoteFromUI(channelIdx, 100);
                updateRandomDrumWord();
            };
            addAndMakeVisible(leds[row].get());
        }

        // === New controls in cols 5-10 ===
        // Layout:
        // Row 0 (BD): MIDI MAPPING (cols 5-6 label, 7-10 menu)
        // Row 1 (SN): NOTE LENGTH (5-6) + LINEAR GRID (8-9 label, 10 display)
        // Row 2 (HH): BPM (5-6) + MIDI CHANNEL (8-9 label, 10 display)
        // Row 3 (BD'): MIDI OUT (cols 5-6 label, 7-10 panel)
        // Row 4 (SN'): ANIMATION (cols 5-6 label, 7 button)
        // Row 5 (HH'): Version text (cols 5-10)

        // Row 0 (BD): MIDI Mapping label + menu box
        midiMappingLabel = std::make_unique<Label>("midiMapping", "MIDI MAPPING");
        midiMappingLabel->setJustificationType(Justification::centredLeft);
        midiMappingLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(midiMappingLabel.get());

        if (auto* mappingMgr = proc.getMidiMappingManager())
        {
            midiMappingMenuBox = std::make_unique<MappingPanel>(*mappingMgr);
            addAndMakeVisible(midiMappingMenuBox.get());

            midiMappingMenuBox->onMappingLoaded = [this]() {
                refreshAllNoteDisplays();
            };
        }

        // Row 1 (SN): Note Length (left) + Linear Grid (right)
        noteLengthLabel = std::make_unique<Label>("noteLength", "NOTE LENGTH");
        noteLengthLabel->setJustificationType(Justification::centredLeft);
        noteLengthLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(noteLengthLabel.get());

        noteDurationDisplay = std::make_unique<ScrollableLabelDisplay>();
        noteDurationDisplay->setLabels({{0,"4n"}, {1,"8n"}, {2,"16n"}, {3,"32n"}, {4,"64n"}});
        noteDurationDisplay->getSlider().setRange(0.0, 4.0, 1.0);
        noteDurationDisplay->setDefaultValue(2.0);
        noteDurationDisplay->setTextColour(Colour(0xff9b74f6));
        addAndMakeVisible(noteDurationDisplay.get());
        noteDurationAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "note_duration", noteDurationDisplay->getSlider());

        linearGridLabel = std::make_unique<Label>("linearGrid", "LINEAR GRID");
        linearGridLabel->setJustificationType(Justification::centredLeft);
        linearGridLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(linearGridLabel.get());

        linearGridDisplay = std::make_unique<ScrollableLabelDisplay>();
        linearGridDisplay->setLabels({{0,"64n"}, {1,"32n"}, {2,"16n"}, {3,"8n"}, {4,"4n"}});
        linearGridDisplay->getSlider().setRange(0.0, 4.0, 1.0);
        linearGridDisplay->setDefaultValue(2.0);
        linearGridDisplay->setTextColour(Colour(0xff9b74f6));
        addAndMakeVisible(linearGridDisplay.get());
        linearGridAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            apvts, LINEAR_DRUMMING_GRID_SUFFIX, linearGridDisplay->getSlider());

        // Row 2 (HH): BPM (left) + MIDI Channel (right) - standalone clock controls
        bpmLabel = std::make_unique<Label>("bpm", "BPM");
        bpmLabel->setJustificationType(Justification::centredLeft);
        bpmLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(bpmLabel.get());

        bpmDisplay = std::make_unique<ScrollableBPMDisplay>();
        bpmDisplay->setValue(static_cast<int>(proc.getInternalClockBPM()), juce::dontSendNotification);
        bpmDisplay->setTextColour(Colour(0xff9b74f6));
        bpmDisplay->onValueChanged = [this](int newBpm) {
            audioProcessor.setInternalClockBPM(static_cast<float>(newBpm));
        };
        addAndMakeVisible(bpmDisplay.get());

        midiChannelLabel = std::make_unique<Label>("midiChannel", "MIDI CHANNEL");
        midiChannelLabel->setJustificationType(Justification::centredLeft);
        midiChannelLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(midiChannelLabel.get());

        midiChannelDisplay = std::make_unique<ScrollableChannelDisplay>();
        midiChannelDisplay->setValue(proc.getMidiOutputChannel(), juce::dontSendNotification);
        midiChannelDisplay->setTextColour(Colour(0xff9b74f6));
        midiChannelDisplay->onValueChanged = [this](int newChannel) {
            audioProcessor.setMidiOutputChannel(newChannel);
        };
        addAndMakeVisible(midiChannelDisplay.get());

        // Row 3 (BD'): MIDI Output device selector
        midiOutputLabel = std::make_unique<Label>("midiOutput", "MIDI OUT");
        midiOutputLabel->setJustificationType(Justification::centredLeft);
        midiOutputLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(midiOutputLabel.get());

        midiOutputPanel = std::make_unique<MidiOutputPanel>(proc);
        addAndMakeVisible(midiOutputPanel.get());

        // Hide standalone-only controls in AUv3 mode (BPM, MIDI Channel, MIDI OUT)
        const bool isStandalone = (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
        bpmLabel->setVisible(isStandalone);
        bpmDisplay->setVisible(isStandalone);
        midiChannelLabel->setVisible(isStandalone);
        midiChannelDisplay->setVisible(isStandalone);
        midiOutputLabel->setVisible(isStandalone);
        midiOutputPanel->setVisible(isStandalone);

        // Row 4 (SN'): ANIMATION label + button
        animationLabel = std::make_unique<Label>("animation", "ANIMATION");
        animationLabel->setJustificationType(Justification::centredLeft);
        animationLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(animationLabel.get());

        animationButton = std::make_unique<AnimationButton>();
        animationButton->setToggleState(true, dontSendNotification);  // Default: enabled
        animationButton->onStateChange = [this](bool newState) {
            if (xyPadRef && xyPadRef->animation)
                xyPadRef->animation->setAnimationEnabled(newState);
        };
        addAndMakeVisible(animationButton.get());

        // Row 5 (HH'): Version text + USER MANUAL link spanning cols 5-10
        midiVersionLabel = std::make_unique<Label>("version",
            "AUGMATIC GRE v" + String(JucePlugin_VersionString) + "  -");
        midiVersionLabel->setJustificationType(Justification::centredLeft);
        midiVersionLabel->setColour(Label::textColourId, Colour(0xffb0b0b0));
        addAndMakeVisible(midiVersionLabel.get());

        manualLink = std::make_unique<HyperlinkButton>("USER MANUAL",
            URL("https://augmaticaudio.com/gre/user-manual/"));
        manualLink->setColour(HyperlinkButton::textColourId, Colour(0xff9b74f6));
        manualLink->setJustificationType(Justification::centredLeft);
        addAndMakeVisible(manualLink.get());

        // Register APVTS listeners for all note parameters
        // so displays update when mappings are loaded externally
        for (int row = 0; row < NUM_ROWS; ++row)
            parameters.addParameterListener(noteParamIds[row], this);

        // Register instrument name listener
        if (mappingMgr)
            mappingMgr->addInstrumentNameListener(this);
    }

    ~MIDITabComponent() override
    {
        dismissInstrumentNameDialog();

        // Remove instrument name listener
        if (auto* mappingMgr = audioProcessor.getMidiMappingManager())
            mappingMgr->removeInstrumentNameListener(this);

        // Remove APVTS listeners
        for (int row = 0; row < NUM_ROWS; ++row)
            parameters.removeParameterListener(noteParamIds[row], this);
    }

    // AudioProcessorValueTreeState::Listener callback
    void parameterChanged(const String& parameterID, float newValue) override
    {
        // Marshal to message thread since we're updating GUI components
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MIDITabComponent>(this), parameterID]() {
            if (safeThis != nullptr)
                safeThis->updateDisplayFromParameter(parameterID);
        });
    }

    // InstrumentNameListener callback
    void instrumentNamesChanged() override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MIDITabComponent>(this)]() {
            if (safeThis == nullptr) return;
            if (auto* mgr = safeThis->audioProcessor.getMidiMappingManager())
            {
                for (int row = 0; row < NUM_ROWS; ++row)
                    safeThis->rowLabels[row]->setText(mgr->getInstrumentName(row), dontSendNotification);
            }
        });
    }

    // Double-click on row label to rename instrument
    void mouseDoubleClick(const MouseEvent& event) override
    {
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            if (event.eventComponent == rowLabels[row].get())
            {
                showInstrumentNameDialog(row);
                return;
            }
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Calculate cell sizes
        int cellWidth = std::max(MIN_CELL_SIZE, bounds.getWidth() / NUM_COLS);
        float totalRowUnits = HEADER_ROW_RATIO + static_cast<float>(NUM_ROWS);
        int cellHeight = std::max(MIN_CELL_SIZE, static_cast<int>(bounds.getHeight() / totalRowUnits));
        int headerHeight = static_cast<int>(cellHeight * HEADER_ROW_RATIO);

        // Set scalable font for header labels
        float headerFontSize = std::max(11.0f, static_cast<float>(headerHeight) * 0.55f);
        Font headerFont(headerFontSize, Font::plain);

        // Position header labels
        noteHeaderLabel->setBounds(1 * cellWidth, 0, cellWidth, headerHeight);
        noteHeaderLabel->setFont(headerFont);

        octaveHeaderLabel->setBounds(2 * cellWidth, 0, cellWidth, headerHeight);
        octaveHeaderLabel->setFont(headerFont);

        ledHeaderLabel->setBounds(11 * cellWidth, 0, cellWidth, headerHeight);
        ledHeaderLabel->setFont(headerFont);

        // Position data rows below header
        int dataRowStartY = headerHeight;

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            int rowY = dataRowStartY + row * cellHeight;
            float fontSize = static_cast<float>(cellHeight) * 0.38f;
            Font dataFont(fontSize, Font::plain);

            // Position label in column 1 (index 0)
            rowLabels[row]->setBounds(0, rowY, cellWidth, cellHeight);
            rowLabels[row]->setFont(dataFont);

            // Position note name display in column 2 (index 1)
            noteNameDisplays[row]->setBounds(1 * cellWidth, rowY, cellWidth, cellHeight);
            noteNameDisplays[row]->setFont(dataFont);

            // Position octave display in column 3 (index 2)
            octaveDisplays[row]->setBounds(2 * cellWidth, rowY, cellWidth, cellHeight);
            octaveDisplays[row]->setFont(dataFont);

            // Position LED in column 12 (index 11)
            int ledSize;
            if (bounds.getWidth() <= 900) {
                ledSize = 25;
            } else {
                int knobMaxSize = std::min(cellWidth, cellHeight) - 10;
                int maxSize = static_cast<int>(knobMaxSize * 0.6f);
                float progress = static_cast<float>(bounds.getWidth() - kResponsiveBreakpointStart) / kResponsiveBreakpointRange;
                progress = std::min(1.0f, progress);
                ledSize = 25 + static_cast<int>((maxSize - 25) * progress);
            }
            int ledX = 11 * cellWidth + (cellWidth - ledSize) / 2;
            int ledY = rowY + (cellHeight - ledSize) / 2;
            leds[row]->setBounds(ledX, ledY, ledSize, ledSize);

            // Position new controls in cols 5-10
            // Layout per standalone.md:
            // Row 0: MIDI MAPPING (5-6 label, 7-10 menu)
            // Row 1: NOTE LENGTH (5-6 label, 7 display) + LINEAR GRID (8-9 label, 10 display)
            // Row 2: BPM (5-6 label, 7 display) + MIDI CHANNEL (8-9 label, 10 display)
            // Row 3: MIDI OUT (5-6 label, 7-10 panel)
            // Row 4: ANIMATION (5-6 label, 7 button)
            // Row 5: Version text (5-10)
            int labelPadLeft = 6;
            if (row == 0)  // MIDI Mapping
            {
                midiMappingLabel->setBounds(5 * cellWidth + labelPadLeft, rowY, 2 * cellWidth - labelPadLeft, cellHeight);
                midiMappingLabel->setFont(headerFont);
                if (midiMappingMenuBox)
                {
                    int menuH = presetMenuHeight;
                    int menuY = rowY + (cellHeight - menuH) / 2;
                    midiMappingMenuBox->setBounds(7 * cellWidth, menuY, 4 * cellWidth, menuH);
                }
            }
            else if (row == 1)  // NOTE LENGTH + LINEAR GRID
            {
                // NOTE LENGTH: cols 5-6 label, col 7 display
                noteLengthLabel->setBounds(5 * cellWidth + labelPadLeft, rowY, 2 * cellWidth - labelPadLeft, cellHeight);
                noteLengthLabel->setFont(headerFont);
                noteDurationDisplay->setBounds(7 * cellWidth, rowY, cellWidth, cellHeight);
                noteDurationDisplay->setFont(dataFont);

                // LINEAR GRID: cols 8-9 label, col 10 display
                linearGridLabel->setBounds(8 * cellWidth + labelPadLeft, rowY, 2 * cellWidth - labelPadLeft, cellHeight);
                linearGridLabel->setFont(headerFont);
                linearGridDisplay->setBounds(10 * cellWidth, rowY, cellWidth, cellHeight);
                linearGridDisplay->setFont(dataFont);
            }
            else if (row == 2)  // BPM + MIDI CHANNEL
            {
                // BPM: cols 5-6 label, col 7 display
                bpmLabel->setBounds(5 * cellWidth + labelPadLeft, rowY, 2 * cellWidth - labelPadLeft, cellHeight);
                bpmLabel->setFont(headerFont);
                bpmDisplay->setBounds(7 * cellWidth, rowY, cellWidth, cellHeight);
                bpmDisplay->setFont(dataFont);

                // MIDI CHANNEL: cols 8-9 label, col 10 display
                midiChannelLabel->setBounds(8 * cellWidth + labelPadLeft, rowY, 2 * cellWidth - labelPadLeft, cellHeight);
                midiChannelLabel->setFont(headerFont);
                midiChannelDisplay->setBounds(10 * cellWidth, rowY, cellWidth, cellHeight);
                midiChannelDisplay->setFont(dataFont);
            }
            else if (row == 3)  // MIDI OUT
            {
                midiOutputLabel->setBounds(5 * cellWidth + labelPadLeft, rowY, 2 * cellWidth - labelPadLeft, cellHeight);
                midiOutputLabel->setFont(headerFont);
                if (midiOutputPanel)
                {
                    int menuH = presetMenuHeight;
                    int menuY = rowY + (cellHeight - menuH) / 2;
                    midiOutputPanel->setBounds(7 * cellWidth, menuY, 4 * cellWidth, menuH);
                }
            }
            else if (row == 4)  // ANIMATION
            {
                animationLabel->setBounds(5 * cellWidth + labelPadLeft, rowY, 2 * cellWidth - labelPadLeft, cellHeight);
                animationLabel->setFont(headerFont);
                int knobPadding = 5;
                int knobSize = std::min(cellWidth, cellHeight) - knobPadding * 2;
                int animBtnSize = static_cast<int>(knobSize * 0.9f);
                int animBtnX = 7 * cellWidth + (cellWidth - animBtnSize) / 2;
                int animBtnY = rowY + (cellHeight - animBtnSize) / 2;
                animationButton->setBounds(animBtnX, animBtnY, animBtnSize, animBtnSize);
            }
            else if (row == 5)  // Version + Manual link
            {
                int versionX = 5 * cellWidth + labelPadLeft;
                midiVersionLabel->setFont(headerFont);
                int versionTextWidth = headerFont.getStringWidth(midiVersionLabel->getText()) + 4;
                midiVersionLabel->setBounds(versionX, rowY, versionTextWidth, cellHeight);

                Font linkFont(headerFont.getHeight(), Font::plain);
                manualLink->setFont(linkFont, false);
                int linkTextWidth = linkFont.getStringWidth("USER MANUAL") + 8;
                manualLink->setBounds(versionX + versionTextWidth - 4, rowY, linkTextWidth, cellHeight);
            }
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff101010));

        // Calculate cell sizes
        auto bounds = getLocalBounds();
        int cellWidth = std::max(MIN_CELL_SIZE, bounds.getWidth() / NUM_COLS);
        float totalRowUnits = HEADER_ROW_RATIO + static_cast<float>(NUM_ROWS);
        int cellHeight = std::max(MIN_CELL_SIZE, static_cast<int>(bounds.getHeight() / totalRowUnits));
        int headerHeight = static_cast<int>(cellHeight * HEADER_ROW_RATIO);
        int dataRowStartY = headerHeight;

        // Grid lines — same as background (invisible)
        g.setColour(Colour(0xff101010));

        // Vertical lines (full height)
        for (int col = 1; col < NUM_COLS; ++col)
            g.drawVerticalLine(col * cellWidth, 0.0f, static_cast<float>(bounds.getHeight()));

        // Horizontal line below header row
        g.drawHorizontalLine(headerHeight, 0.0f, static_cast<float>(bounds.getWidth()));

        // Horizontal lines for data rows
        for (int row = 1; row < NUM_ROWS; ++row)
            g.drawHorizontalLine(dataRowStartY + row * cellHeight, 0.0f, static_cast<float>(bounds.getWidth()));
    }

    // Set the MIDI Mapping menu height to match Preset Menu (called from editor resized)
    void setPresetMenuHeight(int h) { presetMenuHeight = h; }

    // Trigger LED by row index
    void triggerLED(int rowIndex)
    {
        if (rowIndex >= 0 && rowIndex < NUM_ROWS && leds[rowIndex])
            leds[rowIndex]->trigger();
    }

    // Map channel index to row
    void triggerLEDForChannel(int channelIndex)
    {
        static const int channelToRow[] = {0, 1, 2, 3, 4, 5};
        if (channelIndex >= 0 && channelIndex < 6)
            triggerLED(channelToRow[channelIndex]);
    }

    // Update LED header with random drum word (called on tab switch)
    void updateRandomDrumWord()
    {
        if (ledHeaderLabel)
            ledHeaderLabel->setText(getRandomDrumWord(), juce::dontSendNotification);
    }

private:
    // Update MIDI note parameter from note name and octave displays (UI → APVTS)
    void updateMidiNoteParameter(int row)
    {
        if (row < 0 || row >= NUM_ROWS) return;

        int noteIndex = noteNameDisplays[row]->getValue();
        int octave = octaveDisplays[row]->getValue();

        // MIDI note = (octave + 2) * 12 + noteIndex
        int midiNote = (octave + 2) * 12 + noteIndex;

        // Clamp to valid MIDI range
        midiNote = juce::jlimit(0, 127, midiNote);

        // Update the parameter
        if (auto* param = parameters.getParameter(noteParamIds[row]))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(midiNote)));
    }

    // Update note name and octave displays from APVTS parameter (APVTS → UI)
    // Called when MIDI Mapping is loaded or parameter changes externally
    void updateDisplayFromParameter(const String& parameterID)
    {
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            if (noteParamIds[row] == parameterID)
            {
                int midiNote = static_cast<int>(parameters.getRawParameterValue(parameterID)->load());
                int noteIndex = midiNote % 12;
                int octave = (midiNote / 12) - 2;

                // Use dontSendNotification to avoid feedback loop
                // (onValueChanged would call updateMidiNoteParameter which would re-set the parameter)
                noteNameDisplays[row]->setValue(noteIndex, juce::dontSendNotification);
                octaveDisplays[row]->setValue(octave, juce::dontSendNotification);
                noteNameDisplays[row]->repaint();
                octaveDisplays[row]->repaint();
                break;
            }
        }
    }

    // Refresh all note/octave displays from current APVTS parameter values
    // Called directly by MappingPanel callback to bypass APVTS listener timing issues
    void refreshAllNoteDisplays()
    {
        for (int row = 0; row < NUM_ROWS; ++row)
            updateDisplayFromParameter(noteParamIds[row]);
    }

    // --- Instrument Name Dialog ---

    class InstrumentNameDialog : public juce::Component
    {
    public:
        juce::TextEditor nameEditor;
        juce::TextButton saveButton{"Save"};
        juce::TextButton cancelButton{"Cancel"};
        juce::TextButton restoreButton{"Restore"};
        std::function<void(const juce::String&)> onSave;
        std::function<void()> onCancel;
        std::function<void()> onRestore;

        InstrumentNameDialog(const juce::String& currentName, const juce::String& channelLabel)
        {
            titleText = "RENAME " + channelLabel;

            addAndMakeVisible(nameEditor);
            nameEditor.setText(currentName);
            nameEditor.setInputRestrictions(3);
            nameEditor.setFont(juce::Font(16.0f));
            nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
            nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff595e5f));
            nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff707070));
            nameEditor.setJustification(juce::Justification::centred);

            addAndMakeVisible(saveButton);
            saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            saveButton.onClick = [this]() {
                auto text = nameEditor.getText().trim();
                if (text.isNotEmpty() && onSave)
                    onSave(text);
            };

            addAndMakeVisible(cancelButton);
            cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            cancelButton.onClick = [this]() { if (onCancel) onCancel(); };

            addAndMakeVisible(restoreButton);
            restoreButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            restoreButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            restoreButton.onClick = [this]() { if (onRestore) onRestore(); };

            setSize(240, 128);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(10);
            bounds.removeFromTop(28);  // Title area
            nameEditor.setBounds(bounds.removeFromTop(36));
            bounds.removeFromTop(8);
            auto buttonArea = bounds.removeFromTop(36);
            int buttonW = (buttonArea.getWidth() - 20) / 3;
            cancelButton.setBounds(buttonArea.removeFromLeft(buttonW));
            buttonArea.removeFromLeft(10);
            restoreButton.setBounds(buttonArea.removeFromLeft(buttonW));
            buttonArea.removeFromLeft(10);
            saveButton.setBounds(buttonArea);
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour(juce::Colour(0xff2d2d2d));
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(juce::Colour(0xff595e5f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);

            // Title
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(16.0f));
            g.drawText(titleText, getLocalBounds().reduced(10).removeFromTop(24),
                       juce::Justification::centred);
        }

    private:
        juce::String titleText;
    };

    std::unique_ptr<InstrumentNameDialog> instrumentNameDialog;

    void showInstrumentNameDialog(int row)
    {
        dismissInstrumentNameDialog();

        auto* mappingMgr = audioProcessor.getMidiMappingManager();
        if (!mappingMgr) return;

        // Get base name for editing (strip trailing "'" for accent channels)
        juce::String baseName = mappingMgr->getInstrumentNames()[row];
        if (baseName.endsWithChar('\''))
            baseName = baseName.dropLastCharacters(1);
        juce::String channelLabel = mappingMgr->getInstrumentName(row);

        instrumentNameDialog = std::make_unique<InstrumentNameDialog>(baseName, channelLabel);

        instrumentNameDialog->onSave = [this, row](const juce::String& newName) {
            if (auto* mgr = audioProcessor.getMidiMappingManager())
                mgr->setInstrumentName(row, newName);
            dismissInstrumentNameDialog();
        };

        instrumentNameDialog->onCancel = [this]() {
            dismissInstrumentNameDialog();
        };

        instrumentNameDialog->onRestore = [this, row]() {
            if (auto* mgr = audioProcessor.getMidiMappingManager())
                mgr->setInstrumentName(row, MidiMappingManager::DEFAULT_INSTRUMENT_NAMES[row]);
            dismissInstrumentNameDialog();
        };

        if (auto* topLevel = getTopLevelComponent())
        {
            topLevel->addAndMakeVisible(instrumentNameDialog.get());
            int dialogW = instrumentNameDialog->getWidth();
            int dialogH = instrumentNameDialog->getHeight();
            int dialogX = (topLevel->getWidth() - dialogW) / 2;
            int dialogY = topLevel->getHeight() / 6;
            instrumentNameDialog->setBounds(dialogX, dialogY, dialogW, dialogH);
            instrumentNameDialog->nameEditor.grabKeyboardFocus();
        }
    }

    void dismissInstrumentNameDialog()
    {
        if (instrumentNameDialog)
        {
            if (auto* topLevel = instrumentNameDialog->getParentComponent())
                topLevel->removeChildComponent(instrumentNameDialog.get());
            auto safeDialog = std::move(instrumentNameDialog);
            juce::MessageManager::callAsync([d = std::move(safeDialog)]() mutable { d.reset(); });
        }
    }

    // Header labels
    std::unique_ptr<Label> noteHeaderLabel;
    std::unique_ptr<Label> octaveHeaderLabel;
    std::unique_ptr<Label> ledHeaderLabel;

    // Row colors (stored for dynamic access)
    std::array<Colour, NUM_ROWS> rowColours;

    // Parameter IDs for MIDI notes
    std::array<String, NUM_ROWS> noteParamIds;

    // Data row components
    std::array<std::unique_ptr<Label>, NUM_ROWS> rowLabels;
    std::array<std::unique_ptr<ScrollableNoteNameDisplay>, NUM_ROWS> noteNameDisplays;
    std::array<std::unique_ptr<ScrollableOctaveDisplay>, NUM_ROWS> octaveDisplays;
    std::array<std::unique_ptr<PatternLED>, NUM_ROWS> leds;

    // Processor reference for manual note triggering (clickable LEDs)
    AugmaticGREProcessor& audioProcessor;
    AudioProcessorValueTreeState& parameters;

    // New controls in cols 5-10
    std::unique_ptr<Label> noteLengthLabel;
    std::unique_ptr<ScrollableLabelDisplay> noteDurationDisplay;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> noteDurationAttachment;

    std::unique_ptr<Label> linearGridLabel;
    std::unique_ptr<ScrollableLabelDisplay> linearGridDisplay;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> linearGridAttachment;

    std::unique_ptr<Label> midiMappingLabel;
    std::unique_ptr<MappingPanel> midiMappingMenuBox;
    int presetMenuHeight = 34;  // Updated from editor resized() to match Preset Menu

    std::unique_ptr<Label> bpmLabel;
    std::unique_ptr<ScrollableBPMDisplay> bpmDisplay;

    std::unique_ptr<Label> midiChannelLabel;
    std::unique_ptr<ScrollableChannelDisplay> midiChannelDisplay;

    std::unique_ptr<Label> midiOutputLabel;
    std::unique_ptr<MidiOutputPanel> midiOutputPanel;

    std::unique_ptr<Label> animationLabel;
    std::unique_ptr<AnimationButton> animationButton;

    std::unique_ptr<Label> midiVersionLabel;
    std::unique_ptr<HyperlinkButton> manualLink;

    // Reference to XY pad for animation control
    XYPadComponent* xyPadRef = nullptr;

public:
    // Set XY pad reference after construction
    void setXYPadReference(XYPadComponent* xyPad) { xyPadRef = xyPad; }

    // Refresh BPM display from processor state (called after preset load)
    void refreshBPM()
    {
        if (bpmDisplay)
        {
            bpmDisplay->setValue(static_cast<int>(audioProcessor.getInternalClockBPM()), juce::dontSendNotification);
            bpmDisplay->repaint();
        }
    }
};

// Custom TabbedComponent that notifies when tab changes
class DrumTabbedComponent : public juce::TabbedComponent
{
public:
    DrumTabbedComponent(TabbedButtonBar::Orientation orientation)
        : TabbedComponent(orientation) {}

    std::function<void(int, const juce::String&)> onTabChanged;

    void currentTabChanged(int newCurrentTabIndex, const juce::String& newCurrentTabName) override
    {
        TabbedComponent::currentTabChanged(newCurrentTabIndex, newCurrentTabName);
        if (onTabChanged)
            onTabChanged(newCurrentTabIndex, newCurrentTabName);
    }
};

class AugmaticGREEditor : public AudioProcessorEditor, public Slider::Listener
{
public:
    AugmaticGREEditor(AugmaticGREProcessor&);
    ~AugmaticGREEditor() override;

    void paint(Graphics&) override;
    void resized() override;
    void sliderValueChanged(Slider* slider) override;
    void mouseDown(const MouseEvent& event) override;
    bool keyPressed(const KeyPress& key) override;

private:
    AugmaticGREProcessor& audioProcessor;

    // Tab styling helper
    void updateTabBarStyle();

    // Tab component for organized interface (custom subclass with tab change callback)
    std::unique_ptr<DrumTabbedComponent> tabbedComponent;
    TabLookAndFeel tabLookAndFeel;

    // Pattern Tab Components (formerly Euclidean, now contains all pattern controls)
    std::unique_ptr<Slider> gridsXSlider;
    std::unique_ptr<Slider> gridsYSlider;
    std::unique_ptr<Slider> bdDensitySlider;
    std::unique_ptr<Slider> sdDensitySlider;
    std::unique_ptr<Slider> hhDensitySlider;
    std::unique_ptr<Slider> chaosSlider;

    // Accent Pattern Tab Components
    std::unique_ptr<Slider> bdAccentDensitySlider;
    std::unique_ptr<Slider> snAccentDensitySlider;
    std::unique_ptr<Slider> hhAccentDensitySlider;
    std::array<std::unique_ptr<Slider>, 3> chaosAccentSliders;
    
    // Pattern Tab Labels
    std::unique_ptr<Label> gridsXLabel;
    std::unique_ptr<Label> gridsYLabel;
    std::unique_ptr<Label> bdDensityLabel;
    std::unique_ptr<Label> sdDensityLabel;
    std::unique_ptr<Label> hhDensityLabel;
    std::unique_ptr<Label> chaosLabel;

    // Accent Pattern Tab Labels
    std::unique_ptr<Label> accentHeaderLabel;
    std::unique_ptr<Label> snAccentHeaderLabel;
    std::unique_ptr<Label> hhAccentHeaderLabel;
    std::unique_ptr<Label> bdAccentDensityLabel;
    std::unique_ptr<Label> snAccentDensityLabel;
    std::unique_ptr<Label> hhAccentDensityLabel;
    std::array<std::unique_ptr<Label>, 3> chaosAccentLabels;

    // Main channel headers for Pattern tab
    std::unique_ptr<Label> bdChannelHeaderLabel;
    std::unique_ptr<Label> snChannelHeaderLabel;
    std::unique_ptr<Label> hhChannelHeaderLabel;
    
    // Advanced Tab Components (Phase 3 Features)
    std::unique_ptr<Component> advancedTab;

    // Per-channel Phase 3 controls (BD, SN, HH + BD_Acc, SN_Acc, HH_Acc)
    std::array<std::unique_ptr<Slider>, 3> chaosChannelSliders;        // Independent chaos (main channels)
    std::array<std::unique_ptr<Slider>, 6> velocityValueSliders;      // Velocity level (all 6 channels)
    std::array<std::unique_ptr<Slider>, 6> velocityRandomizeSliders;  // Velocity randomization (all 6 channels)

    // Advanced Tab Labels
    std::array<std::unique_ptr<Label>, 6> instrumentLabels;           // BD, SN, HH, BD_Acc, SN_Acc, HH_Acc section labels
    std::array<std::unique_ptr<Label>, 3> chaosChannelLabels;         // Independent chaos labels (main channels only)
    std::array<std::unique_ptr<Label>, 6> velocityValueLabels;        // Velocity level labels (all 6 channels)
    std::array<std::unique_ptr<Label>, 6> velocityRandomizeLabels;    // Velocity randomization labels (all 6 channels)
    
    // Clock Ratio Components (in Advanced Tab)
    std::array<std::unique_ptr<ComboBox>, 6> clockRatioComboBoxes;     // Clock ratio (all 6 channels)
    std::array<std::unique_ptr<Label>, 6> clockRatioLabels;            // Clock ratio labels (all 6 channels)

    
    // XY Pad (Map X / Map Y) + Chaos Slider - left side of plugin
    std::unique_ptr<XYPadComponent> xyPad;
    std::unique_ptr<ChaosSliderComponent> chaosSliderPad;
    std::unique_ptr<TransportButton> transportButton;  // Standalone mode only

    // Pattern Tab Parameter Attachments
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> gridsXAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> gridsYAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> bdDensityAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sdDensityAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hhDensityAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> chaosAttachment;

    // Accent Pattern Tab Parameter Attachments
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> bdAccentDensityAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> snAccentDensityAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hhAccentDensityAttachment;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 3> chaosAccentAttachments;
    
    // Phase 3 Parameter Attachments
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 3> chaosChannelAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>, 6> clockRatioAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> velocityValueAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> velocityRandomizeAttachments;
    
    // Shift/Humanization Tab Components
    std::unique_ptr<Component> shiftTab;
    std::array<std::unique_ptr<Slider>, 6> shiftSliders;              // Shift amount for each channel (bipolar: 0-126, 63=OFF)
    std::array<std::unique_ptr<Slider>, 6> humanizeSliders;           // Humanization amount for each channel
    std::array<std::unique_ptr<Slider>, 6> swingSliders;              // Swing amount for each channel (-99 to +99)
    std::array<std::unique_ptr<Label>, 6> shiftLabels;                // Shift labels
    std::array<std::unique_ptr<Label>, 6> humanizeLabels;             // Humanization labels
    std::array<std::unique_ptr<Label>, 6> swingLabels;                // Swing labels
    std::array<std::unique_ptr<Label>, 6> shiftChannelLabels;         // Channel name labels

    // Shift/Humanization Parameter Attachments
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> shiftAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> humanizeAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> swingAttachments;

    // Accent Bender Panel (now in Velocity tab)
    class AccentBenderPanel* accentBenderPanel = nullptr;  // Raw pointer, owned by advancedTab

    // PATTERN Tab (dynamic grid with labels and LEDs)
    std::unique_ptr<PatternTabComponent> patternTab;
    std::unique_ptr<VelocityTabComponent> velocityTab;
    std::unique_ptr<MixTabComponent> mixTab;
    std::unique_ptr<MIDITabComponent> newMidiTab;

    // Euclidean Tab Components
    std::unique_ptr<Component> euclideanTab;
    std::array<std::unique_ptr<Slider>, 6> engineProbabilitySliders;        // Engine Probability (0.0=Grids, 1.0=Euclidean)
    std::array<std::unique_ptr<TextButton>, 6> engineProbGridsButtons;      // "G" button (100% Grids)
    std::array<std::unique_ptr<TextButton>, 6> engineProbEuclidButtons;     // "E" button (100% Euclidean)
    std::array<std::unique_ptr<Slider>, 6> euclideanStepsSliders;           // Steps (2-32) per channel
    std::array<std::unique_ptr<Slider>, 6> euclideanPulsesSliders;          // Pulses (0-steps) per channel
    std::array<std::unique_ptr<Slider>, 6> euclideanStartOnSliders;          // Start On (1 to steps) per channel

    // Euclidean Tab Labels
    std::array<std::unique_ptr<Label>, 6> euclideanChannelHeaderLabels;     // Channel headers (BD, SN, HH, etc.)
    std::array<std::unique_ptr<Label>, 6> euclideanStepsLabels;             // Steps labels
    std::array<std::unique_ptr<Label>, 6> euclideanPulsesLabels;            // Pulses labels
    std::array<std::unique_ptr<Label>, 6> euclideanStartOnLabels;           // Start On labels

    // Euclidean Tab Parameter Attachments
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> engineProbabilityAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> euclideanStepsAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> euclideanPulsesAttachments;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 6> euclideanStartOnAttachments;

    // Helper methods
    void createPatternTab();
    void createAdvancedTab();
    void createShiftTab();
    // createMidiTab() removed — OldMIDI tab deleted in v0.4.133
    // createLinearDrummingTab() removed — OldMix tab deleted in v0.4.085
    void createEuclideanTab();
    void createLEDIndicators();

    // Layout helper — positions all controls within the given content rectangle
    void layoutAllControls(juce::Rectangle<int> contentBounds);
    
    // LED triggering methods
    void onMIDINoteGenerated(int midiNote, uint8_t velocity, int channelIndex, int stepInPattern, double ppqPosition, double bpm);  // Called from processor when MIDI note is sent

private:
    // Preset management panel
    std::unique_ptr<PresetPanel> presetPanel;

    // Custom LookAndFeel for dropdown background fix
    CustomComboBoxLookAndFeel customLookAndFeel;

    // About overlay window
    std::unique_ptr<AboutOverlay> aboutOverlay;

    // Wrapper component for uniform scaling on iPhone AUv3
    // All UI elements are children of this component; a GPU-accelerated AffineTransform
    // shrinks the layout to fit small host containers (iPhone portrait)
    juce::Component contentWrapper;

    // iOS AUv3: broad constrainer for host size negotiation (member must outlive editor)
    juce::ComponentBoundsConstrainer iosAUv3Constrainer;

#if JUCE_IOS
    // "Enable Files App Access" button — shown in AUv3 when no bookmark is set
    std::unique_ptr<juce::TextButton> bookmarkSetupButton;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AugmaticGREEditor)
};