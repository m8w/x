#pragma once
#include "MidiInput.h"
#include <vector>
#include <random>
#include <cstdint>
#include <string>

// ── Scale modes — matches the HTML app's 12-scale set ────────────────────────
enum class GenScale {
    Chromatic=0, Major, Minor, PentaMaj, PentaMin,
    Dorian, Phrygian, Lydian, Mixolydian, WholeTone, Diminished, Blues,
    COUNT
};
const char* genScaleName(GenScale s);
const char* genRootName (int r);       // r = 0–11 (C … B)

// ── Microtonal modes ──────────────────────────────────────────────────────────
// Applied via pitch bend before each NoteOn.  Bend range is set to ±2
// semitones (200 cents) via RPN 0 on start() so all offsets fit cleanly.
enum class MicrotonalMode {
    Off = 0,      // standard 12-TET, no bend
    RandomDrift,  // random ±amt cents per note  — pure stochastic microtonality
    QuarterTone,  // 24-EDO snap: each note randomly +0, +50 or −50 cents
    JustInton,    // 5-limit just intonation offsets from 12-TET per scale degree
    Harmonic,     // pulls pitches toward the natural harmonic series of the root
                  //   — emphasises H7 (−31c "blue" seventh) and H11 (−49c tritone)
    COUNT
};
const char* microtonalModeName(MicrotonalMode m);

// Step-rate / note-length options (index into kGenBeats[])
// 0=1/32  1=1/16  2=1/8  3=1/4  4=1/2  5=Whole  6=Random
inline constexpr float kGenBeats[] = {0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
inline constexpr int   kGenBeatCount = 7;   // 6 fixed + 1 random slot

// Timer-based random MIDI generator.
// Produces NoteOn / NoteOff / Program-Change messages each frame.
// Feed the output directly into MidiMapper::apply() + MidiMapper::feedLearn().
class MidiGenerator {
public:
    // ── Master ────────────────────────────────────────────────────────────────
    bool  enabled     = false;
    bool  playing     = false;    // true while sequencer is running

    // ── Note & Scale ──────────────────────────────────────────────────────────
    bool  noteEnabled = true;
    int   noteMin     = 36;       // lowest MIDI note
    int   noteMax     = 96;       // highest MIDI note
    int   rootKey     = 0;        // 0=C  …  11=B
    GenScale scale    = GenScale::PentaMin;
    int   velMin      = 40;
    int   velMax      = 110;
    int   channel     = 1;        // MIDI channel 1-16
    int   chordSize   = 1;        // notes per step (1–6)

    // ── Microtonal ────────────────────────────────────────────────────────────
    MicrotonalMode microtonalMode = MicrotonalMode::Off;
    float microtonalAmt = 50.0f;  // max deviation in cents (0–100)

    // ── Timing chaos ─────────────────────────────────────────────────────────
    // 0 = normal humanized step rate
    // 0.1–1.0 = increasingly wild random intervals (Poisson-style)
    // At 1.0 each note fires anywhere from 20ms to 4 seconds after the last.
    float timingChaos = 0.0f;

    // ── Timing ───────────────────────────────────────────────────────────────
    float bpm         = 120.0f;
    int   stepRateIdx = 1;        // index into kGenBeats[] (1/16 default)
    int   noteLenIdx  = 2;        // index into kGenBeats[] (1/8 default)
    float restProb    = 0.20f;    // 0–0.75: probability of a rest on each step
    bool  humanize    = true;     // ±5 % timing scatter

    // ── Auto program-change ───────────────────────────────────────────────────
    bool  pgEnabled   = false;
    int   pgEvery     = 8;
    int   pgMin       = 0;
    int   pgMax       = 10;

    // ── MIDI Thru ─────────────────────────────────────────────────────────────
    bool  midiThru    = false;

    // ── Live state (read by UI each frame) ────────────────────────────────────
    int   liveNote    = -1;
    int   liveVel     = 0;
    int   liveProg    = -1;
    int   liveStep    = 0;
    float liveBendCents = 0.0f;   // last microtonal bend applied (cents)

    // ── Tick: call once per frame ─────────────────────────────────────────────
    std::vector<MidiInput::Message> tick(double time);

    // Transport helpers
    void start(double time);
    void stop (std::vector<MidiInput::Message>& out);
    std::vector<MidiInput::Message> fireOneNote();

    std::vector<int> buildNoteList() const;

private:
    struct PendingOff { double offTime; uint8_t ch, note; };

    double               m_nextStep  = 0.0;
    int                  m_stepsSincePg = 0;
    bool                 m_seeded    = false;
    std::mt19937         m_rng;
    std::vector<PendingOff> m_pending;
    std::vector<MidiInput::Message> m_initQueue;

    float stepSec();
    float noteSec()    const;
    float jitteredSec(float base) const;
    int   pickNote();
    int   pickVel();
    void  scheduleNextStep(double now);
    float microtonalCents(int note);
    void  emitPitchBend(float cents, uint8_t ch0,
                        std::vector<MidiInput::Message>& out);
};
