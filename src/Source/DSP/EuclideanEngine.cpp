#include "EuclideanEngine.h"
#include <algorithm>
#include <cmath>

// Define static const members (required for ODR-use in std::max/std::min)
const uint8_t EuclideanEngine::kMinSteps;
const uint8_t EuclideanEngine::kMaxSteps;

// Default MIDI notes: BD=36 (C1), SN=38 (D1), HH=42 (F#1), then accents
const std::array<uint8_t, EuclideanEngine::kNumChannels> EuclideanEngine::defaultMidiNotes = {
    36, 38, 42,  // BD, SN, HH
    37, 40, 46   // BD_Acc, SN_Acc, HH_Acc (different notes for accents)
};

EuclideanEngine::EuclideanEngine()
{
    // Initialize channels with default MIDI notes
    for (uint8_t i = 0; i < kNumChannels; ++i) {
        channels[i].currentNote = defaultMidiNotes[i];
        channels[i].pattern.resize(kMaxSteps, false);
    }
}

void EuclideanEngine::initialize()
{
    reset();

    // Generate initial patterns for all channels
    for (uint8_t i = 0; i < kNumChannels; ++i) {
        regeneratePattern(i);
    }
}

void EuclideanEngine::reset()
{
    for (auto& ch : channels) {
        ch.step_counter = 0;
        ch.pendingReset = false;
        ch.lastTriggeredStep = 0xFF;  // Reset to allow triggering any step
    }
}

void EuclideanEngine::resetForTransport()
{
    for (auto& ch : channels) {
        ch.pendingReset = true;
    }
}

void EuclideanEngine::resetChannelForTransport(uint8_t channel)
{
    if (channel < kNumChannels) {
        channels[channel].pendingReset = true;
    }
}

uint8_t EuclideanEngine::getCurrentStep(uint8_t channel) const
{
    if (channel >= kNumChannels) return 0;
    return channels[channel].step_counter % 32;
}

void EuclideanEngine::setSteps(uint8_t channel, uint8_t steps)
{
    if (channel >= kNumChannels) return;

    // Clamp to valid range
    steps = std::max(kMinSteps, std::min(kMaxSteps, steps));

    if (channels[channel].steps != steps) {
        channels[channel].steps = steps;

        // Ensure pulses don't exceed steps
        if (channels[channel].pulses > steps) {
            channels[channel].pulses = steps;
        }

        // Clamp startOn to new valid range
        if (channels[channel].startOn > steps) {
            channels[channel].startOn = steps;
        }

        channels[channel].patternDirty = true;
    }
}

void EuclideanEngine::setPulses(uint8_t channel, uint8_t pulses)
{
    if (channel >= kNumChannels) return;

    // Clamp to valid range (0 to current steps)
    pulses = std::min(pulses, channels[channel].steps);

    if (channels[channel].pulses != pulses) {
        channels[channel].pulses = pulses;
        channels[channel].patternDirty = true;
    }
}

void EuclideanEngine::setStartOn(uint8_t channel, uint8_t startOn)
{
    if (channel >= kNumChannels) return;

    // Clamp: minimum 1, maximum = current steps
    startOn = std::max(static_cast<uint8_t>(1), std::min(channels[channel].steps, startOn));

    if (channels[channel].startOn != startOn) {
        channels[channel].startOn = startOn;
        channels[channel].patternDirty = true;
    }
}

void EuclideanEngine::setMidiNote(uint8_t channel, uint8_t note)
{
    if (channel >= kNumChannels) {
        return;
    }
    channels[channel].currentNote = note;
}

void EuclideanEngine::regeneratePattern(uint8_t channel) const
{
    if (channel >= kNumChannels) return;

    auto& ch = channels[channel];

    // Generate base Euclidean rhythm using Bjorklund algorithm
    ch.pattern = generateEuclideanRhythm(ch.steps, ch.pulses);

    // Apply START ON rotation: START ON 1 = no rotation, START ON N = right-shift by (N-1)
    if (ch.startOn > 1) {
        applyStartOn(ch.pattern, ch.startOn, ch.steps);
    }

    // Pad pattern to kMaxSteps for consistent indexing
    ch.pattern.resize(kMaxSteps, false);

    ch.patternDirty = false;
}

/**
 * @brief Bjorklund algorithm for Euclidean rhythm generation
 *
 * Distributes 'pulses' evenly across 'steps' using the Euclidean algorithm.
 * This is the classic Bjorklund pairing algorithm that creates maximally
 * even distributions.
 *
 * @param steps Total number of steps in pattern (2-32)
 * @param pulses Number of active beats (0-steps)
 * @return Pattern as vector of bools (true = hit, false = rest)
 */
std::vector<bool> EuclideanEngine::generateEuclideanRhythm(uint8_t steps, uint8_t pulses) const
{
    std::vector<bool> pattern(steps, false);

    // Edge cases
    if (pulses == 0 || steps == 0) {
        return pattern;  // All rests
    }

    if (pulses >= steps) {
        // All hits
        std::fill(pattern.begin(), pattern.end(), true);
        return pattern;
    }

    // Bresenham's line algorithm approach (equivalent to Bjorklund)
    // Initialize bucket so the first hit always lands at position 0
    int bucket = steps - pulses;
    for (int i = 0; i < steps; ++i) {
        bucket += pulses;
        if (bucket >= steps) {
            bucket -= steps;
            pattern[i] = true;
        } else {
            pattern[i] = false;
        }
    }

    return pattern;
}

