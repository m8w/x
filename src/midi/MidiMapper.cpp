#include "MidiMapper.h"
#include <cstring>
#include <algorithm>

// ── Param name table ─────────────────────────────────────────────────────────
const char* midiParamName(MidiParam p) {
    switch (p) {
        case MidiParam::BlendMandelbrot: return "Blend: Mandelbrot";
        case MidiParam::BlendJulia:      return "Blend: Julia";
        case MidiParam::BlendMandelbulb: return "Blend: Mandelbulb";
        case MidiParam::BlendEuclidean:  return "Blend: Euclidean";
        case MidiParam::BlendDiff:       return "Blend: Differential";
        case MidiParam::JuliaCX:         return "Julia C.x";
        case MidiParam::JuliaCY:         return "Julia C.y";
        case MidiParam::Power:           return "Power (n)";
        case MidiParam::Zoom:            return "Zoom";
        case MidiParam::OffsetX:         return "Offset X";
        case MidiParam::OffsetY:         return "Offset Y";
        case MidiParam::FormulaA:        return "Formula A (index)";
        case MidiParam::FormulaB:        return "Formula B (index)";
        case MidiParam::FormulaBlend:    return "Formula A\xe2\x86\x94 B blend";
        case MidiParam::PixelWeight:     return "Pixel inject";
        case MidiParam::LayerCount:      return "Layer count";
        case MidiParam::LayerOffset:     return "Layer offset";
        case MidiParam::GeoWarp:         return "Geo warp";
        case MidiParam::GeoRadius:       return "Geo radius";
        case MidiParam::GeoRotation:     return "Geo rotation";
        default:                         return "Unknown";
    }
}

// ── Apply one param value ─────────────────────────────────────────────────────
void MidiMapper::applyToParam(MidiParam p, float val,
                              FractalEngine& eng, BlendController& blend) {
    switch (p) {
        case MidiParam::BlendMandelbrot: blend.mandelbrot  = val; break;
        case MidiParam::BlendJulia:      blend.julia       = val; break;
        case MidiParam::BlendMandelbulb: blend.mandelbulb  = val; break;
        case MidiParam::BlendEuclidean:  blend.euclidean   = val; break;
        case MidiParam::BlendDiff:       blend.diff        = val; break;
        case MidiParam::JuliaCX:         eng.juliaC.x      = val; break;
        case MidiParam::JuliaCY:         eng.juliaC.y      = val; break;
        case MidiParam::Power:           eng.power         = std::max(val, 2.0f); break;
        case MidiParam::Zoom:            eng.zoom          = std::max(val, 0.01f); break;
        case MidiParam::OffsetX:         eng.offset.x      = val; break;
        case MidiParam::OffsetY:         eng.offset.y      = val; break;
        case MidiParam::FormulaA:        eng.formula       = (int)val; break;
        case MidiParam::FormulaB:        eng.formulaB      = (int)val; break;
        case MidiParam::FormulaBlend:    eng.formulaBlend  = val; break;
        case MidiParam::PixelWeight:     eng.pixelWeight   = val; break;
        case MidiParam::LayerCount:      eng.layerCount    = std::max(1,(int)val); break;
        case MidiParam::LayerOffset:     eng.layerOffset   = val; break;
        case MidiParam::GeoWarp:         eng.geoWarp       = val; break;
        case MidiParam::GeoRadius:       eng.geoRadius     = val; break;
        case MidiParam::GeoRotation:     eng.geoRotation   = val; break;
        default: break;
    }
}

// ── Apply all mappings to one incoming message ────────────────────────────────
void MidiMapper::apply(const MidiInput::Message& msg,
                       FractalEngine& eng, BlendController& blend) {
    int msgCh   = (msg.status & 0x0F) + 1;   // 1-based channel
    int msgType = (msg.status & 0xF0);

    for (const auto& m : m_mappings) {
        bool chOk = (m.channel == 0 || m.channel == msgCh);
        float raw = 0.0f;
        bool  hit = false;

        if (m.msgType == 0 && msgType == 0xB0 && chOk && msg.data1 == m.number) {
            // CC
            raw = msg.data2 / 127.0f;
            hit = true;
        } else if (m.msgType == 1 && msgType == 0x90 && msg.data2 > 0
                   && chOk && msg.data1 == m.number) {
            // NoteOn — velocity scales the value
            raw = msg.data2 / 127.0f;
            hit = true;
        } else if (m.msgType == 2 && chOk && msg.data1 == m.number
                   && (msgType == 0x80 || (msgType == 0x90 && msg.data2 == 0))) {
            // NoteOff / NoteOn-with-velocity-0 → set to minVal
            raw = 0.0f;
            hit = true;
        }

        if (hit) {
            float val = m.minVal + raw * (m.maxVal - m.minVal);
            applyToParam(m.param, val, eng, blend);
        }
    }
}

// ── MIDI Learn ────────────────────────────────────────────────────────────────
void MidiMapper::feedLearn(const MidiInput::Message& msg) {
    if (!m_learn.active) return;
    int msgType = (msg.status & 0xF0);
    // Only capture CC or NoteOn
    if (msgType != 0xB0 && msgType != 0x90) return;
    m_learn.captured_msg = msg;
    m_learn.captured     = true;
    m_learn.active       = false;
}

void MidiMapper::add(const MidiMapping& m) {
    m_mappings.push_back(m);
}

void MidiMapper::remove(int idx) {
    if (idx >= 0 && idx < (int)m_mappings.size())
        m_mappings.erase(m_mappings.begin() + idx);
}
