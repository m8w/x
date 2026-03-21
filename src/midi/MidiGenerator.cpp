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

// ── Helpers ───────────────────────────────────────────────────────────────────
std::vector<int> MidiGenerator::buildNoteList() const {
    const int* row = kScalePC[(int)scale];
    std::vector<int> notes;
    notes.reserve(64);
    for (int n = noteMin; n <= noteMax; n++) {
        int pc = ((n - rootKey) % 12 + 12) % 12;
        for (int i = 0; i < 12 && row[i] >= 0; i++) {
            if (pc == row[i]) { notes.push_back(n); break; }
        }
        if ((int)scale == 0) notes.push_back(n); // chromatic: add all
    }
    if (scale == GenScale::Chromatic) {
        // remove duplicates from the double-add above
        std::sort(notes.begin(), notes.end());
        notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
    }
    if (notes.empty()) notes.push_back(60);
    return notes;
}

float MidiGenerator::stepSec() const {
    float beats = (stepRateIdx < 6) ? kGenBeats[stepRateIdx]
                : kGenBeats[std::uniform_int_distribution<int>(0,5)(
                      const_cast<std::mt19937&>(m_rng))];
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
    m_nextStep = now + jitteredSec(stepSec());
}

// ── Transport ─────────────────────────────────────────────────────────────────
void MidiGenerator::start(double time) {
    if (!m_seeded) { m_rng.seed(std::random_device{}()); m_seeded = true; }
    m_pending.clear();
    m_stepsSincePg = 0;
    liveStep       = 0;
    m_nextStep     = time + 0.05;
    playing        = true;
}

void MidiGenerator::stop(std::vector<MidiInput::Message>& out) {
    playing = false;
    uint8_t ch0 = (uint8_t)(channel - 1);
    for (auto& p : m_pending) {
        out.push_back({(uint8_t)(0x80 | ch0), p.note, 0});
    }
    m_pending.clear();
}

std::vector<MidiInput::Message> MidiGenerator::fireOneNote() {
    if (!m_seeded) { m_rng.seed(std::random_device{}()); m_seeded = true; }
    std::vector<MidiInput::Message> out;
    uint8_t ch0 = (uint8_t)(channel - 1);
    int n = pickNote(), v = pickVel();
    out.push_back({(uint8_t)(0x90 | ch0), (uint8_t)n, (uint8_t)v});
    // NoteOff after 250 ms (no wall-clock available here; caller handles)
    liveNote = n; liveVel = v;
    return out;
}

// ── Tick ─────────────────────────────────────────────────────────────────────
std::vector<MidiInput::Message> MidiGenerator::tick(double time) {
    if (!m_seeded) { m_rng.seed(std::random_device{}()); m_seeded = true; }
    std::vector<MidiInput::Message> out;
    if (!enabled || !playing) return out;

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
        float dur = noteSec();

        for (int k = 0; k < count; k++) {
            int n = pickNote();
            out.push_back({(uint8_t)(0x90 | ch0), (uint8_t)n, (uint8_t)v});
            m_pending.push_back({time + dur, ch0, (uint8_t)n});
            if (k == 0) { liveNote = n; liveVel = v; }
        }
    }

    // ── Auto program change ─────────────────────────────────────────────────
    if (pgEnabled && m_stepsSincePg >= pgEvery) {
        std::uniform_int_distribution<int> pgRoll(pgMin, pgMax);
        int pg = pgRoll(m_rng);

        if (pg >= 128) {
            // Extended range: emit Bank Select MSB (CC0) then PC
            // Surge XT / most GM synths: bank = pg/128, patch = pg%128
            int bank  = pg / 128;
            int patch = pg % 128;
            out.push_back({(uint8_t)(0xB0 | ch0), 0, (uint8_t)bank});   // CC0
            out.push_back({(uint8_t)(0xC0 | ch0), (uint8_t)patch, 0}); // PC
        } else {
            out.push_back({(uint8_t)(0xC0 | ch0), (uint8_t)pg, 0});
        }
        liveProg = pg;
        m_stepsSincePg = 0;
    }

    return out;
}
