#include "GridsEngine.h"
#include <cstdlib>

// Original Mutable Instruments Grids pattern data (MIT License, Émilie Gillet)
// 25 nodes × 96 bytes each: 32 kick + 32 snare + 32 hihat per node
// Source: https://github.com/pichenettes/eurorack/blob/master/grids/resources.cc
const uint8_t GridsEngine::drum_patterns[25][96] = {
    // Node 0
    {255,0,0,0,0,0,145,0,0,0,0,0,218,0,0,0,72,0,36,0,182,0,0,0,109,0,0,0,72,0,0,0,
     36,0,109,0,0,0,8,0,255,0,0,0,0,0,72,0,0,0,182,0,0,0,36,0,218,0,0,0,145,0,0,0,
     170,0,113,0,255,0,56,0,170,0,141,0,198,0,56,0,170,0,113,0,226,0,28,0,170,0,113,0,198,0,85,0},
    // Node 1
    {229,0,25,0,102,0,25,0,204,0,25,0,76,0,8,0,255,0,8,0,51,0,25,0,178,0,25,0,153,0,127,0,
     28,0,198,0,56,0,56,0,226,0,28,0,141,0,28,0,28,0,170,0,28,0,28,0,255,0,113,0,85,0,85,0,
     159,0,159,0,255,0,63,0,159,0,159,0,191,0,31,0,159,0,127,0,255,0,31,0,159,0,127,0,223,0,95,0},
    // Node 2
    {255,0,0,0,127,0,0,0,0,0,102,0,0,0,229,0,0,0,178,0,204,0,0,0,76,0,51,0,153,0,25,0,
     0,0,127,0,0,0,0,0,255,0,191,0,31,0,63,0,0,0,95,0,0,0,0,0,223,0,0,0,31,0,159,0,
     255,0,85,0,148,0,85,0,127,0,85,0,106,0,63,0,212,0,170,0,191,0,170,0,85,0,42,0,233,0,21,0},
    // Node 3
    {255,0,212,0,63,0,0,0,106,0,148,0,85,0,127,0,191,0,21,0,233,0,0,0,21,0,170,0,0,0,42,0,
     0,0,0,0,141,0,113,0,255,0,198,0,0,0,56,0,0,0,85,0,56,0,28,0,226,0,28,0,170,0,56,0,
     255,0,231,0,255,0,208,0,139,0,92,0,115,0,92,0,185,0,69,0,46,0,46,0,162,0,23,0,208,0,46,0},
    // Node 4
    {255,0,31,0,63,0,63,0,127,0,95,0,191,0,63,0,223,0,31,0,159,0,63,0,31,0,63,0,95,0,31,0,
     8,0,0,0,95,0,63,0,255,0,0,0,127,0,0,0,8,0,0,0,159,0,63,0,255,0,223,0,191,0,31,0,
     76,0,25,0,255,0,127,0,153,0,51,0,204,0,102,0,76,0,51,0,229,0,127,0,153,0,51,0,178,0,102,0},
    // Node 5
    {255,0,51,0,25,0,76,0,0,0,0,0,102,0,0,0,204,0,229,0,0,0,178,0,0,0,153,0,127,0,8,0,
     178,0,127,0,153,0,204,0,255,0,0,0,25,0,76,0,102,0,51,0,0,0,0,0,229,0,25,0,25,0,204,0,
     178,0,102,0,255,0,76,0,127,0,76,0,229,0,76,0,153,0,102,0,255,0,25,0,127,0,51,0,204,0,51,0},
    // Node 6
    {255,0,0,0,223,0,0,0,31,0,8,0,127,0,0,0,95,0,0,0,159,0,0,0,95,0,63,0,191,0,0,0,
     51,0,204,0,0,0,102,0,255,0,127,0,8,0,178,0,25,0,229,0,0,0,76,0,204,0,153,0,51,0,25,0,
     255,0,226,0,255,0,255,0,198,0,28,0,141,0,56,0,170,0,56,0,85,0,28,0,170,0,28,0,113,0,56,0},
    // Node 7
    {223,0,0,0,63,0,0,0,95,0,0,0,223,0,31,0,255,0,0,0,159,0,0,0,127,0,31,0,191,0,31,0,
     0,0,0,0,109,0,0,0,218,0,0,0,182,0,72,0,8,0,36,0,145,0,36,0,255,0,8,0,182,0,72,0,
     255,0,72,0,218,0,36,0,218,0,0,0,145,0,0,0,255,0,36,0,182,0,36,0,182,0,0,0,109,0,0,0},
    // Node 8
    {255,0,0,0,218,0,0,0,36,0,0,0,218,0,0,0,182,0,109,0,255,0,0,0,0,0,0,0,145,0,72,0,
     159,0,0,0,31,0,127,0,255,0,31,0,0,0,95,0,8,0,0,0,191,0,31,0,255,0,31,0,223,0,63,0,
     255,0,31,0,63,0,31,0,95,0,31,0,63,0,127,0,159,0,31,0,63,0,31,0,223,0,223,0,191,0,191,0},
    // Node 9
    {226,0,28,0,28,0,141,0,8,0,8,0,255,0,8,0,113,0,28,0,198,0,85,0,56,0,198,0,170,0,28,0,
     8,0,95,0,8,0,8,0,255,0,63,0,31,0,223,0,8,0,31,0,191,0,8,0,255,0,127,0,127,0,159,0,
     115,0,46,0,255,0,185,0,139,0,23,0,208,0,115,0,231,0,69,0,255,0,162,0,139,0,115,0,231,0,92,0},
    // Node 10
    {145,0,0,0,0,0,109,0,0,0,0,0,255,0,109,0,72,0,218,0,0,0,0,0,36,0,0,0,182,0,0,0,
     0,0,127,0,159,0,127,0,159,0,191,0,223,0,63,0,255,0,95,0,31,0,95,0,31,0,8,0,63,0,8,0,
     255,0,0,0,145,0,0,0,182,0,109,0,109,0,109,0,218,0,0,0,72,0,0,0,182,0,72,0,182,0,36,0},
    // Node 11
    {255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,218,0,72,36,0,0,182,0,0,0,145,109,
     0,0,127,0,0,0,42,0,212,0,0,212,0,0,212,0,0,0,0,0,42,0,0,0,255,0,0,0,170,170,127,85,
     145,0,109,109,218,109,72,0,145,0,72,0,218,0,109,0,182,0,109,0,255,0,72,0,182,109,36,109,255,109,109,0},
    // Node 12
    {255,0,0,0,255,0,191,0,0,0,0,0,95,0,63,0,31,0,0,0,223,0,223,0,0,0,8,0,159,0,127,0,
     0,0,85,0,56,0,28,0,255,0,28,0,0,0,226,0,0,0,170,0,56,0,113,0,198,0,0,0,113,0,141,0,
     255,0,42,0,233,0,63,0,212,0,85,0,191,0,106,0,191,0,21,0,170,0,8,0,170,0,127,0,148,0,148,0},
    // Node 13
    {255,0,0,0,0,0,63,0,191,0,95,0,31,0,223,0,255,0,63,0,95,0,63,0,159,0,0,0,0,0,127,0,
     72,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,72,0,72,0,36,0,8,0,218,0,182,0,145,0,109,0,
     255,0,162,0,231,0,162,0,231,0,115,0,208,0,139,0,185,0,92,0,185,0,46,0,162,0,69,0,162,0,23,0},
    // Node 14
    {255,0,0,0,51,0,0,0,0,0,0,0,102,0,0,0,204,0,0,0,153,0,0,0,0,0,0,0,51,0,0,0,
     0,0,0,0,8,0,36,0,255,0,0,0,182,0,8,0,0,0,0,0,72,0,109,0,145,0,0,0,255,0,218,0,
     212,0,8,0,170,0,0,0,127,0,0,0,85,0,8,0,255,0,8,0,170,0,0,0,127,0,0,0,42,0,8,0},
    // Node 15
    {255,0,0,0,0,0,0,0,36,0,0,0,182,0,0,0,218,0,0,0,0,0,0,0,72,0,0,0,145,0,109,0,
     36,0,36,0,0,0,0,0,255,0,0,0,182,0,0,0,0,0,0,0,0,0,0,109,218,0,0,0,145,0,72,72,
     255,0,28,0,226,0,56,0,198,0,0,0,0,0,28,28,170,0,0,0,141,0,0,0,113,0,0,0,85,85,85,85},
    // Node 16
    {255,0,0,0,0,0,95,0,0,0,127,0,0,0,0,0,223,0,95,0,63,0,31,0,191,0,0,0,159,0,0,0,
     0,0,31,0,255,0,0,0,0,0,95,0,223,0,0,0,0,0,63,0,191,0,0,0,0,0,0,0,159,0,127,0,
     141,0,28,0,28,0,28,0,113,0,8,0,8,0,8,0,255,0,0,0,226,0,0,0,198,0,56,0,170,0,85,0},
    // Node 17
    {255,0,0,0,8,0,0,0,182,0,0,0,72,0,0,0,218,0,0,0,36,0,0,0,145,0,0,0,109,0,0,0,
     0,0,51,25,76,25,25,0,153,0,0,0,127,102,178,0,204,0,0,0,0,0,255,0,0,0,102,0,229,0,76,0,
     113,0,0,0,141,0,85,0,0,0,0,0,170,0,0,0,56,28,255,0,0,0,0,0,198,0,0,0,226,0,0,0},
    // Node 18
    {255,0,8,0,28,0,28,0,198,0,56,0,56,0,85,0,255,0,85,0,113,0,113,0,226,0,141,0,170,0,141,0,
     0,0,0,0,0,0,0,0,255,0,0,0,127,0,0,0,0,0,0,0,0,0,0,0,63,0,0,0,191,0,0,0,
     255,0,0,0,255,0,127,0,0,0,85,0,0,0,212,0,0,0,212,0,42,0,170,0,0,0,127,0,0,0,0,0},
    // Node 19
    {255,0,0,0,0,0,218,0,182,0,0,0,0,0,145,0,145,0,36,0,0,0,109,0,109,0,0,0,72,0,36,0,
     0,0,0,0,109,0,8,0,72,0,0,0,255,0,182,0,0,0,0,0,145,0,8,0,36,0,8,0,218,0,182,0,
     255,0,0,0,0,0,226,0,85,0,0,0,141,0,0,0,0,0,0,0,170,0,56,0,198,0,0,0,113,0,28,0},
    // Node 20
    {255,0,0,0,113,0,0,0,198,0,56,0,85,0,28,0,255,0,0,0,226,0,0,0,170,0,0,0,141,0,0,0,
     0,0,0,0,0,0,0,0,255,0,145,0,109,0,218,0,36,0,182,0,72,0,72,0,255,0,0,0,0,0,109,0,
     36,0,36,0,145,0,0,0,72,0,72,0,182,0,0,0,72,0,72,0,218,0,0,0,109,0,109,0,255,0,0,0},
    // Node 21
    {255,0,0,0,218,0,0,0,145,0,0,0,36,0,0,0,218,0,0,0,36,0,0,0,182,0,72,0,0,0,109,0,
     0,0,0,0,8,0,0,0,255,0,85,0,212,0,42,0,0,0,0,0,8,0,0,0,85,0,170,0,127,0,42,0,
     109,0,109,0,255,0,0,0,72,0,72,0,218,0,0,0,145,0,182,0,255,0,0,0,36,0,36,0,218,0,8,0},
    // Node 22
    {255,0,0,0,42,0,0,0,212,0,0,0,8,0,212,0,170,0,0,0,85,0,0,0,212,0,8,0,127,0,8,0,
     255,0,85,0,0,0,0,0,226,0,85,0,0,0,198,0,0,0,141,0,56,0,0,0,170,0,28,0,0,0,113,0,
     113,0,56,0,255,0,0,0,85,0,56,0,226,0,0,0,0,0,170,0,0,0,141,0,28,0,28,0,198,0,28,0},
    // Node 23
    {255,0,0,0,229,0,0,0,204,0,204,0,0,0,76,0,178,0,153,0,51,0,178,0,178,0,127,0,102,51,51,25,
     0,0,0,0,0,0,0,31,0,0,0,0,255,0,0,31,0,0,8,0,0,0,191,159,127,95,95,0,223,0,63,0,
     255,0,255,0,204,204,204,204,0,0,51,51,51,51,0,0,204,0,204,0,153,153,153,153,153,0,0,0,102,102,102,102},
    // Node 24
    {170,0,0,0,0,255,0,0,198,0,0,0,0,28,0,0,141,0,0,0,0,226,0,0,56,0,0,113,0,85,0,0,
     255,0,0,0,0,113,0,0,85,0,0,0,0,226,0,0,141,0,0,8,0,170,56,56,198,0,0,56,0,141,28,0,
     255,0,0,0,0,191,0,0,159,0,0,0,0,223,0,0,95,0,0,0,0,63,0,0,127,0,0,0,0,31,0,0}
};

