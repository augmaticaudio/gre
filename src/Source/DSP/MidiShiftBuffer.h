#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <deque>
#include <mutex>
#include <random>

/**
 * MidiShiftBuffer - Time-delay buffer for MIDI events with shift and humanization
 *
 * NEW VERSION (v0.3.487+): Bipolar slider with 127 values (0-126, 63 = OFF/center)
 *
 * Key Features:
 * - Bipolar shift range: 0-62 = advance (negative offset), 63 = OFF, 64-126 = delay
 * - PPQ-based timing calculation for musical synchronization
 * - Per-event humanization with stored offsets (for NOTE ON/OFF matching)
 * - Thread-safe event queue with mutex protection
 * - Supports both NOTE ON and NOTE OFF events
 *
 * CRITICAL: NOTE ON and NOTE OFF must receive IDENTICAL timing offsets.
 * This is achieved by:
 * 1. addMidiEvent() returns the humanization offset it applied
 * 2. Caller stores this offset in PendingNoteOff structure
 * 3. addMidiEventWithHumanization() reuses the stored offset for NOTE OFF
 */
class MidiShiftBuffer {
public:
    struct ShiftedMidiEvent {
        juce::MidiMessage message;
        int64_t outputSamplePosition;
        int originalChannel;
    };

private:
    std::deque<ShiftedMidiEvent> eventQueue;
    mutable std::mutex queueMutex;

    double sampleRate = 44100.0;
    double currentBPM = 120.0;
    int shiftAmount = 63;  // 0-126 (63 = OFF/center, bipolar slider)
    int humanizationAmount = 0;  // 0-127
    int64_t currentSamplePosition = 0;

    // Random number generation for humanization
    std::mt19937 rng{std::random_device{}()};
    std::normal_distribution<float> humanizeDistribution{0.0f, 1.0f};

    static constexpr size_t MAX_QUEUE_SIZE = 10000;
    static constexpr int64_t MAX_SHIFT_SAMPLES = 441000;  // 10 seconds @ 44.1kHz

    // PPQ Mapping Table for bipolar slider (127 values, index 63 = OFF)
    // Advance values (0-62): PPQ 975-1905 in steps of 15
    // Center (63): PPQ 0 (OFF)
    // Delay values (64-126): PPQ 15-945 in steps of 15
    static constexpr int ppqTable[127] = {
        // Advance values (0-62): PPQ values 975-1905 in steps of 15
        975, 990, 1005, 1020, 1035, 1050, 1065, 1080, 1095, 1110,  // 0-9
        1125, 1140, 1155, 1170, 1185, 1200, 1215, 1230, 1245, 1260, // 10-19
        1275, 1290, 1305, 1320, 1335, 1350, 1365, 1380, 1395, 1410, // 20-29
        1425, 1440, 1455, 1470, 1485, 1500, 1515, 1530, 1545, 1560, // 30-39
        1575, 1590, 1605, 1620, 1635, 1650, 1665, 1680, 1695, 1710, // 40-49
        1725, 1740, 1755, 1770, 1785, 1800, 1815, 1830, 1845, 1860, // 50-59
        1875, 1890, 1905,                                            // 60-62
        0,  // 63: OFF (center position)
        // Delay values (64-126): PPQ values 15-945 in steps of 15
        15, 30, 45, 60, 75, 90, 105, 120, 135, 150,                 // 64-73
        165, 180, 195, 210, 225, 240, 255, 270, 285, 300,           // 74-83
        315, 330, 345, 360, 375, 390, 405, 420, 435, 450,           // 84-93
        465, 480, 495, 510, 525, 540, 555, 570, 585, 600,           // 94-103
        615, 630, 645, 660, 675, 690, 705, 720, 735, 750,           // 104-113
        765, 780, 795, 810, 825, 840, 855, 870, 885, 900,           // 114-123
        915, 930, 945                                                // 124-126
    };

    // Calculate shift in samples using PPQ-based timing
    int64_t calculateShiftSamples() const {
        int ppqValue = ppqTable[juce::jlimit(0, 126, shiftAmount)];

        if (ppqValue == 0) return 0;

        // Convert PPQ to samples: ppqValue / 1920 gives us the fraction of a beat
        // Using 1920 PPQ per quarter note (standard)
        static const int PPQ_PER_QUARTER = 1920;
        double beatsShift = static_cast<double>(ppqValue) / static_cast<double>(PPQ_PER_QUARTER);
        double samplesPerBeat = (sampleRate * 60.0) / currentBPM;
        return static_cast<int64_t>(beatsShift * samplesPerBeat);
    }

    // Calculate humanization in samples from 0-127 parameter
    // Returns a RANDOM offset each time it's called
    int64_t calculateHumanizationSamples() {
        if (humanizationAmount == 0) return 0;

        // Map 0-127 to 0-8th note duration (1/8 beat) for musical feel
        double samplesPerBeat = (sampleRate * 60.0) / currentBPM;
        double maxVariationSamples = (humanizationAmount / 127.0) * (samplesPerBeat / 8.0);

        // Apply normal distribution for natural feel
        float variation = humanizeDistribution(rng);
        return static_cast<int64_t>(variation * maxVariationSamples);
    }

