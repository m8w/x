#pragma once
#include "FractalEngine.h"
#include "BlendController.h"
#include "midi/MidiInput.h"
#include <vector>
#include <random>
#include <cstdint>

// ── Glitch event types ────────────────────────────────────────────────────────
enum class GlitchType {
    // Fractal visual glitches
    JuliaJump,      // julia C teleports to random position
    FormulaFlash,   // formula A switches for a moment
    ZoomPunch,      // zoom punches in or out
    BlendScatter,   // blend weights randomize
    PowerSpike,     // power jumps to random value
    OffsetShift,    // pan offset shifts
    // MIDI glitches (produce extra messages)
    VelocitySpike,  // next notes are max velocity
    PitchScramble,  // pitch jumps an octave
    GhostNote,      // fires an extra random note mid-step
    COUNT
};

// ── GlitchEngine ─────────────────────────────────────────────────────────────
// Fires short-duration chaos events at random intervals.
// Call tick() once per frame; it returns any extra MIDI messages to inject.
// Fractal params are modified directly (and auto-recovered after glitch ends).
class GlitchEngine {
public:
    // ── Master ────────────────────────────────────────────────────────────────
    bool  enabled       = false;

    // ── Event timing ─────────────────────────────────────────────────────────
    float glitchRateHz  = 0.5f;   // average glitch events per second
    float glitchDurMin  = 0.05f;  // minimum glitch duration (seconds)
    float glitchDurMax  = 0.30f;  // maximum glitch duration (seconds)

    // ── Intensity ─────────────────────────────────────────────────────────────
    float intensity     = 0.6f;   // 0=subtle  1=extreme

    // ── Enabled glitch types ───────────────────────────────────────────────────
    bool  doJuliaJump    = true;
    bool  doFormulaFlash = true;
    bool  doZoomPunch    = false;
    bool  doBlendScatter = true;
    bool  doPowerSpike   = false;
    bool  doOffsetShift  = false;
    bool  doVelocitySpike= true;
    bool  doPitchScramble= true;
    bool  doGhostNote    = true;

    // ── MIDI generator coupling ────────────────────────────────────────────────
    int   midiChannel   = 1;      // channel for ghost notes (1-16)
    int   noteMin       = 36;
    int   noteMax       = 96;

    // ── Live state ────────────────────────────────────────────────────────────
    bool  inGlitch      = false;
    const char* lastGlitchName = "";

    // Tick: call once per frame.
    // Returns extra MIDI messages to inject (ghost notes, velocity overrides, etc.)
    // Also modifies eng/blend directly during glitch windows.
    std::vector<MidiInput::Message> tick(double time,
                                         FractalEngine& eng,
                                         BlendController& blend);

    // Apply velocity override to outgoing MIDI (call for each generator msg)
    // Returns modified message (no-op if no active velocity glitch).
    MidiInput::Message applyMidiGlitch(const MidiInput::Message& msg);

private:
    struct SavedState {
        glm::vec2 juliaC;
        float     power;
        float     zoom;
        glm::vec2 offset;
        int       formula;
        float     mandelbrot, julia, mandelbulb, euclidean, diff;
    };

    // What the glitch SET each param to. Compared against the live value at
    // recovery time: if they still match, the user did not touch the param
    // during the glitch and we restore the pre-glitch value; if they differ,
    // the user took manual control and we leave the current value alone.
    struct PostGlitchVals {
        glm::vec2 juliaC;
        float     power;
        float     zoom;
        glm::vec2 offset;
        int       formula;
        float     mandelbrot;  // representative blend weight for BlendScatter
    };

    double      m_nextGlitch  = 0.0;
    double      m_glitchEnd   = 0.0;
    GlitchType  m_activeType  = GlitchType::COUNT;
    SavedState      m_saved;
    PostGlitchVals  m_post{};
    bool        m_seeded      = false;
    std::mt19937 m_rng;

    // Active MIDI glitch state
    bool   m_velSpike   = false;
    bool   m_pitchShift = false;
    int    m_pitchOct   = 0;   // semitone shift for pitch scramble

    void   seed();
    float  randF(float lo, float hi);
    int    randI(int lo, int hi);
    void   scheduleNext(double now);
    void   fireGlitch(double time, FractalEngine& eng, BlendController& blend);
    void   recoverGlitch(FractalEngine& eng, BlendController& blend);
    std::vector<GlitchType> activeTypes() const;
};
