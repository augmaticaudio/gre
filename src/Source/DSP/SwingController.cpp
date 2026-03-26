#include "SwingController.h"
#include <cmath>

void SwingController::setSwingAmount(float amount) {
    // Clamp to valid range: -99 to +99 (intentionally avoiding ±100 to prevent beat collisions)
    swingAmount = juce::jlimit(-99.0f, 99.0f, amount);
}

void SwingController::reset() {
    // Currently no state to reset, but provided for API consistency
    // Future: Could track phase for more complex swing patterns
}

double SwingController::applySwingToPPQ(double ppqPosition, double bpm, float clockRatio) const {
    // If no swing or invalid PPQ, return unchanged
    if (std::abs(swingAmount) < 0.01f || ppqPosition < 0.0) {
        return ppqPosition;
    }

    // Clock-aware Roger Linn swing algorithm
    //
    // Swing grid matches REAL 16th notes, not engine steps.
    // In modifiedPPQ space, real 16th notes are 0.25 * clockRatio apart.
    //   x1: ppqPerSwingUnit = 0.25   (standard 16th notes)
    //   x2: ppqPerSwingUnit = 0.5    (2 engine steps = 1 real 16th note)
    //   /2: ppqPerSwingUnit = 0.125  (engine steps are real 8th notes — no swing)
    const double safeRatio = (clockRatio > 0.0f) ? static_cast<double>(clockRatio) : 1.0;
    const double ppqPerSwingUnit = 0.25 * safeRatio;

    // Get the step index in the scaled grid
    const double stepPosition = ppqPosition / ppqPerSwingUnit;
    const int stepIndex = static_cast<int>(std::floor(stepPosition + 0.001));  // Small epsilon for rounding

    // Check if this is an even step (0, 2, 4, 6, 8...)
    const bool isEven = (stepIndex % 2) == 0;

    // Roger Linn swing algorithm:
    // - Positive swing: delay EVEN steps (indices 0, 2, 4, 6...)
    // - Negative swing: delay ODD steps (indices 1, 3, 5, 7...) to create opposite feel
    // NEVER move notes backward in time - always delay forward!

    bool shouldApplySwing;
    double effectiveSwingAmount;

    if (swingAmount >= 0.0f) {
        // Positive swing: affect even steps
        shouldApplySwing = isEven;
        effectiveSwingAmount = swingAmount;
    } else {
        // Negative swing: affect odd steps, use absolute value for delay
        shouldApplySwing = !isEven;
        effectiveSwingAmount = -swingAmount;  // Convert to positive for forward delay
    }

    if (!shouldApplySwing) {
        return ppqPosition;
    }

    // Apply forward delay only (never move backward)
    const double swingRatio = effectiveSwingAmount / 100.0;
    const double swingOffsetPPQ = swingRatio * (ppqPerSwingUnit * 0.5);  // 50% of swing unit spacing

    return ppqPosition + swingOffsetPPQ;
}
