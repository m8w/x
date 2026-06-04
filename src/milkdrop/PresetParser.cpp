// PresetParser.cpp — parse / serialize MilkDrop .milk preset files
// Ported from MilkDropMac/Presets/MilkDropPreset.swift PresetParser

#include "PresetParser.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static float toFloat(const std::string& s, float def = 0.f) {
    if (s.empty()) return def;
    try { return std::stof(s); } catch (...) { return def; }
}

static int toInt(const std::string& s, int def = 0) {
    if (s.empty()) return def;
    try { return std::stoi(s); } catch (...) { return def; }
}

// Split "wave_N_key" or "shape_N_key" → (N, "key").
// Returns false if pattern doesn't match.
static bool parseIndexedKey(const std::string& key,
                             const std::string& prefix,
                             int& outIdx, std::string& outSubKey) {
    if (key.size() <= prefix.size()) return false;
    if (key.compare(0, prefix.size(), prefix) != 0) return false;
    auto rest = key.substr(prefix.size());
    auto ul = rest.find('_');
    if (ul == std::string::npos) return false;
    outIdx    = toInt(rest.substr(0, ul), -1);
    outSubKey = rest.substr(ul + 1);
    return outIdx >= 0 && !outSubKey.empty();
}

// ---------------------------------------------------------------------------
// parse()
// ---------------------------------------------------------------------------