    void enforceQueueLimits() {
        // Remove events too far in the past
        int64_t cutoffPosition = currentSamplePosition - MAX_SHIFT_SAMPLES;

        eventQueue.erase(
            std::remove_if(eventQueue.begin(), eventQueue.end(),
                [cutoffPosition](const ShiftedMidiEvent& e) {
                    return e.outputSamplePosition < cutoffPosition;
                }),
            eventQueue.end()
        );

        // Hard limit on queue size
        while (eventQueue.size() > MAX_QUEUE_SIZE) {
            eventQueue.pop_front();
        }
    }

public:
    MidiShiftBuffer(double sr = 44100.0) : sampleRate(sr) {}

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    // Set shift amount (0-126, where 63 is center/OFF)
    void setShiftAmount(int amount) {
        shiftAmount = juce::jlimit(0, 126, amount);
    }

    // Set humanization amount (0-127)
    void setHumanizationAmount(int amount) {
        humanizationAmount = juce::jlimit(0, 127, amount);
    }

    // Update tempo for shift calculation
    void setBPM(double bpm) {
        currentBPM = bpm;
    }

    /**
     * Add MIDI event to shift buffer with shift and humanization
     *
     * CRITICAL: Returns the humanization offset applied (in samples)
     * This allows the caller to store it and reuse for NOTE OFF to ensure
     * NOTE ON and NOTE OFF receive IDENTICAL timing offsets.
     *
     * @param msg MIDI message (NOTE ON or NOTE OFF)
     * @param samplePosition Position within current buffer
     * @param channel MIDI channel number
     * @return Humanization offset applied (in samples) - MUST be stored for NOTE OFF
     */
    int64_t addMidiEvent(const juce::MidiMessage& msg, int samplePosition, int channel) {
        std::lock_guard<std::mutex> lock(queueMutex);

        int64_t shiftSamples = calculateShiftSamples();
        int64_t humanizationSamples = calculateHumanizationSamples();  // RANDOM value
        int64_t totalShift = shiftSamples + humanizationSamples;

        // Ensure shift doesn't go negative
        totalShift = std::max(int64_t(0), totalShift);

        int64_t outputPosition = currentSamplePosition + samplePosition + totalShift;

        ShiftedMidiEvent event;
        event.message = msg;
        event.outputSamplePosition = outputPosition;
        event.originalChannel = channel;

        eventQueue.push_back(event);
        enforceQueueLimits();

        // Return the humanization offset so caller can store it for NOTE OFF
        return humanizationSamples;
    }

    /**
     * Add MIDI event with specific humanization offset (for NOTE OFF)
     *
     * This ensures NOTE OFF uses the SAME humanization as its corresponding NOTE ON.
     * The humanization offset should be the value returned by addMidiEvent() for the NOTE ON.
     *
     * @param msg MIDI message (typically NOTE OFF)
     * @param samplePosition Position within current buffer
     * @param channel MIDI channel number
     * @param humanizationOffset Humanization offset from NOTE ON (in samples)
     */
    void addMidiEventWithHumanization(
        const juce::MidiMessage& msg,
        int samplePosition,
        int channel,
        int64_t humanizationOffset
    ) {
        std::lock_guard<std::mutex> lock(queueMutex);

        int64_t shiftSamples = calculateShiftSamples();
        int64_t totalShift = shiftSamples + humanizationOffset;  // Use provided offset, not new random value

        // Ensure shift doesn't go negative
        totalShift = std::max(int64_t(0), totalShift);

        int64_t outputPosition = currentSamplePosition + samplePosition + totalShift;

        ShiftedMidiEvent event;
        event.message = msg;
        event.outputSamplePosition = outputPosition;
        event.originalChannel = channel;

        eventQueue.push_back(event);
        enforceQueueLimits();
    }

    // Process and output shifted events
    void processShiftedEvents(juce::MidiBuffer& outputBuffer, int numSamples) {
        std::lock_guard<std::mutex> lock(queueMutex);

        int64_t bufferEndPosition = currentSamplePosition + numSamples;

        auto it = eventQueue.begin();
        while (it != eventQueue.end()) {
            if (it->outputSamplePosition >= currentSamplePosition &&
                it->outputSamplePosition < bufferEndPosition) {

                // Calculate relative position within this buffer
                int relativeSamplePos = static_cast<int>(
                    it->outputSamplePosition - currentSamplePosition
                );

                // Clamp to buffer bounds
                relativeSamplePos = juce::jlimit(0, numSamples - 1, relativeSamplePos);

                // Add to output buffer
                outputBuffer.addEvent(it->message, relativeSamplePos);

                // Remove processed event
                it = eventQueue.erase(it);
            } else if (it->outputSamplePosition < currentSamplePosition) {
                // Event is in the past (shouldn't happen, but handle gracefully)
                it = eventQueue.erase(it);
            } else {
                // Event is in the future, keep it
                ++it;
            }
        }

        // Update position for next buffer
        currentSamplePosition += numSamples;
    }

    // Clear all pending events (e.g., on transport stop)
    void clear() {
        std::lock_guard<std::mutex> lock(queueMutex);
        eventQueue.clear();
    }

    // Reset on transport start
    void reset() {
        clear();
        currentSamplePosition = 0;
    }

    // Sync to absolute sample position (for DAW timeline alignment)
    void setSamplePosition(int64_t position) {
        currentSamplePosition = position;
    }

    // Get current queue size (for debugging/monitoring)
    size_t getQueueSize() const {
        std::lock_guard<std::mutex> lock(queueMutex);
        return eventQueue.size();
    }
};
