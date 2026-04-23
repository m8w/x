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
        case MidiParam::FormulaParam:    return "Formula extra param";
        case MidiParam::PixelWeight:     return "Pixel inject";
        case MidiParam::LayerCount:      return "Layer count";
        case MidiParam::LayerOffset:     return "Layer offset";
        case MidiParam::GeoWarp:         return "Geo warp";
        case MidiParam::GeoRadius:       return "Geo radius";
        case MidiParam::GeoRotation:     return "Geo rotation";
        case MidiParam::GeoMirror:       return "Geo mirror (0-3)";
        case MidiParam::GeoKaleid:       return "Kaleidoscope segments";
        case MidiParam::ColorHue:        return "Color: Hue";
        case MidiParam::ColorSat:        return "Color: Saturation";
        case MidiParam::ColorLum:        return "Color: Luminance";
        case MidiParam::ColorAltHue:     return "Color: Alt Hue";
        case MidiParam::ColorAltRate:    return "Color: Alt Rate";
        case MidiParam::ColorHueOscRate: return "Color: Hue Osc Rate";
        case MidiParam::ColorHueOscAmp:  return "Color: Hue Osc Amp";
        case MidiParam::ColorLumOscAmp:  return "Color: Lum Osc Amp";
        case MidiParam::OverlayBlend:    return "Overlay blend";
        case MidiParam::StreamBlendMode: return "Stream blend mode (0-41)";
        case MidiParam::VidFilter:       return "Vid filter ID (0-11)";
        case MidiParam::VidFilterA:      return "Vid filter param A";
        case MidiParam::VidFilterB:      return "Vid filter param B";
        case MidiParam::OvrFilter:       return "Ovr filter ID (0-18)";
        case MidiParam::OvrFilterA:      return "Ovr filter param A";
        case MidiParam::OvrFilterB:      return "Ovr filter param B";
        case MidiParam::ChaosMode:       return "Chaos mode (0-7)";
        case MidiParam::ChaosStrength:   return "Chaos strength";
        case MidiParam::ChaosScale:      return "Chaos scale";
        case MidiParam::ChaosSpeed:      return "Chaos speed";
        default:                         return "Unknown";
    }
}

// ── Apply one param value ─────────────────────────────────────────────────────
void MidiMapper::applyToParam(MidiParam p, float val,
                              FractalEngine& eng, BlendController& blend,
                              ColorSynth& colorSynth) {
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
        case MidiParam::FormulaParam:    eng.formulaParam  = val; break;
        case MidiParam::PixelWeight:     eng.pixelWeight   = val; break;
        case MidiParam::LayerCount:      eng.layerCount    = std::max(1,(int)val); break;
        case MidiParam::LayerOffset:     eng.layerOffset   = val; break;
        case MidiParam::GeoWarp:         eng.geoWarp       = val; break;
        case MidiParam::GeoRadius:       eng.geoRadius     = val; break;
        case MidiParam::GeoRotation:     eng.geoRotation   = val; break;
        // ── Mirror / Kaleidoscope ─────────────────────────────────────────────
        case MidiParam::GeoMirror:       eng.geoMirror     = (int)std::clamp(val, 0.0f, 3.0f); break;
        case MidiParam::GeoKaleid:       eng.geoKaleid     = (int)std::clamp(val, 0.0f, 16.0f); break;
        // ── Color Synthesizer ─────────────────────────────────────────────────
        case MidiParam::ColorHue:        colorSynth.hueBase        = val; break;
        case MidiParam::ColorSat:        colorSynth.satBase        = val; break;
        case MidiParam::ColorLum:        colorSynth.lumBase        = val; break;
        case MidiParam::ColorAltHue:     colorSynth.hueAlt         = val; break;
        case MidiParam::ColorAltRate:    colorSynth.altRate        = val; break;
        case MidiParam::ColorHueOscRate: colorSynth.hueOscRate     = val; break;
        case MidiParam::ColorHueOscAmp:  colorSynth.hueOscAmp      = val; break;
        case MidiParam::ColorLumOscAmp:  colorSynth.lumOscAmp      = val; break;
        case MidiParam::OverlayBlend:    eng.overlayBlend    = std::clamp(val, 0.0f, 1.0f); break;
        case MidiParam::StreamBlendMode: eng.streamBlendMode = (int)std::clamp(val, 0.0f, 41.0f); break;
        case MidiParam::VidFilter:       eng.vidFilter       = (int)std::clamp(val, 0.0f, 11.0f); break;
        case MidiParam::VidFilterA:      eng.vidFilterA      = val; break;
        case MidiParam::VidFilterB:      eng.vidFilterB      = val; break;
        case MidiParam::OvrFilter:       eng.ovrFilter       = (int)std::clamp(val, 0.0f, 18.0f); break;
        case MidiParam::OvrFilterA:      eng.ovrFilterA      = val; break;
        case MidiParam::OvrFilterB:      eng.ovrFilterB      = val; break;
        case MidiParam::ChaosMode:       eng.chaosMode       = (int)std::clamp(val, 0.0f, 7.0f); break;
        case MidiParam::ChaosStrength:   eng.chaosStrength   = std::clamp(val, 0.0f, 1.0f); break;
        case MidiParam::ChaosScale:      eng.chaosScale      = std::clamp(val, 0.5f, 8.0f); break;
        case MidiParam::ChaosSpeed:      eng.chaosSpeed      = std::clamp(val, 0.0f, 3.0f); break;
        default: break;
    }
}

// ── Apply all mappings to one incoming message ────────────────────────────────
void MidiMapper::apply(const MidiInput::Message& msg,
                       FractalEngine& eng, BlendController& blend,
                       ColorSynth& colorSynth) {
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
        } else if (m.msgType == 3 && msgType == 0xC0 && chOk) {
            // Program Change — program number scaled to [minVal, maxVal]
            raw = msg.data1 / 127.0f;
            hit = true;
        }

        if (hit) {
            float val = m.minVal + raw * (m.maxVal - m.minVal);
            applyToParam(m.param, val, eng, blend, colorSynth);
        }
    }
}

// ── MIDI Learn ────────────────────────────────────────────────────────────────
void MidiMapper::feedLearn(const MidiInput::Message& msg) {
    if (!m_learn.active) return;
    int msgType = (msg.status & 0xF0);
    // Capture CC, NoteOn, or Program Change
    if (msgType != 0xB0 && msgType != 0x90 && msgType != 0xC0) return;
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
