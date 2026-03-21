#include "GlitchEngine.h"
#include <cmath>
#include <algorithm>

// ── Internal helpers ──────────────────────────────────────────────────────────

void GlitchEngine::seed() {
    if (!m_seeded) {
        m_rng.seed(std::random_device{}());
        m_seeded = true;
    }
}

float GlitchEngine::randF(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(m_rng);
}

int GlitchEngine::randI(int lo, int hi) {
    if (lo >= hi) return lo;
    return std::uniform_int_distribution<int>(lo, hi)(m_rng);
}

void GlitchEngine::scheduleNext(double now) {
    // Poisson-style: exponential inter-arrival with mean = 1/rate
    float mean = (glitchRateHz > 0.001f) ? 1.0f / glitchRateHz : 60.0f;
    float wait = -mean * std::log(std::max(1e-6f, randF(0.0f, 1.0f)));
    // Clamp to [0.05, 10] seconds
    wait = std::max(0.05f, std::min(wait, 10.0f));
    m_nextGlitch = now + wait;
}

std::vector<GlitchType> GlitchEngine::activeTypes() const {
    std::vector<GlitchType> v;
    if (doJuliaJump)     v.push_back(GlitchType::JuliaJump);
    if (doFormulaFlash)  v.push_back(GlitchType::FormulaFlash);
    if (doZoomPunch)     v.push_back(GlitchType::ZoomPunch);
    if (doBlendScatter)  v.push_back(GlitchType::BlendScatter);
    if (doPowerSpike)    v.push_back(GlitchType::PowerSpike);
    if (doOffsetShift)   v.push_back(GlitchType::OffsetShift);
    if (doVelocitySpike) v.push_back(GlitchType::VelocitySpike);
    if (doPitchScramble) v.push_back(GlitchType::PitchScramble);
    if (doGhostNote)     v.push_back(GlitchType::GhostNote);
    return v;
}

// ── Fire a glitch event ───────────────────────────────────────────────────────

void GlitchEngine::fireGlitch(double time, FractalEngine& eng, BlendController& blend) {
    auto types = activeTypes();
    if (types.empty()) return;

    // Pick a random type
    int idx = randI(0, (int)types.size() - 1);
    m_activeType = types[idx];

    float dur = randF(glitchDurMin, glitchDurMax);
    m_glitchEnd = time + dur;
    inGlitch = true;

    // Save current state
    m_saved.juliaC       = eng.juliaC;
    m_saved.power        = eng.power;
    m_saved.zoom         = eng.zoom;
    m_saved.offset       = eng.offset;
    m_saved.formula      = eng.formula;
    m_saved.formulaB     = eng.formulaB;
    m_saved.formulaBlend = eng.formulaBlend;
    m_saved.mandelbrot   = blend.mandelbrot;
    m_saved.julia        = blend.julia;
    m_saved.mandelbulb   = blend.mandelbulb;
    m_saved.euclidean    = blend.euclidean;
    m_saved.diff         = blend.diff;

    // Reset MIDI glitch flags
    m_velSpike   = false;
    m_pitchShift = false;

    float s = intensity;  // shorthand

    switch (m_activeType) {

    case GlitchType::JuliaJump:
        lastGlitchName = "Julia Jump";
        eng.juliaC.x  = randF(-1.5f * s, 1.5f * s);
        eng.juliaC.y  = randF(-1.5f * s, 1.5f * s);
        m_post.juliaC = eng.juliaC;
        break;

    case GlitchType::FormulaFlash:
        lastGlitchName = "Formula Flash";
        eng.formula     = randI(0, 21);
        eng.formulaB    = randI(0, 21);
        m_post.formula  = eng.formula;
        m_post.formulaB = eng.formulaB;
        break;

    case GlitchType::ZoomPunch: {
        lastGlitchName = "Zoom Punch";
        float zoomDir = (randI(0, 1) == 0) ? (1.0f + 2.0f * s) : (1.0f / (1.0f + 2.0f * s));
        eng.zoom *= zoomDir;
        eng.zoom    = std::max(0.05f, std::min(eng.zoom, 200.0f));
        m_post.zoom = eng.zoom;
        break;
    }

    case GlitchType::BlendScatter:
        lastGlitchName = "Blend Scatter";
        blend.mandelbrot      = randF(0.0f, s);
        blend.julia           = randF(0.0f, s);
        blend.mandelbulb      = randF(0.0f, s * 0.5f);
        blend.euclidean       = randF(0.0f, s * 0.7f);
        blend.diff            = randF(0.0f, s * 0.3f);
        m_post.mandelbrot     = blend.mandelbrot;
        break;

    case GlitchType::PowerSpike:
        lastGlitchName = "Power Spike";
        eng.power   = 2.0f + randF(0.0f, 14.0f * s);
        m_post.power = eng.power;
        break;

    case GlitchType::OffsetShift:
        lastGlitchName = "Offset Shift";
        eng.offset.x  += randF(-0.3f * s, 0.3f * s);
        eng.offset.y  += randF(-0.3f * s, 0.3f * s);
        m_post.offset  = eng.offset;
        break;

    case GlitchType::VelocitySpike:
        lastGlitchName = "Vel Spike";
        m_velSpike = true;
        break;

    case GlitchType::PitchScramble:
        lastGlitchName = "Pitch Scramble";
        m_pitchShift = true;
        // ±1 or ±2 octaves
        m_pitchOct = (randI(0, 1) == 0 ? 12 : -12) * randI(1, 2);
        break;

    case GlitchType::GhostNote:
        // Handled in tick() — fires a note immediately
        lastGlitchName = "Ghost Note";
        break;

    default:
        break;
    }
}

