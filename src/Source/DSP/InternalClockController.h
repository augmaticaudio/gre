#pragma once

#include <juce_core/juce_core.h>
#include "ClockInfo.h"

/**
 * @class InternalClockController
 * @brief Generates high-precision PPQ-based timing for Augmatic GRE's standalone mode
 *
 * This class provides an internal clock that generates PPQ (Pulses Per Quarter Note)
 * timing messages identical to those received from a DAW host. It supports:
 * - BPM range: 20.0 - 400.0 with 0.1 decimal precision
 * - Sample-accurate PPQ calculation
 * - Start/Stop transport control
 * - Thread-safe operation in audio thread
 *
 * The PPQ calculation uses the formula:
 *   PPQ_increment = (numSamples / sampleRate) * (BPM / 60.0)
 *
 * @version 0.3.448
 * @date 2025-10-21
 */
class InternalClockController {
public:
    InternalClockController() = default;
    ~InternalClockController() = default;

    /**
     * @brief Set the sample rate for PPQ calculation
     * @param sampleRate Sample rate in Hz (e.g., 44100.0, 48000.0)
     */
    void setSampleRate(double sampleRate);

    /**
     * @brief Set the BPM (Beats Per Minute)
     * @param bpm Tempo in BPM, automatically clamped to 20.0 - 400.0 range
     */
    void setBPM(double bpm);

    /**
     * @brief Set the playing state and control transport
     * @param isPlaying true to start clock, false to stop
     * @note Starting from stopped state resets PPQ position to 0.0
     */
    void setPlaying(bool isPlaying);

    /**
     * @brief Update clock position based on elapsed samples (called each processBlock)
     * @param numSamples Number of samples processed in this buffer
     * @note Only increments PPQ when playing is true
     */
    void updateClock(int numSamples);

    /**
     * @brief Get current clock information in ClockInfo format
     * @return ClockInfo structure compatible with MasterSyncController
     */
    ClockInfo getClockInfo() const;

    /**
     * @brief Check if clock is currently playing
     * @return true if playing, false if stopped
     */
    bool isPlaying() const { return playing; }

    /**
     * @brief Get current BPM setting
     * @return Current BPM value
     */
    double getBPM() const { return currentBPM; }

    /**
     * @brief Get current PPQ position
     * @return Current PPQ (Pulses Per Quarter Note) position
     */
    double getPPQPosition() const { return ppqPosition; }

    /**
     * @brief Reset PPQ position to 0.0
     * @note Useful for restarting playback from the beginning
     */
    void reset();

private:
    double currentBPM = 120.0;      ///< Current tempo in BPM (20.0 - 400.0)
    double ppqPosition = 0.0;       ///< Current PPQ position
    double sampleRate = 44100.0;    ///< Sample rate in Hz
    bool playing = false;           ///< Transport state (playing/stopped)

    /**
     * @brief Calculate PPQ increment for given number of samples
     * @param numSamples Number of samples to calculate for
     * @return PPQ increment value
     *
     * Formula:
     *   samples_per_quarter_note = (60.0 / BPM) * sampleRate
     *   ppq_increment = numSamples / samples_per_quarter_note
     */
    double calculatePPQIncrement(int numSamples) const;
};
