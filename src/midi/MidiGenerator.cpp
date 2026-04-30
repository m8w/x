#include "MidiGenerator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// ── Scale interval tables (pitch-class offsets within octave) ─────────────────
static const int kScalePC[][12] = {
    {0,1,2,3,4,5,6,7,8,9,10,11},  // Chromatic   12
    {0,2,4,5,7,9,11,-1},           // Major        7
    {0,2,3,5,7,8,10,-1},           // Minor        7
    {0,2,4,7,9,-1},                // Penta Major  5
    {0,3,5,7,10,-1},               // Penta Minor  5
    {0,2,3,5,7,9,10,-1},           // Dorian       7
    {0,1,3,5,7,8,10,-1},           // Phrygian     7
    {0,2,4,6,7,9,11,-1},           // Lydian       7
    {0,2,4,5,7,9,10,-1},           // Mixolydian   7
    {0,2,4,6,8,10,-1},             // Whole Tone   6
    {0,2,3,5,6,8,9,11},            // Diminished   8
    {0,3,5,6,7,10,-1},             // Blues        6
};

// 5-limit just intonation offsets from 12-TET per pitch class (cents)
static const float kJIOffsets[12] = {
     0.00f,   // 0 unison
    11.73f,   // 1 m2  (16/15)
     3.91f,   // 2 M2  (9/8)
   -15.64f,   // 3 m3  (6/5)
   -13.69f,   // 4 M3  (5/4)
    -1.96f,   // 5 P4  (4/3)
     9.78f,   // 6 A4  (45/32)
     1.96f,   // 7 P5  (3/2)
    13.69f,   // 8 m6  (8/5)
    15.64f,   // 9 M6  (5/3)
    -3.91f,   // 10 m7 (9/5)
   -11.73f,   // 11 M7 (15/8)
};

// Harmonic series pull offsets per pitch class (cents)
// Emphasises H7 (−31c blue seventh) and H11 (−49c tritone)
static const float kHarmonicOffsets[12] = {
     0.00f,   // 0  root — on series
     0.00f,   // 1  no close partial
     3.91f,   // 2  H9  (major 2nd, sharp by ~4c)
   -15.64f,   // 3  H19 ~ minor 3rd (5-limit)
   -13.69f,   // 4  H5  (major 3rd, flat by ~14c)
     1.96f,   // 5  H3  (perfect 4th vicinity, +2c)
   -49.36f,   // 6  H11 (sharp 11th — alien tritone, half a semitone flat)
     1.96f,   // 7  H3  (perfect 5th, sharp by 2c)
    13.69f,   // 8  H5/H10 territory
   -31.17f,   // 9  H7  (the "blue" seventh displaced to M6 — very exotic)
    -3.91f,   // 10 H7/H14 neighbourhood
   -11.73f,   // 11 H15 (major 7th, slightly flat)
};

const char* genScaleName(GenScale s) {
    switch(s) {
        case GenScale::Chromatic:   return "Chromatic";
        case GenScale::Major:       return "Major";
        case GenScale::Minor:       return "Natural Minor";
        case GenScale::PentaMaj:    return "Pentatonic Maj";
        case GenScale::PentaMin:    return "Pentatonic Min";
        case GenScale::Dorian:      return "Dorian";
        case GenScale::Phrygian:    return "Phrygian";
        case GenScale::Lydian:      return "Lydian";
        case GenScale::Mixolydian:  return "Mixolydian";
        case GenScale::WholeTone:   return "Whole Tone";
        case GenScale::Diminished:  return "Diminished";
        case GenScale::Blues:       return "Blues";
        default:                    return "?";
    }
}

const char* genRootName(int r) {
    static const char* names[] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
    };
    return names[r & 0x0F];
}