PresetParameters PresetParser::parse(const std::string& text) {
    PresetParameters params;

    // Indexed wave/shape dicts: index → (subKey → value)
    std::unordered_map<int, std::unordered_map<std::string, std::string>> waveDict, shapeDict;

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '[') continue;

        auto eqPos = t.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key   = toLower(trim(t.substr(0, eqPos)));
        std::string value = t.substr(eqPos + 1);   // preserve case of value

        // ── Per-frame equations ──────────────────────────────────────────
        if (key.rfind("per_frame_", 0) == 0) {
            if (key == "per_frame_init_1" || key == "per_frame_init") {
                params.perFrameInit = value;
            } else {
                params.perFrame.push_back(value);
            }
            continue;
        }

        // ── Per-vertex (pixel) equations ─────────────────────────────────
        if (key.rfind("per_pixel_", 0) == 0) {
            params.perVertex.push_back(value);
            continue;
        }

        // ── HLSL shader blocks (stored, not executed) ────────────────────
        if (key.rfind("warp_", 0) == 0 &&
            key.size() > 5 && key.substr(key.size() - 5) == "_hlsl") {
            params.warpHLSL += value + "\n";
            continue;
        }
        if (key.rfind("comp_", 0) == 0 &&
            key.size() > 5 && key.substr(key.size() - 5) == "_hlsl") {
            params.compHLSL += value + "\n";
            continue;
        }

        // ── Waves ────────────────────────────────────────────────────────
        if (key.rfind("wave_", 0) == 0) {
            int idx; std::string sub;
            if (parseIndexedKey(key, "wave_", idx, sub))
                waveDict[idx][sub] = value;
            continue;
        }

        // ── Shapes ───────────────────────────────────────────────────────
        if (key.rfind("shape_", 0) == 0) {
            int idx; std::string sub;
            if (parseIndexedKey(key, "shape_", idx, sub))
                shapeDict[idx][sub] = value;
            continue;
        }

        // ── Scalar parameters ────────────────────────────────────────────
        auto get = [&](float def){ return toFloat(value, def); };
        auto geti = [&](int def){ return toInt(value, def); };

        if      (key == "frating")                   params.rating              = get(3);
        else if (key == "fgammadj")                  params.gamma               = get(1);
        else if (key == "fdecay" ||
                 key == "fvideoechodecay")            params.decay               = get(0.98f);
        else if (key == "fzoom")                     params.zoomAmount          = get(1);
        else if (key == "frot")                      params.rotatAmount         = get(0);
        else if (key == "fwarpscale")                params.warpScale           = get(1);
        else if (key == "fwarpanimspeed")            params.warpSpeed           = get(1);
        else if (key == "fx" || key == "fxcenter")   params.centreX             = get(0.5f);
        else if (key == "fy" || key == "fycenter")   params.centreY             = get(0.5f);
        else if (key == "szx")                       params.szx                 = get(1);
        else if (key == "szy")                       params.szy                 = get(1);
        else if (key == "fvideoechodecayalpha" ||
                 key == "fvideoechozoom")             params.videoEchoAlpha      = get(0);
        else if (key == "fvideoechodecayzoom")       params.videoEchoZoom       = get(1);
        else if (key == "ivideoechodecayorientation")params.videoEchoOrientation = geti(0);
        // Legacy top-level scalars (no "f" prefix — initial values for equations)
        else if (key == "zoom")                      params.zoomAmount          = get(1);
        else if (key == "rot")                       params.rotatAmount         = get(0);
        else if (key == "warp")                      params.warpScale           = get(0);
        else if (key == "decay")                     params.decay               = get(0.98f);
        else if (key == "cx")                        params.centreX             = get(0.5f);
        else if (key == "cy")                        params.centreY             = get(0.5f);
        else if (key == "dx")                        params.warpX               = get(0);
        else if (key == "dy")                        params.warpY               = get(0);
        else if (key == "sx")                        params.szx                 = get(1);
        else if (key == "sy")                        params.szy                 = get(1);
        // Legacy global wave (nWaveMode format)
        else if (key == "nwavemode")                 params.legacyWaveMode      = geti(0);
        else if (key == "fwavealpha")                params.legacyWaveA         = get(0.8f);
        else if (key == "fwavescale")                params.legacyWaveScale     = get(1);
        else if (key == "fwavesmoothing")            params.legacyWaveSmooth    = get(0.5f);
        else if (key == "fwaver")                    params.legacyWaveR         = get(1);
        else if (key == "fwaveg")                    params.legacyWaveG         = get(1);
        else if (key == "fwaveb")                    params.legacyWaveB         = get(1);
        else if (key == "badditivewave" ||
                 key == "badditivewaves")             params.legacyWaveAdditive  = geti(0) == 1;
        else if (key == "bwavedots")                 params.legacyWaveDots      = geti(0) == 1;
        // (other legacy keys silently ignored)
    }

    // ── Build wave objects ────────────────────────────────────────────────
    for (auto& kv : waveDict) {
        int idx = kv.first;
        auto& dict = kv.second;
        PresetWave w;
        w.id        = idx;
        auto gf = [&dict](const std::string& k, float d) {
            auto it = dict.find(k);
            return it != dict.end() ? toFloat(it->second, d) : d;
        };
        auto gi = [&dict](const std::string& k, int d) {
            auto it = dict.find(k);
            return it != dict.end() ? toInt(it->second, d) : d;
        };
        w.enabled   = gi("enabled",   0) == 1;
        w.samples   = gi("samples",   512);
        w.sep       = gi("sep",       0);
        w.scaling   = gf("scaling",   1.f);
        w.smoothing = gf("smoothing", 0.5f);
        w.r = gf("r", 1); w.g = gf("g", 1); w.b = gf("b", 1); w.a = gf("a", 1);
        w.useDots   = gi("usedots",   0) == 1;
        w.drawThick = gi("drawthick", 0) == 1;
        w.additive  = gi("additive",  0) == 1;
        for (int i = 1; ; ++i) {
            auto it = dict.find("per_point_" + std::to_string(i));
            if (it == dict.end()) break;
            w.perPoint.push_back(it->second);
        }
        params.waves.push_back(w);
    }
    std::sort(params.waves.begin(), params.waves.end(),
              [](const PresetWave& a, const PresetWave& b){ return a.id < b.id; });

    // ── Build shape objects ───────────────────────────────────────────────
    for (auto& kv : shapeDict) {
        int idx = kv.first;
        auto& dict = kv.second;
        PresetShape s;
        s.id = idx;
        auto gf = [&dict](const std::string& k, float d) {
            auto it = dict.find(k);
            return it != dict.end() ? toFloat(it->second, d) : d;
        };
        auto gi = [&dict](const std::string& k, int d) {
            auto it = dict.find(k);
            return it != dict.end() ? toInt(it->second, d) : d;
        };
        s.enabled      = gi("enabled",      0) == 1;
        s.sides        = gi("sides",         4);
        s.additive     = gi("additive",      0) == 1;
        s.thickOutline = gi("thickoutline",  0) == 1;
        s.textured     = gi("textured",      0) == 1;
        s.x            = gf("x",      0.5f);
        s.y            = gf("y",      0.5f);
        s.radius       = gf("radius", 0.1f);
        s.ang          = gf("ang",    0.f);
        s.tex_ang      = gf("tex_ang",  0.f);
        s.tex_zoom     = gf("tex_zoom", 1.f);
        s.r  = gf("r",  1); s.g  = gf("g",  1); s.b  = gf("b",  1); s.a  = gf("a",  1);
        s.r2 = gf("r2", 1); s.g2 = gf("g2", 1); s.b2 = gf("b2", 1); s.a2 = gf("a2", 1);
        s.border_r = gf("border_r", 1); s.border_g = gf("border_g", 1);
        s.border_b = gf("border_b", 1); s.border_a = gf("border_a", 0.5f);
        for (int i = 1; ; ++i) {
            auto it = dict.find("per_frame_" + std::to_string(i));
            if (it == dict.end()) break;
            s.perFrame.push_back(it->second);
        }
        params.shapes.push_back(s);
    }
    std::sort(params.shapes.begin(), params.shapes.end(),
              [](const PresetShape& a, const PresetShape& b){ return a.id < b.id; });

    return params;
}

