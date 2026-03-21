#pragma once
#include <vector>
#include <cstdint>

// ── ColorSynth ────────────────────────────────────────────────────────────────
// MIDI-reactive color synthesizer.
//
// Each frame, tick() updates oscillators and reacts to incoming MIDI note-on
// velocities.  The computed HSL values are uploaded as shader uniforms by the
// Renderer, replacing (or blending with) the built-in cosine palette.
//
// Color model
//   • Two HSL slots: primary (hsl) and alternate (hslAlt).
//   • A sine oscillator smoothly alternates between them at altRate Hz.
//   • A second sine oscillator shifts hue at hueOscRate Hz.
//   • A luminance oscillator pulses brightness at lumOscRate Hz.
//   • Incoming MIDI note-on velocity adds an impulse to hue/sat/lum that
//     decays exponentially — the harder the note, the bigger the colour flash.
//
// Shader blend modes (u_cs_mode)
//   0  Replace  — synth colour completely replaces the built-in palette.
//   1  Multiply — synth colour * palette (darkens / tints).
//   2  Screen   — synth colour screen-blended over palette (lightens).
// ─────────────────────────────────────────────────────────────────────────────

struct ColorSynth {
    bool  enabled       = false;

    // ── Primary HSL ──────────────────────────────────────────────────────────
    float hueBase       = 0.0f;   // 0–1 wrapping hue
    float satBase       = 0.8f;
    float lumBase       = 0.5f;

    // ── Alternate HSL (alternates with primary at altRate Hz) ─────────────────
    float hueAlt        = 0.55f;
    float satAlt        = 0.9f;
    float lumAlt        = 0.45f;
    float altRate       = 0.5f;   // Hz — oscillation frequency between colours

    // ── Hue oscillator ────────────────────────────────────────────────────────
    float hueOscAmp     = 0.08f;  // max hue shift (0 = no oscillation)
    float hueOscRate    = 0.25f;  // Hz

    // ── Luminance oscillator ──────────────────────────────────────────────────
    float lumOscAmp     = 0.12f;
    float lumOscRate    = 0.6f;   // Hz

    // ── Escape-value spread (how far hue/lum vary across the fractal detail) ──
    float hueSpread     = 0.35f;  // fraction of hue wheel per escape-value range
    float lumSpread     = 0.4f;   // lum variation across escape range

    // ── MIDI note-on reaction ─────────────────────────────────────────────────
    float midiHueSens   = 0.25f;  // hue impulse per unit velocity (0–1)
    float midiSatSens   = 0.15f;
    float midiLumSens   = 0.40f;
    float midiDecay     = 1.8f;   // impulse half-life in seconds

    // ── Shader blend mode ─────────────────────────────────────────────────────
    int   blendMode     = 1;      // 0=replace  1=multiply  2=screen

    // ── Computed outputs — written by tick(), read by Renderer ────────────────
    float outHSL[3]     = {0.0f, 0.8f, 0.5f};
    float outHSLAlt[3]  = {0.55f, 0.9f, 0.45f};
    float outAltBlend   = 0.0f;   // 0=primary  1=alt

    // Three-byte MIDI message (mirrors MidiInput::Message to avoid circular dep)
    struct Msg { uint8_t status, data1, data2; };

    void tick(float time, float dt, const std::vector<Msg>& msgs);

private:
    float m_hueImpulse  = 0.0f;
    float m_satImpulse  = 0.0f;
    float m_lumImpulse  = 0.0f;
};
