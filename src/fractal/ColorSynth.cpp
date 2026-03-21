#include "ColorSynth.h"
#include <cmath>
#include <algorithm>

static constexpr float kTwoPi = 6.28318530718f;

void ColorSynth::tick(float time, float dt, const std::vector<Msg>& msgs) {
    if (!enabled) return;

    // ── React to MIDI note-on ─────────────────────────────────────────────────
    for (const auto& m : msgs) {
        int type = m.status & 0xF0;
        bool isNoteOn = (type == 0x90) && (m.data2 > 0);
        if (isNoteOn) {
            float vel = m.data2 / 127.0f;           // 0–1 normalised velocity
            // Map MIDI note pitch to hue target (C0=0 .. B9≈1)
            float pitchHue = (m.data1 % 12) / 12.0f;
            m_hueImpulse += (pitchHue - hueBase) * vel * midiHueSens * 2.0f;
            m_satImpulse += vel * midiSatSens;
            m_lumImpulse += vel * midiLumSens;
        }
    }

    // ── Exponential decay of impulses ─────────────────────────────────────────
    float decay = std::exp(-dt * midiDecay * 3.0f);
    m_hueImpulse *= decay;
    m_satImpulse *= decay;
    m_lumImpulse *= decay;

    // ── Oscillators ───────────────────────────────────────────────────────────
    float hueOsc = hueOscAmp * std::sin(kTwoPi * hueOscRate * time);
    float lumOsc = lumOscAmp * std::sin(kTwoPi * lumOscRate * time);

    // ── Alternation blend (smooth sine, 0=primary 1=alt) ─────────────────────
    outAltBlend = 0.5f + 0.5f * std::sin(kTwoPi * altRate * time);

    // ── Primary output ────────────────────────────────────────────────────────
    outHSL[0] = hueBase + hueOsc + m_hueImpulse;
    // keep hue in [0,1) by wrapping
    outHSL[0] = outHSL[0] - std::floor(outHSL[0]);
    outHSL[1] = std::clamp(satBase + m_satImpulse, 0.0f, 1.0f);
    outHSL[2] = std::clamp(lumBase + lumOsc + m_lumImpulse, 0.0f, 1.0f);

    // ── Alternate output (same oscillators, different base) ───────────────────
    outHSLAlt[0] = hueAlt + hueOsc * 0.7f + m_hueImpulse * 0.5f;
    outHSLAlt[0] = outHSLAlt[0] - std::floor(outHSLAlt[0]);
    outHSLAlt[1] = std::clamp(satAlt + m_satImpulse * 0.8f, 0.0f, 1.0f);
    outHSLAlt[2] = std::clamp(lumAlt - lumOsc + m_lumImpulse * 0.6f, 0.0f, 1.0f);
}