// ---------------------------------------------------------------------------
// serialize()
// ---------------------------------------------------------------------------

std::string PresetParser::serialize(const PresetParameters& p, const std::string& name) {
    std::ostringstream out;
    out << "[preset00]\n";
    out << "fRating="          << p.rating          << "\n";
    out << "fGammaAdj="        << p.gamma            << "\n";
    out << "fDecay="           << p.decay            << "\n";
    out << "fZoom="            << p.zoomAmount       << "\n";
    out << "fRot="             << p.rotatAmount      << "\n";
    out << "fWarpScale="       << p.warpScale        << "\n";
    out << "fWarpAnimSpeed="   << p.warpSpeed        << "\n";
    out << "fXCentre="         << p.centreX          << "\n";
    out << "fYCentre="         << p.centreY          << "\n";
    out << "szx="              << p.szx              << "\n";
    out << "szy="              << p.szy              << "\n";
    out << "fVideoEchoDecayAlpha="        << p.videoEchoAlpha       << "\n";
    out << "fVideoEchoDecayZoom="         << p.videoEchoZoom        << "\n";
    out << "iVideoEchoDecayOrientation="  << p.videoEchoOrientation << "\n";

    if (!p.perFrameInit.empty())
        out << "per_frame_init_1=" << p.perFrameInit << "\n";
    for (int i = 0; i < (int)p.perFrame.size(); ++i)
        out << "per_frame_" << (i + 1) << "=" << p.perFrame[i] << "\n";
    for (int i = 0; i < (int)p.perVertex.size(); ++i)
        out << "per_pixel_" << (i + 1) << "=" << p.perVertex[i] << "\n";

    // HLSL (split back into numbered lines)
    if (!p.warpHLSL.empty()) {
        std::istringstream ss(p.warpHLSL);
        std::string l; int i = 1;
        while (std::getline(ss, l))
            out << "warp_" << i++ << "_hlsl=" << l << "\n";
    }
    if (!p.compHLSL.empty()) {
        std::istringstream ss(p.compHLSL);
        std::string l; int i = 1;
        while (std::getline(ss, l))
            out << "comp_" << i++ << "_hlsl=" << l << "\n";
    }

    for (const auto& w : p.waves) {
        if (!w.enabled) continue;
        std::string px = "wave_" + std::to_string(w.id) + "_";
        out << px << "enabled=1\n";
        out << px << "samples="   << w.samples   << "\n";
        out << px << "sep="       << w.sep        << "\n";
        out << px << "scaling="   << w.scaling    << "\n";
        out << px << "smoothing=" << w.smoothing  << "\n";
        out << px << "r=" << w.r << "\n" << px << "g=" << w.g << "\n";
        out << px << "b=" << w.b << "\n" << px << "a=" << w.a << "\n";
        out << px << "usedots="   << (w.useDots   ? 1 : 0) << "\n";
        out << px << "drawthick=" << (w.drawThick ? 1 : 0) << "\n";
        out << px << "additive="  << (w.additive  ? 1 : 0) << "\n";
        for (int i = 0; i < (int)w.perPoint.size(); ++i)
            out << px << "per_point_" << (i + 1) << "=" << w.perPoint[i] << "\n";
    }

    for (const auto& s : p.shapes) {
        if (!s.enabled) continue;
        std::string px = "shape_" + std::to_string(s.id) + "_";
        out << px << "enabled=1\n";
        out << px << "sides="        << s.sides        << "\n";
        out << px << "additive="     << (s.additive     ? 1 : 0) << "\n";
        out << px << "thickoutline=" << (s.thickOutline ? 1 : 0) << "\n";
        out << px << "textured="     << (s.textured     ? 1 : 0) << "\n";
        out << px << "x="      << s.x      << "\n" << px << "y=" << s.y << "\n";
        out << px << "radius=" << s.radius << "\n";
        out << px << "ang="    << s.ang    << "\n";
        out << px << "tex_ang="  << s.tex_ang  << "\n";
        out << px << "tex_zoom=" << s.tex_zoom << "\n";
        out << px << "r="  << s.r  << "\n" << px << "g="  << s.g  << "\n";
        out << px << "b="  << s.b  << "\n" << px << "a="  << s.a  << "\n";
        out << px << "r2=" << s.r2 << "\n" << px << "g2=" << s.g2 << "\n";
        out << px << "b2=" << s.b2 << "\n" << px << "a2=" << s.a2 << "\n";
        out << px << "border_r=" << s.border_r << "\n";
        out << px << "border_g=" << s.border_g << "\n";
        out << px << "border_b=" << s.border_b << "\n";
        out << px << "border_a=" << s.border_a << "\n";
        for (int i = 0; i < (int)s.perFrame.size(); ++i)
            out << px << "per_frame_" << (i + 1) << "=" << s.perFrame[i] << "\n";
    }

    return out.str();
}