// Original Mutable Instruments Grids drum_map: curated musical topology
// Maps grid position [x>>6][y>>6] to node index for bilinear interpolation
const uint8_t GridsEngine::drum_map[5][5] = {
    { 10,  8,  0,  9, 11 },
    { 15,  7, 13, 12,  6 },
    { 18, 14,  4,  5,  3 },
    { 23, 16, 21,  1,  2 },
    { 24, 19, 17, 20, 22 }
};

GridsEngine::GridsEngine()
{
    initialize();
}

void GridsEngine::initialize()
{
    lastTriggeredStep = 255;

    for (size_t i = 0; i < kNumChannels; ++i) {
        channels[i].noteActive = false;
        channels[i].noteOffTimer = 0.0;
    }
}

void GridsEngine::reset()
{
    lastTriggeredStep = 255;
}

void GridsEngine::setMidiNote(uint8_t channel, uint8_t note, juce::MidiBuffer* midiBuffer, uint8_t midiChannel)
{
    if (channel >= kNumChannels) return;
    
    // If the note is changing and we have an active note on this channel,
    // we need to send a NOTE OFF for the old note
    if (channels[channel].currentNote != note && channels[channel].noteActive)
    {
        if (midiBuffer != nullptr)
        {
            // Send NOTE OFF for the old note immediately
            auto noteOffMessage = juce::MidiMessage::noteOff(midiChannel, channels[channel].currentNote, (uint8_t)0);
            midiBuffer->addEvent(noteOffMessage, 0);
        }

        // Update state
        channels[channel].noteActive = false;
        channels[channel].noteOffTimer = 0.0;
    }

    // Update to new note
    channels[channel].currentNote = note;
}

