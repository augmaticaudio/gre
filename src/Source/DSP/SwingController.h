#pragma once

#include <juce_core/juce_core.h>

/**
 * @class SwingController
 * @brief Implements Roger Linn swing algorithm for clock timing modification
 *
 * Key principle: Swing lives in the CLOCK SIGNAL, not in pattern algorithms.
 * The pattern generator (Grids/Euclidean) receives timing that is already swung.
 */
class SwingController {
public:
    SwingController() = default;
    ~SwingController() = default;

    /**
     * @brief Set the swing amount
     * @param amount Swing percentage from -99 to +99
     *               0 = no swing (straight timing)
     *               +66 = perfect triplet swing
     *               +99 = maximum forward swing
     *               -99 = maximum backward swing
     */
    void setSwingAmount(float amount);

    /**
     * @brief Get current swing amount
     * @return Swing amount from -99 to +99
     */
    float getSwingAmount() const { return swingAmount; }

    /**
     * @brief Apply swing to a PPQ position
     * @param ppqPosition Unswung PPQ position from DAW/clock
     * @param bpm Current tempo in BPM
     * @param clockRatio Effective clock multiplier (>1 = faster, <1 = slower, 1.0 = normal)
     *                   Scales the swing grid so swing works correctly at any clock ratio.
     * @return PPQ position with swing applied
     */
    double applySwingToPPQ(double ppqPosition, double bpm, float clockRatio = 1.0f) const;

    /**
     * @brief Reset internal state (call when transport starts)
     */
    void reset();

private:
    float swingAmount = 0.0f;  ///< Current swing amount (-99 to +99)
};
