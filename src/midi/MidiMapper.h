#pragma once
#include "MidiInput.h"
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include "fractal/ColorSynth.h"
#include <vector>
#include <string>

// Every fractal/blend parameter that can be MIDI-controlled.
enum class MidiParam {
    BlendMandelbrot, BlendJulia, BlendMandelbulb, BlendEuclidean, BlendDiff,
    JuliaCX, JuliaCY,
    Power, Zoom, OffsetX, OffsetY,
    FormulaA, FormulaB, FormulaBlend,
    PixelWeight, LayerCount, LayerOffset,
    GeoWarp, GeoRadius, GeoRotation,
    // ── Mirror / Kaleidoscope ─────────────────────────────────────────────────
    GeoMirror,      // 0=none 1=X 2=Y 3=XY
    GeoKaleid,      // 0=off  2-16=segment count
    // ── Color Synthesizer ─────────────────────────────────────────────────────
    ColorHue,       // primary hue base
    ColorSat,       // primary saturation
    ColorLum,       // primary luminance
    ColorAltHue,    // alternate hue base
    ColorAltRate,   // alternation oscillator frequency (Hz)
    ColorHueOscRate,// hue oscillator frequency (Hz)
    ColorHueOscAmp, // hue oscillator amplitude
    ColorLumOscAmp, // luminance oscillator amplitude
    COUNT
};

// Human-readable name for each param (parallel to MidiParam enum).
const char* midiParamName(MidiParam p);

struct MidiMapping {
    // ── MIDI trigger ─────────────────────────────────────────────────────────
    int msgType;    // 0=CC  1=NoteOn  2=NoteOff/toggle  3=ProgramChange
    int channel;    // 0=any  1-16=specific
    int number;     // CC number / note number / program number (ignored for PC if -1)

    // ── Target parameter ──────────────────────────────────────────────────────
    MidiParam param;
    float     minVal;
    float     maxVal;

    // ── Optional display name (filled by MIDI-Learn) ──────────────────────────
    char label[32];
};

// ── MIDI-Learn state ─────────────────────────────────────────────────────────
struct LearnState {
    bool  active      = false;  // true while waiting for incoming message
    bool  captured    = false;  // true after message received, awaiting confirm
    MidiInput::Message captured_msg = {0,0,0};
};

class MidiMapper {
public:
    // Apply all mappings to a single incoming message
    void apply(const MidiInput::Message& msg,
               FractalEngine& eng, BlendController& blend, ColorSynth& colorSynth);

    // Mapping list management
    void add(const MidiMapping& m);
    void remove(int idx);
    const std::vector<MidiMapping>& mappings() const { return m_mappings; }
    std::vector<MidiMapping>&       mappings()       { return m_mappings; }

    // MIDI-Learn helpers
    LearnState& learn() { return m_learn; }
    // Feed incoming messages into learn capture
    void feedLearn(const MidiInput::Message& msg);

private:
    std::vector<MidiMapping> m_mappings;
    LearnState               m_learn;

    static void applyToParam(MidiParam p, float val,
                             FractalEngine& eng, BlendController& blend,
                             ColorSynth& colorSynth);
};