uint8_t GridsEngine::readDrumPattern(uint8_t step, uint8_t channel, uint8_t x, uint8_t y) const
{
    uint8_t i = x >> 6;
    uint8_t j = y >> 6;
    if (i > 3) i = 3;
    if (j > 3) j = 3;

    uint8_t node = drum_map[i][j];
    uint8_t offset = channel * kStepsPerPattern + (step % kStepsPerPattern);

    return drum_patterns[node][offset];
}

uint8_t GridsEngine::interpolatePatterns(uint8_t step, uint8_t channel, uint8_t x, uint8_t y) const
{
    uint8_t i = x >> 6;  // 0-3 (grid column pair)
    uint8_t j = y >> 6;  // 0-3 (grid row pair)

    // Clamp so i+1 and j+1 stay within [0,4]
    if (i > 3) i = 3;
    if (j > 3) j = 3;

    // Four surrounding nodes from the curated drum map
    const uint8_t* a = drum_patterns[drum_map[i][j]];
    const uint8_t* b = drum_patterns[drum_map[i + 1][j]];
    const uint8_t* c = drum_patterns[drum_map[i][j + 1]];
    const uint8_t* d = drum_patterns[drum_map[i + 1][j + 1]];

    // Offset into node data: channel * 32 + step
    uint8_t offset = channel * kStepsPerPattern + (step % kStepsPerPattern);

    uint8_t a_val = a[offset];
    uint8_t b_val = b[offset];
    uint8_t c_val = c[offset];
    uint8_t d_val = d[offset];

    // Fractional position within the grid cell (lower 6 bits, scaled to 0-252)
    uint8_t x_frac = (x & 0x3F) << 2;
    uint8_t y_frac = (y & 0x3F) << 2;

    // Bilinear interpolation: blend horizontally, then vertically
    return mixValues(mixValues(a_val, b_val, x_frac),
                     mixValues(c_val, d_val, x_frac), y_frac);
}