const char* microtonalModeName(MicrotonalMode m) {
    switch (m) {
        case MicrotonalMode::Off:         return "Off (12-TET)";
        case MicrotonalMode::RandomDrift: return "Random Drift";
        case MicrotonalMode::QuarterTone: return "Quarter-Tone (24-EDO)";
        case MicrotonalMode::JustInton:   return "Just Intonation (5-limit)";
        case MicrotonalMode::Harmonic:    return "Harmonic Series";
        default:                          return "?";
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
std::vector<int> MidiGenerator::buildNoteList() const {
    const int* row = kScalePC[(int)scale];
    std::vector<int> notes;
    notes.reserve(64);
    for (int n = noteMin; n <= noteMax; n++) {
        int pc = ((n - rootKey) % 12 + 12) % 12;
        if ((int)scale == 0) {
            notes.push_back(n);  // chromatic: all notes
            continue;
        }
        for (int i = 0; i < 12 && row[i] >= 0; i++) {
            if (pc == row[i]) { notes.push_back(n); break; }
        }
    }
    if (notes.empty()) notes.push_back(60);
    return notes;
}

float MidiGenerator::stepSec() {
    // Chaos timing: when timingChaos > 0, interval is randomly drawn from a
    // wide range so there is no discernible rhythmic pattern at all.
    if (timingChaos > 0.001f) {
        float minT = 0.02f;                           // 20 ms floor
        float maxT = 0.02f + 4.0f * timingChaos;     // up to 4 s at full chaos
        // Use exponential-style distribution: bias toward shorter intervals
        // but with a long tail — similar to a Poisson process.
        float u = std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng);
        float mean = minT + (maxT - minT) * 0.25f;   // mean ≈ 1/4 of range
        float t = -mean * std::log(std::max(1e-6f, u));
        return std::clamp(t, minT, maxT);
    }
    // Normal mode: fixed beat grid with humanize scatter
    float beats = (stepRateIdx < 6) ? kGenBeats[stepRateIdx]
                : kGenBeats[std::uniform_int_distribution<int>(0,5)(m_rng)];
    return beats * 60.0f / bpm;
}

float MidiGenerator::noteSec() const {
    float beats = (noteLenIdx < 6) ? kGenBeats[noteLenIdx]
                : kGenBeats[std::uniform_int_distribution<int>(0,5)(
                      const_cast<std::mt19937&>(m_rng))];
    return beats * 60.0f / bpm;
}

float MidiGenerator::jitteredSec(float base) const {
    if (!humanize) return base;
    std::uniform_real_distribution<float> d(base * 0.93f, base * 1.07f);
    return d(const_cast<std::mt19937&>(m_rng));
}

int MidiGenerator::pickNote() {
    auto notes = buildNoteList();
    std::uniform_int_distribution<int> d(0, (int)notes.size()-1);
    return notes[d(m_rng)];
}

int MidiGenerator::pickVel() {
    std::uniform_int_distribution<int> d(velMin, velMax);
    return d(m_rng);
}

void MidiGenerator::scheduleNextStep(double now) {
    float s = stepSec();
    m_nextStep = now + (timingChaos > 0.001f ? s : jitteredSec(s));
}

// ── Microtonal pitch computation ──────────────────────────────────────────────
float MidiGenerator::microtonalCents(int note) {
    if (microtonalMode == MicrotonalMode::Off) return 0.0f;

    int pc = ((note - rootKey) % 12 + 12) % 12;
    float scale_f = microtonalAmt / 50.0f;  // 50c → 1.0 normalised
    float cents = 0.0f;

    switch (microtonalMode) {
    case MicrotonalMode::RandomDrift:
        cents = std::uniform_real_distribution<float>(-microtonalAmt, microtonalAmt)(m_rng);
        break;

    case MicrotonalMode::QuarterTone: {
        // 24-EDO: randomly choose 0, +50, or −50 cent offset.
        // When amt < 50 the deviation is scaled down proportionally.
        static const int steps[3] = {0, 1, -1};
        int idx = std::uniform_int_distribution<int>(0, 2)(m_rng);
        cents = steps[idx] * 50.0f * (microtonalAmt / 50.0f);
        break;
    }

    case MicrotonalMode::JustInton:
        cents = kJIOffsets[pc] * scale_f;
        break;

    case MicrotonalMode::Harmonic:
        cents = kHarmonicOffsets[pc] * scale_f;
        break;

    default: break;
    }

    return std::clamp(cents, -200.0f, 200.0f);
}

// ── Pitch bend emission ───────────────────────────────────────────────────────
// Pitch bend range is set to ±2 semitones (200 cents) by the RPN init.
// 14-bit value: center=8192, 1 cent = 8192/200 = 40.96 units.
void MidiGenerator::emitPitchBend(float cents, uint8_t ch0,
                                   std::vector<MidiInput::Message>& out) {
    int bend = 8192 + (int)std::round(cents * (8192.0f / 200.0f));
    bend = std::clamp(bend, 0, 16383);
    uint8_t lsb = (uint8_t)(bend & 0x7F);
    uint8_t msb = (uint8_t)((bend >> 7) & 0x7F);
    out.push_back({(uint8_t)(0xE0 | ch0), lsb, msb});
}

// ── Transport ─────────────────────────────────────────────────────────────────
void MidiGenerator::start(double time) {
    if (!m_seeded) { m_rng.seed(std::random_device{}()); m_seeded = true; }
    m_pending.clear();
    m_stepsSincePg = 0;
    liveStep       = 0;
    liveBendCents  = 0.0f;
    m_nextStep     = time + 0.05;
    playing        = true;

    // Queue RPN 0 to set pitch bend range to ±2 semitones.
    // This arrives on the first tick() so the synth is ready before notes fire.
    uint8_t ch0 = (uint8_t)(channel - 1);
    m_initQueue.clear();
    m_initQueue.push_back({(uint8_t)(0xB0 | ch0), 101,   0});  // RPN MSB=0
    m_initQueue.push_back({(uint8_t)(0xB0 | ch0), 100,   0});  // RPN LSB=0 (pitch bend range)
    m_initQueue.push_back({(uint8_t)(0xB0 | ch0),   6,   2});  // Data entry: 2 semitones
    m_initQueue.push_back({(uint8_t)(0xB0 | ch0),  38,   0});  // Data entry LSB: 0
    m_initQueue.push_back({(uint8_t)(0xB0 | ch0), 101, 127});  // Null RPN
    m_initQueue.push_back({(uint8_t)(0xB0 | ch0), 100, 127});
    // Reset pitch bend to center
    m_initQueue.push_back({(uint8_t)(0xE0 | ch0), 0, 64});     // bend center
}

void MidiGenerator::stop(std::vector<MidiInput::Message>& out) {
    playing = false;
    uint8_t ch0 = (uint8_t)(channel - 1);
    for (auto& p : m_pending) {
        out.push_back({(uint8_t)(0x80 | ch0), p.note, 0});
    }
    m_pending.clear();
    m_initQueue.clear();
    // Reset pitch bend to center on stop
    out.push_back({(uint8_t)(0xE0 | ch0), 0, 64});
}

std::vector<MidiInput::Message> MidiGenerator::fireOneNote() {
    if (!m_seeded) { m_rng.seed(std::random_device{}()); m_seeded = true; }
    std::vector<MidiInput::Message> out;
    uint8_t ch0 = (uint8_t)(channel - 1);
    int n = pickNote(), v = pickVel();
    if (microtonalMode != MicrotonalMode::Off) {
        float c = microtonalCents(n);
        emitPitchBend(c, ch0, out);
        liveBendCents = c;
    }
    out.push_back({(uint8_t)(0x90 | ch0), (uint8_t)n, (uint8_t)v});
    liveNote = n; liveVel = v;
    return out;
}

// ── Tick ─────────────────────────────────────────────────────────────────────
std::vector<MidiInput::Message> MidiGenerator::tick(double time) {
    if (!m_seeded) { m_rng.seed(std::random_device{}()); m_seeded = true; }
    std::vector<MidiInput::Message> out;
    if (!enabled || !playing) return out;

    // Flush RPN init queue (sent once after start())
    if (!m_initQueue.empty()) {
        out.insert(out.end(), m_initQueue.begin(), m_initQueue.end());
        m_initQueue.clear();
    }

    uint8_t ch0 = (uint8_t)(channel - 1);

    // ── Fire pending NoteOffs ───────────────────────────────────────────────
    for (auto it = m_pending.begin(); it != m_pending.end(); ) {
        if (time >= it->offTime) {
            out.push_back({(uint8_t)(0x80 | it->ch), it->note, 0});
            it = m_pending.erase(it);
        } else { ++it; }
    }

    // ── Advance step ────────────────────────────────────────────────────────
    if (time < m_nextStep) return out;

    scheduleNextStep(time);
    liveStep++;
    m_stepsSincePg++;

    // Rest?
    std::uniform_real_distribution<float> restRoll(0.0f, 1.0f);
    bool isRest = noteEnabled && (restRoll(m_rng) < restProb);

    if (noteEnabled && !isRest) {
        int count = std::max(1, chordSize);
        int v     = pickVel();
        // For chaos timing, note length is also random
        float dur = (timingChaos > 0.001f)
                  ? std::uniform_real_distribution<float>(0.02f, 0.5f + timingChaos)(m_rng)
                  : noteSec();

        // One pitch bend per step (affects all notes in the chord equally)
        if (microtonalMode != MicrotonalMode::Off) {
            // Use first note's pitch class for the bend calculation
            int firstNote = pickNote();
            float c = microtonalCents(firstNote);
            emitPitchBend(c, ch0, out);
            liveBendCents = c;

            // First note already picked above
            out.push_back({(uint8_t)(0x90 | ch0), (uint8_t)firstNote, (uint8_t)v});
            m_pending.push_back({time + dur, ch0, (uint8_t)firstNote});
            liveNote = firstNote; liveVel = v;

            for (int k = 1; k < count; k++) {
                int n = pickNote();
                out.push_back({(uint8_t)(0x90 | ch0), (uint8_t)n, (uint8_t)v});
                m_pending.push_back({time + dur, ch0, (uint8_t)n});
            }
        } else {
            liveBendCents = 0.0f;
            for (int k = 0; k < count; k++) {
                int n = pickNote();
                out.push_back({(uint8_t)(0x90 | ch0), (uint8_t)n, (uint8_t)v});
                m_pending.push_back({time + dur, ch0, (uint8_t)n});
                if (k == 0) { liveNote = n; liveVel = v; }
            }
        }
    }

    // ── Auto program change ─────────────────────────────────────────────────
    if (pgEnabled && m_stepsSincePg >= pgEvery) {
        std::uniform_int_distribution<int> pgRoll(pgMin, pgMax);
        int pg = pgRoll(m_rng);

        if (pg >= 128) {
            int bank  = pg / 128;
            int patch = pg % 128;
            out.push_back({(uint8_t)(0xB0 | ch0), 0,  (uint8_t)bank});
            out.push_back({(uint8_t)(0xB0 | ch0), 32, 0});
            out.push_back({(uint8_t)(0xC0 | ch0), (uint8_t)patch, 0});
        } else {
            out.push_back({(uint8_t)(0xC0 | ch0), (uint8_t)pg, 0});
        }
        liveProg = pg;
        m_stepsSincePg = 0;
    }

    return out;
}
