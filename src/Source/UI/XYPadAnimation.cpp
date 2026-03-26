#include "XYPadAnimation.h"

XYPadAnimation::XYPadAnimation(juce::AudioProcessorValueTreeState& apvts)
    : parameters(apvts)
{
    setInterceptsMouseClicks(false, false);
    setOpaque(false);

    for (auto& s : splashes) s.active = false;
    for (auto& h : halos) h.active = false;

    startTimerHz(kTimerHz);
}

XYPadAnimation::~XYPadAnimation()
{
    stopTimer();
}

void XYPadAnimation::setDotRadius(float radius)
{
    dotRadius = radius;
}

// ---- Trigger ----

void XYPadAnimation::randomizeLineAssignments()
{
    // Static mapping: no randomization
    // Horizontal: BD=0, SN=1, HH=2 (lines 0,1,2)
    // Vertical: BD'=3, SN'=4, HH'=5 (lines 3,4,5)
    lineAssignments = {0, 1, 2, 3, 4, 5};
}

void XYPadAnimation::triggerUserRipple(float x, float y)
{
    if (!animationEnabled) return;

    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;

    // Create 3 concentric ripples in purple (matching cursor dot)
    float baseMaxRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.35f;

    for (int i = 0; i < 3; ++i)
    {
        auto& halo = halos[nextHaloIndex];
        nextHaloIndex = (nextHaloIndex + 1) % kMaxHalos;
        halo.x = x;
        halo.y = y;
        halo.currentRadius = dotRadius * 0.3f + i * 8.0f;
        halo.maxRadius = baseMaxRadius;
        halo.expansionSpeed = 2.0f;
        halo.alpha = 0.5f - i * 0.12f;
        halo.decayRate = halo.alpha / 45.0f;
        halo.colour = juce::Colour(0xff9b74f6);  // Purple
        halo.active = true;
    }
}

void XYPadAnimation::triggerNote(int channelIndex, uint8_t velocity, double ppqPosition, double bpm)
{
    if (!animationEnabled) return;
    if (channelIndex < 0 || channelIndex >= kNumTracks) return;
    if (bpm <= 0.0) bpm = 120.0;

    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;

    // 1 bar = 4 quarter notes in 4/4 time — normalized position 0..1
    double normPos = std::fmod(ppqPosition, 4.0) / 4.0;
    if (normPos < 0.0) normPos += 1.0;

    float posX, posY;

    // Use randomized line assignment
    int lineIndex = lineAssignments[channelIndex];

    if (lineIndex < 3)
    {
        // Horizontal lines: dot moves left → right
        // Y fixed at grid lines: 1/4, 2/4, 3/4 of height
        posX = bounds.getX() + static_cast<float>(normPos) * bounds.getWidth();
        posY = bounds.getY() + bounds.getHeight() * static_cast<float>(lineIndex + 1) / 4.0f;
    }
    else
    {
        // Vertical lines: dot moves top → bottom
        // X fixed at grid lines: 1/4, 2/4, 3/4 of width
        int vertIndex = lineIndex - 3;
        posX = bounds.getX() + bounds.getWidth() * static_cast<float>(vertIndex + 1) / 4.0f;
        posY = bounds.getY() + static_cast<float>(normPos) * bounds.getHeight();
    }

    float velNorm = velocity / 127.0f;

    // Create splash — dot appears at grid position (50% of cursor dot size)
    // 100ms duration at 60Hz = 6 frames
    auto& splash = splashes[nextSplashIndex];
    nextSplashIndex = (nextSplashIndex + 1) % kMaxSplashes;
    splash.x = posX;
    splash.y = posY;
    splash.alpha = 1.0f;
    splash.age = 0.0f;
    splash.holdFrames = 3.0f;   // Hold at full alpha for 50ms (3 frames)
    splash.fadeFrames = 3.0f;   // Fade over 50ms (3 frames)
    splash.dotRadius = dotRadius * 0.5f;  // 50% of cursor dot size
    splash.colour = juce::Colour(trackColourValues[channelIndex]).darker(0.4f);  // Darker version of ripple color
    splash.active = true;

    // Create 3 concentric water ripple waves
    // Ripple distance scales with velocity: low velocity = small ripples, high velocity = large ripples
    float baseMaxRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float velocityScaledMaxRadius = baseMaxRadius * (0.3f + velNorm * 0.7f);  // 30% to 100% of base radius

    for (int i = 0; i < 3; ++i)
    {
        auto& halo = halos[nextHaloIndex];
        nextHaloIndex = (nextHaloIndex + 1) % kMaxHalos;
        halo.x = posX;
        halo.y = posY;
        halo.currentRadius = dotRadius * 0.3f + i * 8.0f;  // Staggered start radii
        halo.maxRadius = velocityScaledMaxRadius;
        halo.expansionSpeed = 2.0f + velNorm * 1.0f;
        halo.alpha = (0.5f - i * 0.12f) * (0.6f + velNorm * 0.4f);  // Decreasing alpha per ripple
        halo.decayRate = halo.alpha / 45.0f;  // Fade over ~0.75 seconds (45 frames at 60Hz)
        halo.colour = juce::Colour(trackColourValues[channelIndex]);
        halo.active = true;
    }
}