uint8_t GridsEngine::mixValues(uint8_t a, uint8_t b, uint8_t balance) const
{
    // Original U8Mix: a + ((b - a) * balance) >> 8
    return static_cast<uint8_t>(a + ((static_cast<int16_t>(b) - static_cast<int16_t>(a)) * balance >> 8));
}

void GridsEngine::processBlock(juce::MidiBuffer& midiBuffer, int bufferSize,
                               double ppqPosition, double bpm, bool isPlaying, uint8_t midiChannel)
{
    // SINGLE-SOURCE PPQ TIMING (v0.4.169)
    // PPQ is the sole authority for step position — no independent step counter.
    // This eliminates drift caused by dual-control (setStepFromPPQ + processBlock both
    // competing over step_counter). Architecture matches EuclideanEngine (see clock-single-source.md).

    if (!isPlaying || bpm <= 0.0 || current_sample_rate <= 0.0) {
        return;
    }

    const double ppqPerStep = 0.25;  // 16th notes: 4 steps per quarter note
    const double ppqPerSample = bpm / (60.0 * current_sample_rate);

    // Handle negative PPQ (DAW preroll/count-in)
    if (ppqPosition < 0.0) {
        double endPpq = ppqPosition + (bufferSize * ppqPerSample);
        if (endPpq < 0.0) return;  // Entire buffer in preroll
        ppqPosition = 0.0;
    }

    for (int sampleIndex = 0; sampleIndex < bufferSize; ++sampleIndex) {
        double currentPpq = ppqPosition + (sampleIndex * ppqPerSample);
        double stepFloat = currentPpq / ppqPerStep;

        // Step 0 = strong downbeat, matching original Mutable Instruments Grids
        int rawStep = static_cast<int>(std::floor(stepFloat));
        uint8_t currentStep = static_cast<uint8_t>(rawStep % kStepsPerPattern);

        // Detect step change (prevents double-triggers)
        if (currentStep != lastTriggeredStep) {
            lastTriggeredStep = currentStep;

            // Process each channel at this step
            for (uint8_t channel = 0; channel < kNumChannels; ++channel) {
                uint8_t level = interpolatePatterns(currentStep, channel, x_coordinate, y_coordinate);

                // Apply chaos/randomization (thread-safe using thread_local)
                if (chaos_amount > 0) {
                    thread_local std::minstd_rand randomEngine(std::random_device{}());
                    std::uniform_int_distribution<int> dist(-static_cast<int>(chaos_amount), static_cast<int>(chaos_amount));
                    int perturbation = dist(randomEngine);
                    level = static_cast<uint8_t>(juce::jlimit(0, static_cast<int>(MAX_PATTERN_LEVEL), static_cast<int>(level) + perturbation));
                }

                // Check density threshold
                bool should_trigger = level > (MAX_PATTERN_LEVEL - channel_density[channel]);

                if (should_trigger) {
                    if (channels[channel].noteActive) {
                        sendNoteOff(midiBuffer, sampleIndex, channel, midiChannel);
                    }

                    uint8_t velocity = juce::jmap(level, (uint8_t)0, MAX_PATTERN_LEVEL, MIN_VELOCITY, MAX_VELOCITY);
                    sendNoteOn(midiBuffer, sampleIndex, channel, velocity, midiChannel);
                }
            }
        }
    }
}

