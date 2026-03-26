#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

using namespace juce;

class LEDIndicator : public Component, public Timer
{
public:
    LEDIndicator() = default;
    ~LEDIndicator() override
    {
        stopTimer();  // Explicitly stop timer before destruction to prevent blocking cleanup
    }
    
    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(2);
        
        // LED background (dark when off)
        g.setColour(isLit ? ledOnColour : ledOffColour);
        g.fillEllipse(bounds.toFloat());
        
        // LED highlight when on
        if (isLit)
        {
            g.setColour(ledOnColour.brighter(0.3f));
            auto highlight = bounds.reduced(bounds.getWidth() * 0.25f);
            g.fillEllipse(highlight.toFloat());
        }
        
        // LED border
        g.setColour(Colours::darkgrey);
        g.drawEllipse(bounds.toFloat(), 1.0f);
    }
    
    void trigger()
    {
        // Only trigger if not muted
        if (!isMuted) {
            // Only repaint if state actually changes (avoid redundant repaints)
            bool needsRepaint = !isLit;
            isLit = true;

            // INTELLIGENT LED BATCHING: Extend timer if already active (rapid-fire notes)
            if (isTimerRunning()) {
                // Reset timer to 50ms from now - keeps LED lit during rapid sequences
                stopTimer();
            }
            startTimer(50); // HIGH-SPEED OPTIMIZATION: Reduced to 50ms for 200 BPM visualization

            // Only repaint if state changed (reduces redundant paint calls)
            if (needsRepaint) {
                repaint();
            }
        }
    }

    void timerCallback() override
    {
        // Only repaint if state actually changes
        if (isLit) {
            isLit = false;
            repaint();
        }
        stopTimer();
    }
    
    void setLEDColour(Colour onColour, Colour offColour = Colours::darkgrey)
    {
        if (!isMuted) {
            ledOnColour = onColour;
            ledOffColour = offColour;
            repaint();
        }
    }

    void setMuted(bool muted)
    {
        isMuted = muted;
        if (muted) {
            // Set grey colors for muted state
            ledOnColour = Colours::grey;
            ledOffColour = Colours::darkgrey;
        }
        repaint();
    }

private:
    bool isLit = false;
    bool isMuted = false;
    Colour ledOnColour = Colours::green;
    Colour ledOffColour = Colours::darkgrey.darker();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LEDIndicator)
};