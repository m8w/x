// BeatDetector.cpp — ported from MilkDropMac/Engine/BeatDetector.swift

#include "BeatDetector.h"
#include <algorithm>
#include <numeric>
#include <cmath>

BeatDetector::BeatDetector() {
    m_lastHardcut = Clock::now() - std::chrono::seconds(100);
    m_bassHistory.fill(0.f);
    m_trebleHistory.fill(0.f);
    m_beatTimes.fill(Clock::now());
}

void BeatDetector::process(const AudioData& data) {
    // Reset one-shot flags from previous call
    hardcutFired  = false;
    softbeatFired = false;

    // Push band values into history ring buffers
    m_bassHistory  [m_histWrite % kBassHistLen] = data.bass;
    m_trebleHistory[m_histWrite % kBassHistLen] = data.treble;
    ++m_histWrite;

    // Beat strength = bass / adaptive mean
    float adaptive = adaptiveThreshold(m_bassHistory);
    float strength = (adaptive > 0.f) ? (data.bass / adaptive) : 0.f;
    beatStrength = std::min(strength, 1.f);

    detectBeat(data.bass, strength);
    checkHardcut(data);
}

// ---------------------------------------------------------------------------

void BeatDetector::checkHardcut(const AudioData& data) {
    auto now = Clock::now();
    double elapsed = std::chrono::duration<double>(now - m_lastHardcut).count();
    if (elapsed < hardcutMinDelay) return;

    float bassNorm   = data.bassAttn;
    float trebleNorm = std::min(data.treble * 3.f, 1.f);

    bool triggered = false;
    switch (hardcutMode) {
        case HardcutMode::Bass:
            triggered = bassNorm > hardcutLowThreshold;
            break;
        case HardcutMode::Treble:
            triggered = trebleNorm > hardcutHighThreshold;
            break;
        case HardcutMode::BassAndTreble:
            triggered = bassNorm > hardcutLowThreshold && trebleNorm > hardcutHighThreshold;
            break;
        case HardcutMode::BassOrTreble:
            triggered = bassNorm > hardcutLowThreshold || trebleNorm > hardcutHighThreshold;
            break;
    }

    if (triggered) {
        m_lastHardcut = now;
        hardcutFired  = true;
    }
}

void BeatDetector::detectBeat(float bass, float strength) {
    // Onset: strength > 1.2× and bass rising faster than 30%
    if (strength <= 1.2f || bass <= m_prevBass * 1.3f) {
        m_prevBass = bass;
        return;
    }
    m_prevBass = bass;
    softbeatFired = true;

    auto now = Clock::now();
    m_beatTimes[m_beatWrite % kBeatHistLen] = now;
    ++m_beatWrite;
    if (m_beatCount < kBeatHistLen) ++m_beatCount;

    // BPM from last N inter-beat intervals
    if (m_beatCount >= 2) {
        int count = std::min(m_beatCount, kBeatHistLen);
        double totalInterval = 0.0;
        int intervals = 0;
        for (int i = 1; i < count; ++i) {
            int a = (m_beatWrite - i - 1 + kBeatHistLen) % kBeatHistLen;
            int b = (m_beatWrite - i     + kBeatHistLen) % kBeatHistLen;
            double diff = std::chrono::duration<double>(m_beatTimes[b] - m_beatTimes[a]).count();
            if (diff > 0.0) { totalInterval += diff; ++intervals; }
        }
        if (intervals > 0)
            bpm = 60.0 / (totalInterval / (double)intervals);
    }
}

float BeatDetector::adaptiveThreshold(const std::array<float, kBassHistLen>& hist) const {
    float sum = 0.f;
    int filled = std::min(m_histWrite, kBassHistLen);
    if (filled == 0) return 1.f;
    for (int i = 0; i < filled; ++i) sum += hist[i];
    return (sum / (float)filled) * 1.5f;
}
