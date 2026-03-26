#pragma once

struct ClockInfo {
    double ppqPosition = 0.0;
    double bpm = 120.0;
    bool isPlaying = false;
    int sampleOffset = 0;
};