// ── Recover from a glitch ─────────────────────────────────────────────────────

void GlitchEngine::recoverGlitch(FractalEngine& eng, BlendController& blend) {
    // Guard every restore: if the current value no longer matches what the
    // glitch set it to, the user changed the param manually during the glitch
    // window and we respect that — leave the live value as-is.
    switch (m_activeType) {
    case GlitchType::JuliaJump:
        if (eng.juliaC == m_post.juliaC) eng.juliaC = m_saved.juliaC;
        break;
    case GlitchType::FormulaFlash:
        if (eng.formula  == m_post.formula)  eng.formula  = m_saved.formula;
        if (eng.formulaB == m_post.formulaB) eng.formulaB = m_saved.formulaB;
        break;
    case GlitchType::ZoomPunch:
        if (eng.zoom == m_post.zoom) eng.zoom = m_saved.zoom;
        break;
    case GlitchType::BlendScatter:
        // Use mandelbrot as the sentinel — if the user changed any blend
        // weight during the glitch we skip the full restore to avoid
        // partial overwrites.
        if (blend.mandelbrot == m_post.mandelbrot) {
            blend.mandelbrot = m_saved.mandelbrot;
            blend.julia      = m_saved.julia;
            blend.mandelbulb = m_saved.mandelbulb;
            blend.euclidean  = m_saved.euclidean;
            blend.diff       = m_saved.diff;
        }
        break;
    case GlitchType::PowerSpike:
        if (eng.power == m_post.power) eng.power = m_saved.power;
        break;
    case GlitchType::OffsetShift:
        if (eng.offset == m_post.offset) eng.offset = m_saved.offset;
        break;
    case GlitchType::VelocitySpike:
        m_velSpike = false;
        break;
    case GlitchType::PitchScramble:
        m_pitchShift = false;
        m_pitchOct   = 0;
        break;
    default:
        break;
    }
    inGlitch    = false;
    m_activeType = GlitchType::COUNT;
}

// ── Main tick ─────────────────────────────────────────────────────────────────

std::vector<MidiInput::Message> GlitchEngine::tick(double time,
                                                     FractalEngine& eng,
                                                     BlendController& blend) {
    seed();
    std::vector<MidiInput::Message> out;
    if (!enabled) {
        if (inGlitch) recoverGlitch(eng, blend);
        return out;
    }

    // End active glitch?
    if (inGlitch && time >= m_glitchEnd) {
        recoverGlitch(eng, blend);
        scheduleNext(time);
    }

    // Fire new glitch?
    if (!inGlitch && time >= m_nextGlitch) {
        fireGlitch(time, eng, blend);

        // Ghost note fires immediately as an extra MIDI message
        if (m_activeType == GlitchType::GhostNote) {
            uint8_t ch0  = (uint8_t)(midiChannel - 1);
            int note = randI(noteMin, noteMax);
            int vel  = randI(100, 127);
            out.push_back({(uint8_t)(0x90 | ch0), (uint8_t)note, (uint8_t)vel});
            // NoteOff ~100ms later via duration — handled externally; send here too
            // (caller should schedule the NoteOff; we send a brief gate)
            inGlitch = false;  // ghost note has no "recovery" — it's just a note
            scheduleNext(time);
        }
    }

    return out;
}

// ── Per-message MIDI glitch modifier ─────────────────────────────────────────

MidiInput::Message GlitchEngine::applyMidiGlitch(const MidiInput::Message& msg) {
    if (!enabled || !inGlitch) return msg;

    uint8_t type = msg.status & 0xF0;
    MidiInput::Message out = msg;

    if (type == 0x90 && msg.data2 > 0) {
        // NoteOn
        if (m_velSpike)
            out.data2 = 127;

        if (m_pitchShift) {
            int note = (int)msg.data1 + m_pitchOct;
            note = std::max(0, std::min(127, note));
            out.data1 = (uint8_t)note;
        }
    }

    return out;
}
