#pragma once

#include <cstddef>

// Channel Configuration Constants
static constexpr const char* CHANNEL_NAMES[] = {"bd", "sn", "hh", "bd_acc", "sn_acc", "hh_acc"};
static constexpr const char* CHANNEL_DISPLAY_NAMES[] = {"BD", "SN", "HH", "BD Acc", "SN Acc", "HH Acc"};
static constexpr const char* MAIN_CHANNEL_NAMES[] = {"bd", "sn", "hh"};
static constexpr size_t NUM_CHANNELS = sizeof(CHANNEL_NAMES) / sizeof(CHANNEL_NAMES[0]);
static constexpr size_t NUM_MAIN_CHANNELS = sizeof(MAIN_CHANNEL_NAMES) / sizeof(MAIN_CHANNEL_NAMES[0]);

// Audio Processing Constants
static constexpr double SAMPLE_RATE_STANDARD = 44100.0;
static constexpr int STANDARD_BUFFER_SIZE = 512;

// Parameter Value Constants
static constexpr float CHAOS_MIN = 0.0f;
static constexpr float CHAOS_MAX = 1.0f;
static constexpr uint8_t VELOCITY_VALUE_MIN = 1;
static constexpr uint8_t VELOCITY_VALUE_MAX = 127;

// Clock Division Constants
static constexpr int CLOCK_NUM_MIN = 1;
static constexpr int CLOCK_NUM_MAX = 32;
static constexpr int CLOCK_DEN_MIN = 1;
static constexpr int CLOCK_DEN_MAX = 32;

// MIDI Note Constants
static constexpr uint8_t MIDI_NOTE_MIN = 21;  // A0
static constexpr uint8_t MIDI_NOTE_MAX = 127; // G9
static constexpr uint8_t DEFAULT_BD_NOTE = 36; // C2 (Kick)
static constexpr uint8_t DEFAULT_SN_NOTE = 38; // D2 (Snare)
static constexpr uint8_t DEFAULT_HH_NOTE = 42; // F#2 (Hi-hat)

// MIDI Channel Constants
static constexpr uint8_t MIDI_CHANNEL_MIN = 0;   // Channel 1 (0-indexed)
static constexpr uint8_t MIDI_CHANNEL_MAX = 15;  // Channel 16 (0-indexed)
static constexpr uint8_t DEFAULT_MIDI_CHANNEL = 9; // Channel 10 (0-indexed) - GM Drums

// String Processing Constants
static constexpr const char* PARAMETER_SEPARATOR = "_";
static constexpr const char* PROBABILITY_PRE_SUFFIX = "probability_pre";   // P1: Probability before M1
static constexpr const char* PROBABILITY_POST_SUFFIX = "probability_post"; // P2: Probability before S2 (after Mix Matrix)
static constexpr const char* MUTE_PRE_SUFFIX = "mute_pre";   // Mute before priority (M1)
static constexpr const char* MUTE_POST_SUFFIX = "mute_post"; // Mute after priority (M2)
static constexpr const char* SOLO_PRE_SUFFIX = "solo_pre";   // Solo before priority (S1)
static constexpr const char* SOLO_POST_SUFFIX = "solo_post"; // Solo after priority (S2)
static constexpr const char* CHAOS_SUFFIX = "chaos";
static constexpr const char* VEL_VALUE_SUFFIX = "vel_value";
static constexpr const char* VEL_RANDOMIZE_SUFFIX = "vel_randomize";
static constexpr const char* CLOCK_NUM_SUFFIX = "clock_num";
static constexpr const char* CLOCK_DEN_SUFFIX = "clock_den";
static constexpr const char* CLOCK_RATIO_SUFFIX = "clock_ratio";
static constexpr const char* SHIFT_SUFFIX = "shift";
static constexpr const char* HUMANIZE_SUFFIX = "humanize";
static constexpr const char* SWING_SUFFIX = "swing";

// Priority Matrix Constants (formerly "Linear Drumming")
// Note: LINEAR_DRUMMING_ENABLE was removed in v0.3.400 - Priority Matrix now always active
static constexpr const char* LINEAR_DRUMMING_GRID_SUFFIX = "linear_grid";
static constexpr const char* LINEAR_DRUMMING_PRIORITY_SUFFIX = "linear_priority";

// Euclidean Sequencer Constants
static constexpr const char* ENGINE_PROBABILITY_SUFFIX = "engine_probability";  // 0.0 = 100% Grids, 1.0 = 100% Euclidean
static constexpr const char* EUCLIDEAN_STEPS_SUFFIX = "euclidean_steps";    // Pattern length (2-32)
static constexpr const char* EUCLIDEAN_PULSES_SUFFIX = "euclidean_pulses";  // Number of hits (0-steps)
static constexpr const char* EUCLIDEAN_START_SUFFIX = "euclidean_start";    // Start step (1-based, 1 to steps)
static constexpr uint8_t EUCLIDEAN_MIN_STEPS = 2;
static constexpr uint8_t EUCLIDEAN_MAX_STEPS = 32;
static constexpr uint8_t EUCLIDEAN_DEFAULT_STEPS = 16;
static constexpr uint8_t EUCLIDEAN_DEFAULT_START = 1;