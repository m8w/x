#include "ColorSynth.h"
#include <cmath>
#include <algorithm>

static constexpr float kTwoPi = 6.28318530718f;

// Convert RGB [0,1] → HSL [0,1]
static void rgb2hsl(float r, float g, float b, float& h, float& s, float& l) {
    float mx = std::max({r, g, b});
    float mn = std::min({r, g, b});
    l = (mx + mn) * 0.5f;
    float d = mx - mn;
    if (d < 1e-6f) { h = s = 0.0f; return; }
    s = d / (1.0f - std::fabs(2.0f * l - 1.0f));
    if      (mx == r) h = std::fmod((g - b) / d + 6.0f, 6.0f) / 6.0f;
    else if (mx == g) h = ((b - r) / d + 2.0f) / 6.0f;
    else              h = ((r - g) / d + 4.0f) / 6.0f;
}

void ColorSynth::tick(float time, float dt, const std::vector<Msg>& msgs) {
    if (!enabled) return;

    float decay = std::exp(-dt * midiDecay * 3.0f);

    // ── React to glitch (rising edge) ────────────────────────────────────────
    if (glitchColorReact && inGlitch && !m_wasInGlitch) {
        if (synthMode == 0) {
            m_hueImpulse += glitchHueSens * (std::fmod(m_hueImpulse + 0.5f, 1.0f) - 0.5f + 0.3f);
            m_satImpulse += glitchSatSens;
            m_lumImpulse += glitchLumSens;
        } else {
            m_rImpulse += glitchLumSens * 0.8f;
            m_gImpulse += glitchLumSens * 0.5f;
            m_bImpulse += glitchLumSens * 1.0f;
        }
    }
    m_wasInGlitch = inGlitch;

    // ── React to MIDI note-on ─────────────────────────────────────────────────
    for (const auto& m : msgs) {
        bool isNoteOn = ((m.status & 0xF0) == 0x90) && (m.data2 > 0);
        if (isNoteOn) {
            float vel = m.data2 / 127.0f;
            if (synthMode == 0) {
                float pitchHue = (m.data1 % 12) / 12.0f;
                m_hueImpulse += (pitchHue - hueBase) * vel * midiHueSens * 2.0f;
                m_satImpulse += vel * midiSatSens;
                m_lumImpulse += vel * midiLumSens;
            } else {
                m_rImpulse += vel * midiRSens;
                m_gImpulse += vel * midiGSens;
                m_bImpulse += vel * midiBSens;
            }
        }
    }

    // ── Exponential decay ─────────────────────────────────────────────────────
    m_hueImpulse *= decay;  m_satImpulse *= decay;  m_lumImpulse *= decay;
    m_rImpulse   *= decay;  m_gImpulse   *= decay;  m_bImpulse   *= decay;

    // ── Alternation blend ─────────────────────────────────────────────────────
    float altBlend = 0.5f + 0.5f * std::sin(kTwoPi * altRate * time);

    if (synthMode == 0) {
        // ── HSL path ──────────────────────────────────────────────────────────
        float hueOsc = hueOscAmp * std::sin(kTwoPi * hueOscRate * time);
        float lumOsc = lumOscAmp * std::sin(kTwoPi * lumOscRate * time);

        outAltBlend  = altBlend;

        outHSL[0] = hueBase + hueOsc + m_hueImpulse;
        outHSL[0] = outHSL[0] - std::floor(outHSL[0]);
        outHSL[1] = std::clamp(satBase + m_satImpulse, 0.0f, 1.0f);
        outHSL[2] = std::clamp(lumBase + lumOsc + m_lumImpulse, 0.0f, 1.0f);

        outHSLAlt[0] = hueAlt + hueOsc * 0.7f + m_hueImpulse * 0.5f;
        outHSLAlt[0] = outHSLAlt[0] - std::floor(outHSLAlt[0]);
        outHSLAlt[1] = std::clamp(satAlt + m_satImpulse * 0.8f, 0.0f, 1.0f);
        outHSLAlt[2] = std::clamp(lumAlt - lumOsc + m_lumImpulse * 0.6f, 0.0f, 1.0f);
    } else {
        // ── RGB path: blend primary + alt, add per-channel oscillators ─────────
        float rOsc = rOscAmp * std::sin(kTwoPi * rOscRate * time);
        float gOsc = gOscAmp * std::sin(kTwoPi * gOscRate * time);
        float bOsc = bOscAmp * std::sin(kTwoPi * bOscRate * time);

        float r = std::clamp(rBase * (1.0f - altBlend) + rAlt * altBlend + rOsc + m_rImpulse, 0.0f, 1.0f);
        float g = std::clamp(gBase * (1.0f - altBlend) + gAlt * altBlend + gOsc + m_gImpulse, 0.0f, 1.0f);
        float b = std::clamp(bBase * (1.0f - altBlend) + bAlt * altBlend + bOsc + m_bImpulse, 0.0f, 1.0f);

        // Convert blended RGB to HSL for the shader (which expects HSL)
        rgb2hsl(r, g, b, outHSL[0], outHSL[1], outHSL[2]);
        // Second slot = slightly phase-shifted alt so the shader still gets variation
        float r2 = std::clamp(rAlt + rOsc * 0.6f, 0.0f, 1.0f);
        float g2 = std::clamp(gAlt + gOsc * 0.6f, 0.0f, 1.0f);
        float b2 = std::clamp(bAlt + bOsc * 0.6f, 0.0f, 1.0f);
        rgb2hsl(r2, g2, b2, outHSLAlt[0], outHSLAlt[1], outHSLAlt[2]);
        outAltBlend = altBlend;
    }
}