// ---- Timer ----

void XYPadAnimation::timerCallback()
{
    bool needsRepaint = false;

    for (auto& splash : splashes)
    {
        if (!splash.active) continue;
        needsRepaint = true;

        splash.age += 1.0f;

        if (splash.age <= splash.holdFrames)
        {
            splash.alpha = 1.0f;  // Hold phase — full opacity
        }
        else
        {
            float fadeAge = splash.age - splash.holdFrames;
            splash.alpha = 1.0f - (fadeAge / splash.fadeFrames);
        }

        if (splash.alpha <= 0.0f)
            splash.active = false;
    }

    for (auto& halo : halos)
    {
        if (!halo.active) continue;
        needsRepaint = true;

        halo.currentRadius += halo.expansionSpeed;
        halo.alpha -= halo.decayRate;

        if (halo.alpha <= 0.0f || halo.currentRadius >= halo.maxRadius)
            halo.active = false;
    }

    if (needsRepaint)
        repaint();
}

// ---- Layout ----

void XYPadAnimation::resized()
{
    // Positions are derived from PPQ at trigger time — nothing to pre-compute
}

// ---- Paint ----

void XYPadAnimation::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Clip to rounded rect matching XY pad background
    juce::Path clipPath;
    clipPath.addRoundedRectangle(bounds, 6.0f);
    g.reduceClipRegion(clipPath);

    // Layer 1: Water ripple waves — expanding thin rings with soft edges
    for (auto& halo : halos)
    {
        if (!halo.active) continue;

        float r = halo.currentRadius;
        float thickness = 2.5f;  // Thin ring

        // Draw ring with soft inner and outer edges
        // Outer edge blur
        g.setColour(halo.colour.withAlpha(halo.alpha * 0.15f));
        g.drawEllipse(halo.x - r - thickness, halo.y - r - thickness,
                      (r + thickness) * 2.0f, (r + thickness) * 2.0f, thickness);

        // Main ring
        g.setColour(halo.colour.withAlpha(halo.alpha));
        g.drawEllipse(halo.x - r, halo.y - r, r * 2.0f, r * 2.0f, thickness * 0.7f);

        // Inner edge blur
        g.setColour(halo.colour.withAlpha(halo.alpha * 0.15f));
        g.drawEllipse(halo.x - r + thickness, halo.y - r + thickness,
                      (r - thickness) * 2.0f, (r - thickness) * 2.0f, thickness);
    }

    // Layer 2: Splashes — bright dots (same size as cursor dot) with soft glow
    for (auto& splash : splashes)
    {
        if (!splash.active) continue;

        float r = splash.dotRadius;

        // Soft glow around dot
        float glowR = r * 2.5f;
        {
            juce::ColourGradient glow(
                splash.colour.withAlpha(splash.alpha * 0.35f),
                splash.x, splash.y,
                splash.colour.withAlpha(0.0f),
                splash.x + glowR, splash.y,
                true);
            g.setGradientFill(glow);
            g.fillEllipse(splash.x - glowR, splash.y - glowR,
                           glowR * 2.0f, glowR * 2.0f);
        }

        // Bright center dot (same size as cursor)
        g.setColour(splash.colour.withAlpha(splash.alpha));
        g.fillEllipse(splash.x - r, splash.y - r, r * 2.0f, r * 2.0f);
    }
}