void EuclideanEngine::applyStartOn(std::vector<bool>& pattern, uint8_t startOn, uint8_t steps) const
{
    if (pattern.empty() || startOn <= 1) return;

    const int size = static_cast<int>(pattern.size());
    // Right-shift by (startOn - 1): equivalent to left-shift by (size - (startOn - 1))
    int rotation = size - ((startOn - 1) % size);
    if (rotation == size) rotation = 0;

    if (rotation > 0) {
        std::rotate(pattern.begin(), pattern.begin() + rotation, pattern.end());
    }
}

bool EuclideanEngine::isStepActive(uint8_t channel, uint8_t step) const
{
    if (channel >= kNumChannels || step >= kMaxSteps) {
        return false;
    }

    // Regenerate if dirty
    if (channels[channel].patternDirty) {
        regeneratePattern(channel);
    }

    // Check if step is within active pattern length
    if (step >= channels[channel].steps) {
        return false;
    }

    return channels[channel].pattern[step];
}

void EuclideanEngine::processBlock(juce::MidiBuffer& midiBuffer, int bufferSize,
                                   double ppqPosition, double bpm, bool isPlaying,
                                   uint8_t midiChannel, uint8_t velocityOverride)
{
    // Process all channels together
    // Calls processChannelBlock for each channel
    for (uint8_t ch = 0; ch < kNumChannels; ++ch) {
        processChannelBlock(ch, midiBuffer, bufferSize, ppqPosition, bpm,
                           isPlaying, midiChannel, velocityOverride);
    }
}

/**
 * @brief Single-source PPQ timing implementation
 *
 * This implementation follows the ppq-drift-solution.md recommendation:
 * PPQ is the SOLE AUTHORITY for step position. Step is calculated directly
 * from PPQ for each sample - NO independent step counter increment.
 *
 * Key insight: step_counter is never incremented independently. Every sample's
 * step position derives mathematically from ppqPosition + (sample × ppqPerSample).
 */
void EuclideanEngine::processChannelBlock(uint8_t channel, juce::MidiBuffer& midiBuffer,
                                         int bufferSize, double ppqPosition, double bpm,
                                         bool isPlaying, uint8_t midiChannel,
                                         uint8_t velocityOverride)
{
    if (!isPlaying || bpm <= 0.0 || current_sample_rate <= 0.0 || channel >= kNumChannels) {
        return;
    }

    auto& ch = channels[channel];

    // Regenerate pattern if parameters changed
    if (ch.patternDirty) {
        regeneratePattern(channel);
    }

    // SINGLE SOURCE OF TRUTH: Calculate step from PPQ
    const double ppqPerStep = 0.25;  // 16th notes (1 step = 1/4 quarter note)
    const double ppqPerSample = bpm / (60.0 * current_sample_rate);

    // Handle negative PPQ (DAW preroll)
    if (ppqPosition < 0.0) {
        double endPpq = ppqPosition + (bufferSize * ppqPerSample);
        if (endPpq < 0.0) {
            return;  // Entire buffer is in negative PPQ territory
        }
        // Adjust starting point to where PPQ becomes non-negative
        // We'll process from sample 0 but with corrected PPQ
        ppqPosition = 0.0;
    }

    // Process each sample with PPQ-derived position
    for (int sample = 0; sample < bufferSize; ++sample) {
        double currentPpq = ppqPosition + (sample * ppqPerSample);
        double stepFloat = currentPpq / ppqPerStep;

        // Step 0 = downbeat at transport start (Euclidean patterns start at position 0)
        int rawStep = static_cast<int>(std::floor(stepFloat));

        // Wrap to pattern length
        uint8_t currentStep = static_cast<uint8_t>(rawStep % ch.steps);

        // Detect step change and trigger if active
        if (currentStep != ch.lastTriggeredStep) {
            if (isStepActive(channel, currentStep)) {
                uint8_t velocity = velocityOverride > 0 ? velocityOverride : 100;
                sendNoteOn(midiBuffer, sample, channel, velocity, midiChannel);
            }
            ch.lastTriggeredStep = currentStep;
        }
    }

    // Clear pending reset flag (transport has started)
    ch.pendingReset = false;
}

void EuclideanEngine::sendNoteOn(juce::MidiBuffer& buffer, int sampleOffset,
                                uint8_t channel, uint8_t velocity, uint8_t midiChannel)
{
    if (channel >= kNumChannels) return;

    const uint8_t note = channels[channel].currentNote;

    // Clamp velocity to valid MIDI range
    velocity = std::max(uint8_t(1), std::min(uint8_t(127), velocity));

    // Create and add MIDI note-on message
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(
        static_cast<int>(midiChannel),
        static_cast<int>(note),
        static_cast<uint8_t>(velocity)
    );

    buffer.addEvent(noteOn, sampleOffset);
}
