#pragma once
#include "IAudioCapture.h"
#include <chrono>
#include <array>
#include <vector>

// ---------------------------------------------------------------------------
// BeatDetector — MilkDrop 3-style beat / hardcut detection
// Ported from MilkDropMac/Engine/BeatDetector.swift
// ---------------------------------------------------------------------------

class BeatDetector {
public:
    // ── Hardcut configuration ─────────────────────────────────────────────
    enum class HardcutMode {
        Bass,           // bass spike alone triggers
        Treble,         // treble spike alone triggers
        BassAndTreble,  // both must be above threshold (default)
        BassOrTreble,   // either suffices
    };

    float        hardcutLowThreshold  = 0.8f;   // bassAttn threshold
    float        hardcutHighThreshold = 0.5f;   // treble threshold
    double       hardcutMinDelay      = 3.0;    // seconds between hardcuts
    HardcutMode  hardcutMode          = HardcutMode::BassAndTreble;

    // ── Outputs (updated by process()) ───────────────────────────────────
    bool   hardcutFired   = false;  // true for exactly one process() call after a hardcut
    bool   softbeatFired  = false;  // true for one call after a beat onset
    float  beatStrength   = 0.f;   // normalised 0–1 beat energy
    double bpm            = 0.0;   // estimated BPM from recent beats

    BeatDetector();

    // Call once per render frame with the latest AudioData.
    void process(const AudioData& data);

private:
    // Ring buffers (fixed-size, index-modulo)
    static constexpr int kBassHistLen   = 60;
    static constexpr int kBeatHistLen   = 8;

    std::array<float, kBassHistLen>   m_bassHistory{};
    std::array<float, kBassHistLen>   m_trebleHistory{};
    int m_histWrite = 0;

    // BPM tracking
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    std::array<TimePoint, kBeatHistLen> m_beatTimes{};
    int   m_beatWrite  = 0;
    int   m_beatCount  = 0;
    float m_prevBass   = 0.f;

    TimePoint m_lastHardcut;

    // Helpers
    float adaptiveThreshold(const std::array<float, kBassHistLen>& hist) const;
    void  checkHardcut(const AudioData& data);
    void  detectBeat(float bass, float strength);
};
