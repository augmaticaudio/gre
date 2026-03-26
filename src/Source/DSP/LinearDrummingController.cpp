#include "LinearDrummingController.h"

void LinearDrummingController::initialize(double sampleRate) {
    sampleRate_ = sampleRate;

    // Initialize all instruments to Priority 1 by default
    for (int i = 0; i < 6; ++i) {
        instrumentPriorities_[i] = 0; // Priority 1 (index 0)
    }

    // Clear all mute windows
    for (auto& window : channelMuteWindows_) {
        window.active = false;
        window.startSample = -1;
        window.endSample = -1;
        window.triggeringChannel = -1;
    }
}

void LinearDrummingController::updateParameters(bool enabled, int gridDuration, const std::array<int, 6>& instrumentPriorities) {
    enabled_ = enabled;

    // Validate grid duration index (0-4: 64n, 32n, 16n, 8n, 4n)
    if (gridDuration < 0 || gridDuration >= 5) {
        gridDurationIndex_ = 2; // Default to 16n
    } else {
        gridDurationIndex_ = gridDuration;
    }

    // Copy instrument priorities
    // instrumentPriorities[i] contains the priority for instrument i (0-5 maps to Priority 1-6)
    instrumentPriorities_ = instrumentPriorities;
}

void LinearDrummingController::processLinearDrumming(std::vector<PendingNote>& notes, int64_t globalSamplePosition, double currentBPM) {
    if (!enabled_ || notes.empty()) return;

    // 1. Update existing mute windows
    updateMuteWindows(globalSamplePosition);

    // 2. PERFORMANCE OPTIMIZATION: Use O(n²) algorithm instead of map allocation
    // This is more efficient for small note counts (typically 1-6 notes per buffer)
    //
    // CRITICAL FIX: Use sample tolerance for simultaneity detection
    // Grids and Euclidean engines may generate notes at slightly different sample positions
    // even when they should be considered "simultaneous" for priority resolution
    // Increased from 4 to 64 samples after testing revealed larger timing differences
    // 64 samples = ~1.45ms at 44.1kHz (still imperceptible, well below 10ms rhythm perception threshold)
    const int SIMULTANEITY_TOLERANCE = 64;

    for (size_t i = 0; i < notes.size(); ++i) {
        if (notes[i].isMuted) continue; // Skip already muted notes

        // Check if this note is muted by existing mute windows
        if (isChannelMuted(notes[i].channelIndex, globalSamplePosition + notes[i].sampleOffset)) {
            notes[i].isMuted = true;
            continue;
        }

        // Find all simultaneous notes (within tolerance window)
        std::vector<size_t> simultaneousNotes;
        simultaneousNotes.push_back(i);

        for (size_t j = i + 1; j < notes.size(); ++j) {
            if (!notes[j].isMuted) {
                // Check if notes are within simultaneity tolerance
                int sampleDiff = std::abs(notes[j].sampleOffset - notes[i].sampleOffset);
                if (sampleDiff <= SIMULTANEITY_TOLERANCE) {
                    simultaneousNotes.push_back(j);
                }
            }
        }

        // If multiple notes at same time (or very close), apply priority logic
        if (simultaneousNotes.size() > 1) {
            applyPriorityLogic(simultaneousNotes, notes, globalSamplePosition + notes[i].sampleOffset, currentBPM);
        }
    }
}

void LinearDrummingController::applyPriorityLogic(const std::vector<size_t>& noteIndices,
                                                  std::vector<PendingNote>& notes,
                                                  int64_t samplePosition,
                                                  double currentBPM) {
    if (noteIndices.size() <= 1) return; // No conflict if only one note

    // Find the highest priority among all playing notes
    int highestPriority = 6; // Start with lowest priority
    std::vector<size_t> winningIndices;

    for (size_t idx : noteIndices) {
        if (idx >= notes.size()) continue; // Bounds check
        int channelIndex = notes[idx].channelIndex;
        if (channelIndex < 0 || channelIndex >= 6) continue; // Bounds check

        int priority = instrumentPriorities_[channelIndex];

        if (priority < highestPriority) {
            // Found a higher priority - clear previous winners
            highestPriority = priority;
            winningIndices.clear();
            winningIndices.push_back(idx);
        } else if (priority == highestPriority) {
            // Same priority - add to winners (priority group)
            winningIndices.push_back(idx);
        }
    }

    // NEW LOGIC: All notes in the highest priority group pass through
    // All notes with lower priority get muted
    int64_t gridDuration = calculateGridDurationSamples(currentBPM);

    for (size_t idx : noteIndices) {
        if (idx >= notes.size()) continue;

        // Check if this note is in the winning priority group
        bool isWinner = std::find(winningIndices.begin(), winningIndices.end(), idx) != winningIndices.end();

        if (!isWinner) {
            // Mute this note and create mute window
            notes[idx].isMuted = true;

            int channelIdx = notes[idx].channelIndex;
            if (channelIdx >= 0 && channelIdx < 6) {
                channelMuteWindows_[channelIdx].startSample = samplePosition;
                channelMuteWindows_[channelIdx].endSample = samplePosition + gridDuration;
                channelMuteWindows_[channelIdx].active = true;

                // Use the first winning channel as the triggering channel
                if (!winningIndices.empty() && winningIndices[0] < notes.size()) {
                    int winningChannelIdx = notes[winningIndices[0]].channelIndex;
                    if (winningChannelIdx >= 0 && winningChannelIdx < 6) {
                        channelMuteWindows_[channelIdx].triggeringChannel = winningChannelIdx;
                    }
                }
            }
        }
    }
}

int64_t LinearDrummingController::calculateGridDurationSamples(double bpm) const {
    if (bpm <= 0.0) bpm = 120.0; // Fallback BPM

    // Bounds check for grid duration index
    int safeGridIndex = gridDurationIndex_;
    if (safeGridIndex < 0 || safeGridIndex >= 5) {
        safeGridIndex = 2; // Default to 16n
    }

    double quarterNoteDuration = 60.0 / bpm; // Seconds per quarter note
    double gridDuration = quarterNoteDuration * GRID_DURATIONS_QUARTERS[safeGridIndex];
    return static_cast<int64_t>(gridDuration * sampleRate_);
}

bool LinearDrummingController::isChannelMuted(int channelIndex, int64_t samplePosition) {
    if (channelIndex < 0 || channelIndex >= 6) return false;

    const auto& window = channelMuteWindows_[channelIndex];
    return window.active && samplePosition >= window.startSample && samplePosition < window.endSample;
}

void LinearDrummingController::updateMuteWindows(int64_t currentSample) {
    for (auto& window : channelMuteWindows_) {
        if (window.active && currentSample >= window.endSample) {
            window.active = false;
        }
    }
}

void LinearDrummingController::reset() {
    for (auto& window : channelMuteWindows_) {
        window.active = false;
        window.startSample = -1;
        window.endSample = -1;
        window.triggeringChannel = -1;
    }
}