void GridsEngine::sendNoteOff(juce::MidiBuffer& buffer, int sampleOffset, uint8_t channel, uint8_t midiChannel)
{
    auto noteOffMessage = juce::MidiMessage::noteOff(midiChannel, channels[channel].currentNote, (uint8_t)0);
    buffer.addEvent(noteOffMessage, sampleOffset);
    channels[channel].noteActive = false;
}

void GridsEngine::sendNoteOn(juce::MidiBuffer& buffer, int sampleOffset, uint8_t channel, uint8_t velocity, uint8_t midiChannel)
{
    auto noteOnMessage = juce::MidiMessage::noteOn(midiChannel, channels[channel].currentNote, velocity);
    buffer.addEvent(noteOnMessage, sampleOffset);
    channels[channel].noteActive = true;
}

//=============================================================================

// Transport-aware reset for DAW sync
void GridsEngine::resetForTransport() {
    // Reset step tracking so first step triggers cleanly on transport start
    lastTriggeredStep = 255;

    // Clear note states without sending note-offs (transparent)
    for (size_t i = 0; i < kNumChannels; ++i) {
        channels[i].noteActive = false;
        channels[i].noteOffTimer = 0.0;
    }
}

// NOTE: setStepFromPPQ is now an inline no-op in the header.
// Single-source PPQ timing passes ppqPosition directly to processBlock.