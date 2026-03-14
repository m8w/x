#include "EquationEditor.h"
#include "midi/MidiOutput.h"
#include "FilePicker.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

static const char* kResLabels[] = {"1280x720", "1920x1080", "2560x1440", "3840x2160 (4K)"};
static const int   kResW[]      = {1280, 1920, 2560, 3840};
static const int   kResH[]      = { 720, 1080, 1440, 2160};
static const char* kShapeLabels[] = {"Circle", "Polygon", "Star", "Grid"};

EquationEditor::EquationEditor(FractalEngine& engine, BlendController& blend,
                                GlitchEngine& glitch, ColorSynth& colorSynth,
                                VideoInput& videoIn, StreamOutput& streamOut,
                                MidiInput& midiIn, MidiOutput& midiOut,
                                MidiMapper& midiMapper, MidiGenerator& midiGen)
    : m_engine(engine), m_blend(blend), m_glitch(glitch), m_colorSynth(colorSynth),
      m_videoIn(videoIn), m_streamOut(streamOut),
      m_midiIn(midiIn), m_midiOut(midiOut),
      m_midiMapper(midiMapper), m_midiGen(midiGen) {}

void EquationEditor::draw() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({360, 980}, ImGuiCond_Once);
    ImGui::Begin("Fractal Stream Controls");

    if (ImGui::CollapsingHeader("Blend"))
        drawBlendPanel();
    if (ImGui::CollapsingHeader("Fractal Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        drawFractalPanel();
    if (ImGui::CollapsingHeader("Animation"))
        drawAnimPanel();
    if (ImGui::CollapsingHeader("Euclidean Geometry"))
        drawGeometryPanel();
    if (ImGui::CollapsingHeader("Video Input", ImGuiTreeNodeFlags_DefaultOpen))
        drawVideoPanel();
    if (ImGui::CollapsingHeader("Color Synthesizer"))
        drawColorSynthPanel();
    if (ImGui::CollapsingHeader("Chaos Effects"))
        drawChaosPanel();
    if (ImGui::CollapsingHeader("Distortion / Metaballs"))
        drawDistortionPanel();
    if (ImGui::CollapsingHeader("Stream Output"))
        drawStreamPanel();

    ImGui::End();

    // UI2 — MIDI Mapper (separate window)
    drawMidiWindow();
    drawGlitchPanel();
}

void EquationEditor::drawBlendPanel() {
    ImGui::SliderFloat("Mandelbrot",   &m_blend.mandelbrot, 0.0f, 1.0f);
    ImGui::SliderFloat("Julia",        &m_blend.julia,      0.0f, 1.0f);
    ImGui::SliderFloat("Mandelbulb",   &m_blend.mandelbulb, 0.0f, 1.0f);
    ImGui::SliderFloat("Euclidean",    &m_blend.euclidean,  0.0f, 1.0f);
    ImGui::SliderFloat("Differential", &m_blend.diff,       0.0f, 1.0f);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("de Jong attractor ODE field — parameters driven by Julia C and Power");

    float total = m_blend.mandelbrot + m_blend.julia +
                  m_blend.mandelbulb + m_blend.euclidean + m_blend.diff;
    ImGui::Separator();
    ImGui::Text("Total blend: %.2f", total);
    if (ImGui::Button("Normalize")) {
        if (total > 0.001f) {
            m_blend.mandelbrot /= total;
            m_blend.julia      /= total;
            m_blend.mandelbulb /= total;
            m_blend.euclidean  /= total;
            m_blend.diff       /= total;
        }
    }
}

static const char* kFormulas[] = {
    // ── Classic 11 ───────────────────────────────────────────────────────────
    "z\xc2\xb2 + c  (Mandelbrot)",               //  0
    "sin(z) + c",                                  //  1
    "exp(z) + c",                                  //  2
    "cos(z) + c",                                  //  3
    "sinh(z) + c",                                 //  4
    "cosh(z) + c",                                 //  5
    "Burning Ship",                                //  6
    "Tricorn",                                     //  7
    "Newton z\xc2\xb3\xe2\x88\x92" "1",           //  8
    "Phoenix",                                     //  9
    "z\xe2\x81\xbf + c  (power)",                 // 10
    // ── New formulas ─────────────────────────────────────────────────────────
    "tan(z) + c",                                  // 11
    "z\xc2\xb7""exp(z) + c",                      // 12  z·exp(z)+c
    "Celtic  (|Re(z\xc2\xb2)|,Im(z\xc2\xb2))+c", // 13
    "Magnet I",                                    // 14
    "z\xe1\xb5\x87 + c  (self-power)",             // 15  zᶻ+c
    "Manowar  z\xc2\xb2+z\xe2\x82\x99\xe2\x82\x8b\xe2\x82\x81+c", // 16
    "Perp Burning Ship",                           // 17
    "Time-spiral  [\xe2\x88\x82param]",            // 18  animated by formulaParam
    "z\xc2\xb3 + z + c",                           // 19
    "cosh(conj(z)) + c",                           // 20
    "Polar\xe2\x86\x92""Cart warp  [\xe2\x88\x82param]", // 21  formulaParam=twist
};
static constexpr int kNumFormulas = 22;
static const char* k3DTypes[] = {
    "Mandelbulb",
    "Mandelbox",
    "Quaternion Julia",
};

void EquationEditor::drawFractalPanel() {
    // ── Iteration formula A × B cross-blend ──────────────────────────────────
    ImGui::TextDisabled("Formula A  \xe2\x86\x94  Formula B");
    ImGui::Combo("Formula A##sel", &m_engine.formula,  kFormulas, kNumFormulas);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
        "11=tan  12=z\xc2\xb7""exp(z)  13=Celtic  14=Magnet I  15=z\xe1\xb5\x87\n"
        "16=Manowar  17=PerpShip  18=Time-spiral  19=z\xc2\xb3+z  20=cosh(conj)  21=PolarWarp");
    ImGui::Combo("Formula B##sel", &m_engine.formulaB, kFormulas, kNumFormulas);
    ImGui::SliderFloat("A \xe2\x86\x94 B blend", &m_engine.formulaBlend, 0.0f, 1.0f);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 = pure Formula A   1 = pure Formula B   0.5 = crossfade");

    // ── Formula extra parameter (used by Time-spiral and Polar warp) ──────────
    ImGui::Separator();
    ImGui::TextDisabled("Formula extra param  (Time-spiral speed / Polar warp twist)");
    ImGui::SliderFloat("Param", &m_engine.formulaParam, -6.283f, 6.283f, "%.3f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
        "Formula 18 (Time-spiral): rotation speed in rad/s — higher = faster spiral.\n"
        "Formula 21 (Polar warp): angle multiplier — 1=normal, 2=doubled twist, -1=reverse.\n"
        "Other formulas: available as u_formula_param but currently unused.");

    // ── Auto-animate formula param ────────────────────────────────────────────
    {
        static bool  animParam = false;
        static float paramSpeed = 0.5f;
        static float paramAmp   = 3.14159f;
        ImGui::Checkbox("Auto-animate param", &animParam);
        if (animParam) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat("##paramsp", &paramSpeed, 0.01f, 4.0f, "spd %.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat("##paramamp", &paramAmp, 0.1f, 6.283f, "amp %.2f");
            m_engine.formulaParam = paramAmp * (float)sin(ImGui::GetTime() * paramSpeed);
        }
    }

    // ── Formula Presets ───────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("── Formula Presets ────────────────────────────────────");

    struct FPre {
        const char* name;
        int a, b; float blend;
        float cx, cy, power, param;
        const char* tip;
    };
    static const FPre kFPre[] = {
        {"Classic",    0,  0, 0.0f,  0.000f, 0.000f, 2.0f, 1.0f,
         "Pure Mandelbrot — the original escape-time fractal"},
        {"Electric",   1,  2, 0.35f,-0.700f, 0.270f, 2.0f, 1.0f,
         "sin(z) fading into exp(z) — lightning-bolt filaments"},
        {"Phoenix",    9,  0, 0.0f,  0.560f,-0.500f, 2.0f, 1.0f,
         "Phoenix recurrence — feather-wing symmetry"},
        {"Ghosts",     8,  7, 0.4f, -0.123f, 0.745f, 2.0f, 1.0f,
         "Newton z\xc2\xb3-1 bleeding into Tricorn — ghost-convergence shells"},
        {"Fire Ship",  6, 11, 0.5f, -0.750f, 0.100f, 2.0f, 1.0f,
         "Burning Ship + Tangent blend — fiery spike corona"},
        {"Magnet Storm",14,15,0.3f, -0.500f, 0.000f, 2.0f, 1.0f,
         "Magnet I attractor morphing into z\xe1\xb5\x87 self-power"},
        {"Vortex",    12, 18, 0.5f, -0.400f, 0.200f, 2.0f, 1.5f,
         "z\xc2\xb7""exp(z) spirals + Time-spiral rotation — galaxy arms"},
        {"Celtic Cross",13,7, 0.5f,  0.000f, 0.650f, 2.0f, 1.0f,
         "Celtic fold blending with Tricorn — knotwork symmetry"},
        {"Manowar+Ship",16,6, 0.4f, -0.800f, 0.156f, 2.0f, 1.0f,
         "Manowar memory + Burning Ship folds — chaotic coastlines"},
        {"Polar Drift", 21,0, 0.3f, -0.700f, 0.270f, 2.0f, 1.3f,
         "Polar warp blending with Mandelbrot — twisted orbit paths"},
        {"Cubic Galaxy",19,12,0.45f,-0.620f, 0.440f, 3.0f, 1.0f,
         "Cubic+linear + z\xc2\xb7""exp(z) — three-arm spiral with halos"},
        {"Cosh Mirror", 20, 3, 0.5f,  0.285f, 0.010f, 2.0f, 1.0f,
         "cosh(conj(z)) + cos(z) blend — bilateral mirror symmetry"},
    };
    static constexpr int kNFPre = 12;

    for (int p = 0; p < kNFPre; p++) {
        if (p > 0 && p % 4 != 0) ImGui::SameLine();
        if (ImGui::SmallButton(kFPre[p].name)) {
            const auto& pr = kFPre[p];
            m_engine.formula       = pr.a;
            m_engine.formulaB      = pr.b;
            m_engine.formulaBlend  = pr.blend;
            m_engine.juliaC        = {pr.cx, pr.cy};
            m_engine.power         = pr.power;
            m_engine.formulaParam  = pr.param;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kFPre[p].tip);
    }

    ImGui::Separator();

    // ── Pixel coordinate injection ────────────────────────────────────────────
    ImGui::TextDisabled("Pixel coord as equation variable");
    ImGui::SliderFloat("Pixel inject", &m_engine.pixelWeight, 0.0f, 1.0f);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Adds the screen-space pixel position into the iteration seed.\n"
                          "Each pixel's location becomes a live variable in the equation.");

    ImGui::Separator();

    // ── Multi-layer repetition ────────────────────────────────────────────────
    ImGui::TextDisabled("Layer repetition");
    ImGui::SliderInt("Layers (1-4)", &m_engine.layerCount, 1, 4);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Runs the equation N times with spatially offset seeds\n"
                          "and averages the results — creates woven depth.");
    if (m_engine.layerCount > 1)
        ImGui::SliderFloat("Layer offset", &m_engine.layerOffset, 0.01f, 1.0f);

    ImGui::Separator();

    // ── Geometry coupling ─────────────────────────────────────────────────────
    ImGui::TextDisabled("Euclidean \xe2\x86\x94 fractal coupling");
    ImGui::SliderFloat("Geo warp", &m_engine.geoWarp, 0.0f, 1.0f);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("SDF gradient bends the fractal orbit inside the iteration loop.\n"
                          "0 = coloring only   1 = maximum algebraic coupling");

    ImGui::Separator();

    // ── Complex parameters ────────────────────────────────────────────────────
    ImGui::DragFloat2("Julia / Phoenix C", &m_engine.juliaC.x,
                      0.001f, -2.0f, 2.0f, "%.4f");
    ImGui::DragFloat("Power (z^n / bulb)", &m_engine.power,
                     0.1f, 2.0f, 16.0f, "%.1f");
    ImGui::SliderInt("Max iterations",  &m_engine.maxIter, 16, 512);
    ImGui::DragFloat("Bailout radius",  &m_engine.bailout, 0.1f, 2.0f, 10.0f);

    ImGui::Separator();

    // ── View ──────────────────────────────────────────────────────────────────
    ImGui::DragFloat("Zoom", &m_engine.zoom, 0.01f, 0.1f, 1000.0f, "%.3f",
                     ImGuiSliderFlags_Logarithmic);
    ImGui::DragFloat2("Offset (x,y)", &m_engine.offset.x, 0.001f);
    if (ImGui::Button("Reset view")) {
        m_engine.zoom   = 1.0f;
        m_engine.offset = {0.0f, 0.0f};
    }

    // ── Julia animation ───────────────────────────────────────────────────────
    static bool  animJulia = false;
    static float animSpeed = 0.3f;
    ImGui::Checkbox("Animate Julia C", &animJulia);
    if (animJulia) {
        ImGui::SameLine();
        ImGui::SliderFloat("##aspeed", &animSpeed, 0.01f, 2.0f);
        double t = ImGui::GetTime();
        m_engine.juliaC.x = (float)(0.7885 * cos(t * animSpeed));
        m_engine.juliaC.y = (float)(0.7885 * sin(t * animSpeed));
    }

    ImGui::Separator();

    // ── 3-D fractal (mandelbulb.frag) ─────────────────────────────────────────
    ImGui::TextDisabled("3-D fractal (when Mandelbulb blend > 0.5)");
    ImGui::Combo("3D type", &m_engine.fractal3D, k3DTypes, 3);
    if (m_engine.fractal3D == 1) {
        ImGui::DragFloat("MB scale", &m_engine.mbScale, 0.01f, 0.5f, 4.0f);
        ImGui::DragFloat("MB fold",  &m_engine.mbFold,  0.01f, 0.1f, 3.0f);
    }
}

void EquationEditor::drawGeometryPanel() {
    ImGui::Combo("Shape", &m_engine.geoShape, kShapeLabels, 4);
    if (m_engine.geoShape == 1 || m_engine.geoShape == 2)
        ImGui::SliderInt("Sides", &m_engine.geoSides, 3, 12);
    ImGui::DragFloat("Radius",   &m_engine.geoRadius,   0.01f, 0.05f, 2.0f);
    ImGui::DragFloat("Rotation", &m_engine.geoRotation, 0.01f);
    ImGui::Checkbox("Tile / repeat", &m_engine.geoTile);

    ImGui::Separator();

    // ── Mirror ────────────────────────────────────────────────────────────────
    static const char* kMirrorLabels[] = {"None", "Mirror X", "Mirror Y", "Mirror XY"};
    ImGui::Combo("Mirror", &m_engine.geoMirror, kMirrorLabels, 4);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Folds the complex plane on one or both axes before iteration.\n"
                          "Produces 2-fold or 4-fold reflective symmetry across the fractal.");

    // ── Kaleidoscope ──────────────────────────────────────────────────────────
    ImGui::Separator();
    bool kaleidOn = m_engine.geoKaleid >= 2;
    if (ImGui::Checkbox("Kaleidoscope", &kaleidOn)) {
        m_engine.geoKaleid = kaleidOn ? 6 : 0;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Folds the plane into N angular wedges.\n"
                          "Creates radial symmetry — like spinning a fractal in a mirror tunnel.");
    if (kaleidOn) {
        ImGui::SliderInt("Segments", &m_engine.geoKaleid, 2, 16);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Number of repeating wedges.\n"
                              "2=bilateral  3=tri  6=hex  12=clock  16=snowflake");
        // Quick-pick presets
        ImGui::TextDisabled("Segments:");
        ImGui::SameLine();
        if (ImGui::SmallButton("3"))  m_engine.geoKaleid = 3;
        ImGui::SameLine();
        if (ImGui::SmallButton("4"))  m_engine.geoKaleid = 4;
        ImGui::SameLine();
        if (ImGui::SmallButton("6"))  m_engine.geoKaleid = 6;
        ImGui::SameLine();
        if (ImGui::SmallButton("8"))  m_engine.geoKaleid = 8;
        ImGui::SameLine();
        if (ImGui::SmallButton("12")) m_engine.geoKaleid = 12;
        ImGui::SameLine();
        if (ImGui::SmallButton("16")) m_engine.geoKaleid = 16;
    }

    // ── Scene presets (shape + mirror + kaleidoscope combos) ──────────────────
    ImGui::Separator();
    ImGui::TextDisabled("── Scene Presets ─────────────────────────────");

    // Snowflake
    if (ImGui::SmallButton("Snowflake")) {
        m_engine.geoShape    = 2;   // star
        m_engine.geoSides    = 6;
        m_engine.geoRadius   = 0.35f;
        m_engine.geoMirror   = 3;   // XY
        m_engine.geoKaleid   = 6;
        m_engine.geoWarp     = 0.3f;
        m_engine.geoTile     = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("6-arm snowflake: star + XY mirror + hex kaleidoscope");
    ImGui::SameLine();

    // Tunnel
    if (ImGui::SmallButton("Tunnel")) {
        m_engine.geoShape    = 0;   // circle
        m_engine.geoRadius   = 0.5f;
        m_engine.geoMirror   = 0;
        m_engine.geoKaleid   = 8;
        m_engine.geoWarp     = 0.6f;
        m_engine.geoTile     = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tiled circles + 8-segment kaleid: wormhole effect");
    ImGui::SameLine();

    // Mandala
    if (ImGui::SmallButton("Mandala")) {
        m_engine.geoShape    = 1;   // polygon
        m_engine.geoSides    = 8;
        m_engine.geoRadius   = 0.4f;
        m_engine.geoMirror   = 3;
        m_engine.geoKaleid   = 12;
        m_engine.geoWarp     = 0.2f;
        m_engine.geoTile     = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Octagon + XY mirror + 12-segment kaleid");
    ImGui::SameLine();

    // Lattice
    if (ImGui::SmallButton("Lattice")) {
        m_engine.geoShape    = 3;   // grid
        m_engine.geoRadius   = 0.25f;
        m_engine.geoMirror   = 3;
        m_engine.geoKaleid   = 4;
        m_engine.geoWarp     = 0.4f;
        m_engine.geoTile     = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid + XY mirror + 4-segment kaleid: crystal lattice");
    ImGui::SameLine();

    // Prism
    if (ImGui::SmallButton("Prism")) {
        m_engine.geoShape    = 2;   // star
        m_engine.geoSides    = 3;
        m_engine.geoRadius   = 0.45f;
        m_engine.geoMirror   = 1;   // X only
        m_engine.geoKaleid   = 3;
        m_engine.geoWarp     = 0.5f;
        m_engine.geoTile     = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Triangle star + X mirror + 3-segment kaleid");

    // Second row
    if (ImGui::SmallButton("Eye")) {
        m_engine.geoShape    = 0;   // circle
        m_engine.geoRadius   = 0.3f;
        m_engine.geoMirror   = 2;   // Y mirror
        m_engine.geoKaleid   = 2;
        m_engine.geoWarp     = 0.8f;
        m_engine.geoTile     = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Circle + Y mirror + bilateral fold: eye / lens shape");
    ImGui::SameLine();

    if (ImGui::SmallButton("Fractal Grid")) {
        m_engine.geoShape    = 3;   // grid
        m_engine.geoRadius   = 0.15f;
        m_engine.geoMirror   = 0;
        m_engine.geoKaleid   = 0;
        m_engine.geoWarp     = 0.9f;
        m_engine.geoTile     = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Dense tiled grid with max warp: fractal interference grid");
    ImGui::SameLine();

    if (ImGui::SmallButton("No Mirror")) {
        m_engine.geoMirror = 0;
        m_engine.geoKaleid = 0;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove all mirror / kaleidoscope folds");

    // ── MIDI map hint ─────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("MIDI: map 'Geo mirror (0-3)' and 'Kaleidoscope segments'");
    ImGui::TextDisabled("      in the MIDI Mapper window to control live.");
}

void EquationEditor::drawVideoPanel() {
    // ── Browse button ─────────────────────────────────────────────────────────
    if (ImGui::Button("Browse...")) {
        std::string picked = pickVideoFile();
        if (!picked.empty()) {
            snprintf(m_videoPath, sizeof(m_videoPath), "%s", picked.c_str());
            m_videoIn.open(picked);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close video")) m_videoIn.close();

    // Manual path entry as fallback
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##videopath", m_videoPath, sizeof(m_videoPath),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (m_videoPath[0] != '\0') m_videoIn.open(m_videoPath);
    }
    ImGui::TextDisabled("(or type a path and press Enter)");

    ImGui::Separator();
    if (m_videoIn.isOpen())
        ImGui::TextColored({0.2f,1.0f,0.4f,1.0f}, "Playing: %dx%d  %s",
                           m_videoIn.width(), m_videoIn.height(),
                           m_videoIn.path().c_str());
    else
        ImGui::TextDisabled("No video loaded — click Browse to choose a file");
}

// Common service presets: { display label, RTMP base URL }
struct ServicePreset { const char* label; const char* rtmpBase; };
static const ServicePreset kPresets[] = {
    { "YouTube",   "rtmp://a.rtmp.youtube.com/live2/" },
    { "Twitch",    "rtmp://live.twitch.tv/app/" },
    { "Facebook",  "rtmps://live-api-s.facebook.com:443/rtmp/" },
    { "Kick",      "rtmps://fa723fc1b171.global-contribute.live-video.net/app/" },
    { "TikTok",    "rtmp://push.tiktok.com/live/" },
    { "Restream",  "rtmp://live.restream.io/live/" },
    { "Custom",    "" },
};
static const int kNumPresets = 7;

void EquationEditor::drawStreamPanel() {
    ImGui::SliderInt("Bitrate (kbps)", &m_bitrateKbps, 500, 8000);
    ImGui::SameLine();
    ImGui::TextDisabled("(Restream: ≤4500 for 1080p30)");
    ImGui::Combo("Resolution", &m_resIndex, kResLabels, 4);
    ImGui::Separator();

    // ── Destination list ──────────────────────────────────────────────────────
    ImGui::Text("Destinations (%d)", m_streamOut.destCount());
    ImGui::Spacing();

    int removeIdx = -1;
    for (int i = 0; i < m_streamOut.destCount(); i++) {
        DestSink& s = m_streamOut.dest(i);
        ImGui::PushID(i);

        // Enable toggle
        ImGui::Checkbox("##en", &s.enabled);
        ImGui::SameLine();

        // Live status dot
        if (m_streamOut.isStreaming() && s.connected)
            ImGui::TextColored({0.2f,1.0f,0.2f,1.0f}, "[LIVE]");
        else if (m_streamOut.isStreaming() && !s.connected)
            ImGui::TextColored({1.0f,0.4f,0.1f,1.0f}, "[ERR] ");
        else
            ImGui::TextDisabled("[    ]");
        ImGui::SameLine();

        // Editable URL (password field to hide stream key embedded in URL)
        char urlBuf[512];
        snprintf(urlBuf, sizeof(urlBuf), "%s", s.url.c_str());
        ImGui::SetNextItemWidth(180);
        if (ImGui::InputText("##url", urlBuf, sizeof(urlBuf),
                             ImGuiInputTextFlags_Password))
            s.url = urlBuf;
        ImGui::SameLine();
        ImGui::TextUnformatted(s.name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeIdx = i;

        ImGui::PopID();
    }
    if (removeIdx >= 0) m_streamOut.removeDestination(removeIdx);

    // ── Add destination ───────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Add destination:");

    // Preset buttons
    for (int p = 0; p < kNumPresets; p++) {
        if (p > 0) ImGui::SameLine();
        if (ImGui::SmallButton(kPresets[p].label)) {
            snprintf(m_newName, sizeof(m_newName), "%s", kPresets[p].label);
            snprintf(m_newUrl,  sizeof(m_newUrl),  "%s", kPresets[p].rtmpBase);
        }
    }
    ImGui::SetNextItemWidth(80);
    ImGui::InputText("Name##new", m_newName, sizeof(m_newName));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    ImGui::InputText("URL/Key##new", m_newUrl, sizeof(m_newUrl),
                     ImGuiInputTextFlags_Password);
    ImGui::SameLine();
    if (ImGui::Button("Add") && m_newName[0] && m_newUrl[0]) {
        m_streamOut.addDestination(m_newName, m_newUrl);
        m_newName[0] = '\0';
        m_newUrl[0]  = '\0';
    }

    // ── Start / Stop ──────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    if (!m_streamOut.isStreaming()) {
        bool hasAny = false;
        for (int i = 0; i < m_streamOut.destCount(); i++)
            if (m_streamOut.dest(i).enabled) { hasAny = true; break; }
        if (!hasAny) ImGui::BeginDisabled();
        if (ImGui::Button("Start Stream  ▶")) {
            m_streamOut.start(kResW[m_resIndex], kResH[m_resIndex],
                              m_bitrateKbps, 30);
        }
        if (!hasAny) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("(add a destination first)");
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f,0.1f,0.1f,1.0f));
        if (ImGui::Button("Stop Stream  ■")) m_streamOut.stop();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored({0.2f,1.0f,0.2f,1.0f}, "● LIVE to %d destination(s)",
                           m_streamOut.destCount());
    }
}

// ── Animation Panel ───────────────────────────────────────────────────────────
// All oscillations use static state so base values are preserved across frames.
void EquationEditor::drawAnimPanel() {
    double t = ImGui::GetTime();

    // ── Zoom oscillation ──────────────────────────────────────────────────────
    static bool  animZoom      = false;
    static float zoomBase      = 1.0f;
    static float zoomAmp       = 0.4f;
    static float zoomSpeed     = 0.4f;

    if (ImGui::Checkbox("Animate Zoom", &animZoom)) {
        if (animZoom) zoomBase = m_engine.zoom;   // capture current on enable
    }
    if (animZoom) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);  ImGui::DragFloat("##zbase", &zoomBase, 0.01f, 0.05f, 500.0f, "base %.2f");
        ImGui::SliderFloat("Zoom amp",   &zoomAmp,   0.0f, 2.0f);
        ImGui::SliderFloat("Zoom speed", &zoomSpeed, 0.01f, 4.0f);
        m_engine.zoom = zoomBase * (1.0f + zoomAmp * (float)sin(t * zoomSpeed));
    }

    ImGui::Separator();

    // ── Power oscillation ─────────────────────────────────────────────────────
    static bool  animPower     = false;
    static float powerBase     = 8.0f;
    static float powerAmp      = 2.0f;
    static float powerSpeed    = 0.25f;

    if (ImGui::Checkbox("Animate Power", &animPower)) {
        if (animPower) powerBase = m_engine.power;
    }
    if (animPower) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);  ImGui::DragFloat("##pbase", &powerBase, 0.1f, 2.0f, 16.0f, "base %.1f");
        ImGui::SliderFloat("Power amp",   &powerAmp,   0.0f, 6.0f);
        ImGui::SliderFloat("Power speed", &powerSpeed, 0.01f, 2.0f);
        m_engine.power = powerBase + powerAmp * (float)sin(t * powerSpeed);
        m_engine.power = m_engine.power < 2.0f ? 2.0f : m_engine.power;
    }

    ImGui::Separator();

    // ── Formula A↔B blend oscillation ────────────────────────────────────────
    static bool  animFBlend    = false;
    static float fBlendAmp     = 1.0f;
    static float fBlendSpeed   = 0.15f;

    ImGui::Checkbox("Animate Formula blend", &animFBlend);
    if (animFBlend) {
        ImGui::SliderFloat("Blend amp",   &fBlendAmp,   0.0f, 1.0f);
        ImGui::SliderFloat("Blend speed", &fBlendSpeed, 0.01f, 2.0f);
        m_engine.formulaBlend = 0.5f + fBlendAmp * 0.5f * (float)sin(t * fBlendSpeed);
    }

    ImGui::Separator();

    // ── Offset drift ─────────────────────────────────────────────────────────
    static bool  animDrift     = false;
    static float driftSpeed    = 0.02f;
    static float driftAngle    = 0.0f;   // radians
    static float driftOriginX  = 0.0f;
    static float driftOriginY  = 0.0f;

    if (ImGui::Checkbox("Animate Offset drift", &animDrift)) {
        if (animDrift) {
            driftOriginX = m_engine.offset.x;
            driftOriginY = m_engine.offset.y;
        }
    }
    if (animDrift) {
        ImGui::SliderFloat("Drift speed", &driftSpeed, 0.001f, 0.2f);
        ImGui::SliderFloat("Drift angle", &driftAngle, 0.0f, 6.28318f);
        float radius = (float)t * driftSpeed;
        m_engine.offset.x = driftOriginX + radius * (float)cos(driftAngle + t * driftSpeed * 0.3);
        m_engine.offset.y = driftOriginY + radius * (float)sin(driftAngle + t * driftSpeed * 0.3);
    }

    ImGui::Separator();

    // ── Pixel weight oscillation ──────────────────────────────────────────────
    static bool  animPixel     = false;
    static float pixelAmp      = 0.5f;
    static float pixelSpeed    = 0.3f;

    ImGui::Checkbox("Animate Pixel inject", &animPixel);
    if (animPixel) {
        ImGui::SliderFloat("Pixel amp",   &pixelAmp,   0.0f, 1.0f);
        ImGui::SliderFloat("Pixel speed", &pixelSpeed, 0.01f, 2.0f);
        m_engine.pixelWeight = pixelAmp * 0.5f * (1.0f + (float)sin(t * pixelSpeed));
    }
}

// ════════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════════
// UI2 — MIDI Mapper + Generator  (separate floating window)
// ════════════════════════════════════════════════════════════════════════════════
static const char* kNoteNames[] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};
static inline const char* midiNoteName(int n) {
    static char buf[8];
    snprintf(buf,sizeof(buf),"%s%d", kNoteNames[n%12], n/12-1);
    return buf;
}

void EquationEditor::drawMidiWindow() {
    ImGui::SetNextWindowPos ({800, 10},  ImGuiCond_Once);
    ImGui::SetNextWindowSize({460, 820}, ImGuiCond_Once);
    ImGui::Begin("MIDI Mapper — UI2");

    // ── Port selector ─────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("MIDI Routing", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Helper lambda to build a null-null ImGui combo buffer from a port list
        // (declared once, used for both in and out)
        auto buildPortBuf = [](auto& midiObj, char* buf, int bufsz) {
            buf[0] = '\0';
            int n = midiObj.portCount();
            for (int i = 0; i < n; i++) {
                auto nm = midiObj.portName(i);
                size_t pos = strlen(buf);
                if (pos + nm.size() + 2 < (size_t)bufsz) {
                    memcpy(buf+pos, nm.c_str(), nm.size()+1);
                    buf[pos+nm.size()+1] = '\0';
                }
            }
        };

        // ── MIDI INPUT ────────────────────────────────────────────────────────
        ImGui::TextDisabled("INPUT  (hardware controller → fractal params)");
        {
            static int selIn = 0;
            static char inBuf[1024];
            buildPortBuf(m_midiIn, inBuf, sizeof(inBuf));
            ImGui::SetNextItemWidth(230);
            ImGui::Combo("In port##midi", &selIn, inBuf);
            ImGui::SameLine();
            if (!m_midiIn.isOpen()) {
                if (ImGui::Button("Connect##in") && m_midiIn.portCount() > 0)
                    m_midiIn.open(selIn);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f,0.1f,0.1f,1));
                if (ImGui::Button("Disconnect##in")) m_midiIn.close();
                ImGui::PopStyleColor();
            }
            if (m_midiIn.isOpen()) {
                auto last = m_midiIn.lastMessage();
                int  type = (last.status & 0xF0), ch = (last.status & 0x0F)+1;
                const char* tn = type==0xB0?"CC":type==0x90?"NoteOn":
                                 type==0x80?"NoteOff":type==0xC0?"PC":"—";
                ImGui::TextColored({0.3f,1,0.3f,1}, "● IN: %s",
                                   m_midiIn.portName(m_midiIn.openedPort()).c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s ch%d #%d v%d", tn,ch,last.data1,last.data2);
            } else {
                ImGui::TextColored({0.5f,0.5f,0.5f,1},"○ IN not connected");
            }
        }

        ImGui::Spacing();

        // ── MIDI OUTPUT ───────────────────────────────────────────────────────
        ImGui::TextDisabled("OUTPUT  (generator notes → DAW / VST for recording)");
        ImGui::TextDisabled("Mac: IAC Driver Bus 1   Win: loopMIDI   Linux: ALSA virtual");
        {
            static int selOut = 0;
            static char outBuf[1024];
            buildPortBuf(m_midiOut, outBuf, sizeof(outBuf));
            ImGui::SetNextItemWidth(230);
            ImGui::Combo("Out port##midi", &selOut, outBuf);
            ImGui::SameLine();
            if (!m_midiOut.isOpen()) {
                if (ImGui::Button("Connect##out") && m_midiOut.portCount() > 0)
                    m_midiOut.open(selOut);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f,0.1f,0.1f,1));
                if (ImGui::Button("Disconnect##out")) m_midiOut.close();
                ImGui::PopStyleColor();
            }
            if (m_midiOut.isOpen()) {
                ImGui::TextColored({0.2f,0.8f,1,1}, "● OUT: %s",
                                   m_midiOut.portName(m_midiOut.openedPort()).c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("generator notes routing here");
            } else {
                ImGui::TextColored({0.5f,0.5f,0.5f,1},"○ OUT not connected  (generator notes will not reach DAW)");
            }

            if (m_midiOut.isOpen()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Panic##out")) m_midiOut.panic();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Send All-Notes-Off on all 16 channels");
            }
        }

        ImGui::Spacing();

        // ── MIDI THRU ─────────────────────────────────────────────────────────
        ImGui::Checkbox("MIDI Thru  (hardware input → output port)", &m_midiGen.midiThru);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Forwards incoming hardware MIDI straight to the output port\n"
                              "so a physical controller can also play the VST.");
    }

    ImGui::Separator();

    // ════════════════════════════════════════════════════════════════════════════
    // GENERATOR (ported from VST MIDI Randomizer HTML app)
    // ════════════════════════════════════════════════════════════════════════════
    if (ImGui::CollapsingHeader("Generator", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& G = m_midiGen;

        ImGui::Checkbox("Generator enabled", &G.enabled);
        if (!G.enabled) { ImGui::BeginDisabled(); }

        // ── Transport ──────────────────────────────────────────────────────────
        ImGui::Spacing();
        if (!G.playing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f,0.5f,0.2f,1));
            if (ImGui::Button("  ▶  Play  ")) {
                G.start(ImGui::GetTime());
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f,0.1f,0.1f,1));
            if (ImGui::Button("  ■  Stop  ")) {
                std::vector<MidiInput::Message> offs;
                G.stop(offs);
                for (auto& m : offs) m_midiMapper.apply(m, m_engine, m_blend, m_colorSynth);
            }
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (ImGui::Button("♪ One Note")) {
            auto msgs = G.fireOneNote();
            for (auto& m : msgs) {
                m_midiMapper.apply(m, m_engine, m_blend, m_colorSynth);
                m_midiOut.send(m);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("⚠ Panic")) {
            std::vector<MidiInput::Message> offs;
            G.stop(offs);
            for (auto& m : offs) {
                m_midiMapper.apply(m, m_engine, m_blend, m_colorSynth);
                m_midiOut.send(m);   // NoteOffs to real MIDI port
            }
            m_midiOut.panic();   // belt-and-suspenders: CC123 all channels
        }

        // ── BPM ───────────────────────────────────────────────────────────────
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("BPM", &G.bpm, 20.0f, 280.0f, "%.0f");
        ImGui::SameLine();
        if (G.playing) {
            ImGui::TextColored({1,0.7f,0.1f,1},"● PLAYING  step %d", G.liveStep);
        }

        // ── Live display (like the HTML "Live Output" card) ───────────────────
        ImGui::Separator();
        ImGui::TextDisabled("Live");
        ImGui::SameLine(60);
        if (G.liveNote >= 0) {
            ImGui::TextColored({1,0.7f,0.1f,1}, "Note %-4s (%3d)",
                               midiNoteName(G.liveNote), G.liveNote);
        } else {
            ImGui::TextDisabled("Note —");
        }
        ImGui::SameLine();
        ImGui::TextColored({0.7f,0.5f,1,1}, "Vel %3d", G.liveVel);
        ImGui::SameLine();
        if (G.liveProg >= 0)
            ImGui::TextColored({0.2f,0.9f,0.7f,1}, "PC %2d", G.liveProg);
        else
            ImGui::TextDisabled("PC —");

        ImGui::Separator();

        // ── Note & Scale ──────────────────────────────────────────────────────
        ImGui::TextDisabled("Note & Scale");

        ImGui::SetNextItemWidth(55); ImGui::InputInt("Min note##G", &G.noteMin);
        G.noteMin = std::max(0,  std::min(G.noteMin, G.noteMax-1));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(55); ImGui::InputInt("Max##G", &G.noteMax);
        G.noteMax = std::max(G.noteMin+1, std::min(127, G.noteMax));
        ImGui::SameLine();
        // Show note names
        ImGui::TextDisabled("%s – %s", midiNoteName(G.noteMin), midiNoteName(G.noteMax));

        // Root key
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("Key##G", genRootName(G.rootKey))) {
            for (int k = 0; k < 12; k++) {
                if (ImGui::Selectable(genRootName(k), G.rootKey==k))
                    G.rootKey = k;
                if (G.rootKey==k) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        // Scale mode
        ImGui::SetNextItemWidth(160);
        if (ImGui::BeginCombo("Scale##G", genScaleName(G.scale))) {
            for (int s = 0; s < (int)GenScale::COUNT; s++) {
                auto sm = static_cast<GenScale>(s);
                if (ImGui::Selectable(genScaleName(sm), G.scale==sm))
                    G.scale = sm;
                if (G.scale==sm) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Velocity range
        ImGui::SetNextItemWidth(180);
        ImGui::SliderInt("Vel min##G", &G.velMin, 1, G.velMax-1);
        ImGui::SetNextItemWidth(180);
        ImGui::SliderInt("Vel max##G", &G.velMax, G.velMin+1, 127);

        ImGui::Separator();

        // ── Timing ────────────────────────────────────────────────────────────
        ImGui::TextDisabled("Timing");
        static const char* kRateLabels[] =
            {"1/32","1/16","1/8","1/4","1/2","Whole","Random"};

        ImGui::SetNextItemWidth(100);
        ImGui::Combo("Step rate##G", &G.stepRateIdx, kRateLabels, 7);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("Note len##G",  &G.noteLenIdx,  kRateLabels, 7);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::SliderInt("Chord##G", &G.chordSize, 1, 6);

        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("Rests##G", &G.restProb, 0.0f, 0.75f, "%.0f%%",
                           ImGuiSliderFlags_None);
        // show as percent
        {
            char tmp[16]; snprintf(tmp,sizeof(tmp),"%.0f%%", G.restProb*100);
            ImGui::SameLine(); ImGui::TextDisabled("%s rests", tmp);
        }

        ImGui::Checkbox("Humanize##G", &G.humanize);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Adds ±7%% timing scatter to feel less robotic");

        ImGui::Separator();

        // ── Auto program change ───────────────────────────────────────────────
        ImGui::TextDisabled("Auto Program Change  →  maps to formula / any PC-mapped param");
        ImGui::Checkbox("Enable PC##G", &G.pgEnabled);
        if (G.pgEnabled) {
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Every N steps##G", &G.pgEvery);
            G.pgEvery = std::max(1, G.pgEvery);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60); ImGui::InputInt("PC min##G", &G.pgMin);
            G.pgMin = std::max(0,   std::min(G.pgMin, G.pgMax-1));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60); ImGui::InputInt("PC max##G", &G.pgMax);
            G.pgMax = std::max(G.pgMin+1, std::min(127, G.pgMax));
            ImGui::TextDisabled("PC %d–%d  (map PC→FormulaA in table below to drive formula changes)",
                                G.pgMin, G.pgMax);
        }

        if (!G.enabled) { ImGui::EndDisabled(); }
    }

    ImGui::Separator();

    // ════════════════════════════════════════════════════════════════════════════
    // MIDI LEARN + MAPPING TABLE
    // ════════════════════════════════════════════════════════════════════════════
    if (ImGui::CollapsingHeader("Mappings", ImGuiTreeNodeFlags_DefaultOpen)) {

        auto& learn = m_midiMapper.learn();
        static MidiMapping newMap = {0, 0, 0, MidiParam::FormulaBlend, 0.0f, 1.0f, ""};
        static int newParamIdx = (int)MidiParam::FormulaBlend;
        static const char* kMsgTypes[] = {"CC", "NoteOn", "NoteOff", "ProgramChange"};

        // ── MIDI Learn ────────────────────────────────────────────────────────
        if (!learn.active && !learn.captured) {
            if (ImGui::Button("  MIDI Learn  "))
                learn.active = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Click then wiggle a knob / press a key / change a program");
        } else if (learn.active) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,0,1));
            ImGui::Text("Waiting for MIDI…");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) learn.active = false;
        } else if (learn.captured) {
            int ct = (learn.captured_msg.status & 0xF0);
            newMap.msgType = (ct==0xB0)?0:(ct==0xC0)?3:1;
            newMap.channel = (learn.captured_msg.status & 0x0F)+1;
            newMap.number  = learn.captured_msg.data1;
            ImGui::TextColored({0.4f,1,0.4f,1},"\xe2\x9c\x94 %s ch%d #%d",
                               kMsgTypes[newMap.msgType], newMap.channel, newMap.number);
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) learn.captured = false;
        }

        // ── Add-mapping form ──────────────────────────────────────────────────
        ImGui::SetNextItemWidth(100); ImGui::Combo("Type##nm",     &newMap.msgType, kMsgTypes, 4);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(40);  ImGui::InputInt("Ch##nm",    &newMap.channel);
        newMap.channel = std::max(0, std::min(16, newMap.channel));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(40);  ImGui::InputInt("#/PC##nm",  &newMap.number);
        newMap.number  = std::max(0, std::min(127, newMap.number));
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("Param##nm", midiParamName(static_cast<MidiParam>(newParamIdx)))) {
            for (int i = 0; i < (int)MidiParam::COUNT; i++) {
                bool sel = (newParamIdx == i);
                if (ImGui::Selectable(midiParamName(static_cast<MidiParam>(i)), sel))
                    newParamIdx = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        newMap.param = static_cast<MidiParam>(newParamIdx);
        ImGui::SetNextItemWidth(70);  ImGui::DragFloat("Min##nm", &newMap.minVal, 0.01f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);  ImGui::DragFloat("Max##nm", &newMap.maxVal, 0.01f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);  ImGui::InputText("Label##nm", newMap.label, sizeof(newMap.label));
        ImGui::SameLine();
        if (ImGui::Button("Add##nm")) {
            if (!newMap.label[0])
                snprintf(newMap.label, sizeof(newMap.label), "%s#%d",
                         kMsgTypes[newMap.msgType], newMap.number);
            m_midiMapper.add(newMap);
            learn.captured = false;
            memset(newMap.label, 0, sizeof(newMap.label));
        }

        ImGui::Separator();

        // ── Mapping table ─────────────────────────────────────────────────────
        ImGui::TextDisabled("Active mappings (%d)", (int)m_midiMapper.mappings().size());
        if (ImGui::BeginTable("##maptbl", 7,
                ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|
                ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingFixedFit,
                ImVec2(0,200))) {
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Ch",    ImGuiTableColumnFlags_WidthFixed, 26);
            ImGui::TableSetupColumn("#",     ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableSetupColumn("Min",   ImGuiTableColumnFlags_WidthFixed, 48);
            ImGui::TableSetupColumn("Max",   ImGuiTableColumnFlags_WidthFixed, 48);
            ImGui::TableSetupColumn("×",     ImGuiTableColumnFlags_WidthFixed, 18);
            ImGui::TableHeadersRow();

            static const char* kMsgShort[] = {"CC","NoteOn","NoteOff","PC"};
            int removeIdx = -1;
            auto& maps = m_midiMapper.mappings();
            for (int i = 0; i < (int)maps.size(); i++) {
                auto& m = maps[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                char eid[16]; snprintf(eid,sizeof(eid),"##l%d",i);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText(eid, m.label, sizeof(m.label));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(kMsgShort[std::min(m.msgType,3)]);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", m.channel==0?"*":std::to_string(m.channel).c_str());
                ImGui::TableSetColumnIndex(3); ImGui::Text("%d", m.number);
                ImGui::TableSetColumnIndex(4);
                ImGui::SetNextItemWidth(44);
                char mid2[16]; snprintf(mid2,sizeof(mid2),"##mn%d",i);
                ImGui::DragFloat(mid2, &m.minVal, 0.01f);
                ImGui::TableSetColumnIndex(5);
                ImGui::SetNextItemWidth(44);
                char mid3[16]; snprintf(mid3,sizeof(mid3),"##mx%d",i);
                ImGui::DragFloat(mid3, &m.maxVal, 0.01f);
                ImGui::TableSetColumnIndex(6);
                ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,0.3f,0.3f,1));
                char did[16]; snprintf(did,sizeof(did),"×##d%d",i);
                if (ImGui::SmallButton(did)) removeIdx = i;
                ImGui::PopStyleColor();
            }
            if (removeIdx >= 0) m_midiMapper.remove(removeIdx);
            ImGui::EndTable();
        }

        // ── Quick presets ─────────────────────────────────────────────────────
        ImGui::Separator();
        ImGui::TextDisabled("Quick presets");
        if (ImGui::Button("CC1→Julia.x"))  m_midiMapper.add({0,0,1,MidiParam::JuliaCX,-1.5f,1.5f,"Mod→Julia.x"});
        ImGui::SameLine();
        if (ImGui::Button("CC2→Julia.y"))  m_midiMapper.add({0,0,2,MidiParam::JuliaCY,-1.5f,1.5f,"Mod→Julia.y"});
        ImGui::SameLine();
        if (ImGui::Button("CC7→Zoom"))     m_midiMapper.add({0,0,7,MidiParam::Zoom,0.2f,8.0f,"Vol→Zoom"});
        if (ImGui::Button("CC74→Power"))   m_midiMapper.add({0,0,74,MidiParam::Power,2.0f,12.0f,"Bright→Power"});
        ImGui::SameLine();
        if (ImGui::Button("CC71→FBlend"))  m_midiMapper.add({0,0,71,MidiParam::FormulaBlend,0.0f,1.0f,"Res→FBlend"});
        ImGui::SameLine();
        if (ImGui::Button("PC→FormulaA"))  m_midiMapper.add({3,0,0,MidiParam::FormulaA,0.0f,10.0f,"PC→FrmA"});
    }

    ImGui::End();
}

// ════════════════════════════════════════════════════════════════════════════════
// UI3 — Glitch Engine  (separate floating window)
// ════════════════════════════════════════════════════════════════════════════════
void EquationEditor::drawGlitchPanel() {
    ImGui::SetNextWindowPos ({1280, 10},  ImGuiCond_Once);
    ImGui::SetNextWindowSize({310, 440},  ImGuiCond_Once);
    ImGui::Begin("Glitch Engine — UI3");

    auto& G = m_glitch;

    // ── Master ────────────────────────────────────────────────────────────────
    ImGui::Checkbox("Enable Glitch Engine", &G.enabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Fires random chaos events that affect both\n"
                          "MIDI output and fractal visuals simultaneously.\n"
                          "Each glitch lasts a short time then auto-recovers.");

    if (!G.enabled) { ImGui::BeginDisabled(); }

    // ── Live status ───────────────────────────────────────────────────────────
    ImGui::Spacing();
    if (G.inGlitch) {
        ImGui::TextColored({1.0f, 0.3f, 0.1f, 1.0f}, "⚡ GLITCHING: %s", G.lastGlitchName);
    } else {
        ImGui::TextColored({0.4f, 0.4f, 0.4f, 1.0f}, "● Idle");
    }

    ImGui::Separator();

    // ── Event timing ─────────────────────────────────────────────────────────
    ImGui::TextDisabled("Event Rate");
    ImGui::SliderFloat("Rate (glitches/sec)", &G.glitchRateHz, 0.05f, 5.0f, "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Average number of glitch events per second.\n"
                          "0.1 = rare  0.5 = occasional  2.0 = frantic  5.0 = chaos");

    ImGui::SliderFloat("Min duration (s)", &G.glitchDurMin, 0.01f, 0.5f, "%.2f");
    ImGui::SliderFloat("Max duration (s)", &G.glitchDurMax,
                       G.glitchDurMin, 1.0f, "%.2f");

    // ── Intensity ─────────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Intensity");
    ImGui::SliderFloat("Intensity", &G.intensity, 0.0f, 1.0f);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Controls how extreme each glitch is.\n"
                          "0 = subtle nudges   1 = extreme chaos");

    ImGui::Separator();

    // ── Glitch type toggles ───────────────────────────────────────────────────
    ImGui::TextDisabled("Fractal Glitches");
    ImGui::Checkbox("Julia Jump",      &G.doJuliaJump);
    ImGui::SameLine(120);
    ImGui::Checkbox("Formula Flash",   &G.doFormulaFlash);
    ImGui::Checkbox("Zoom Punch",      &G.doZoomPunch);
    ImGui::SameLine(120);
    ImGui::Checkbox("Blend Scatter",   &G.doBlendScatter);
    ImGui::Checkbox("Power Spike",     &G.doPowerSpike);
    ImGui::SameLine(120);
    ImGui::Checkbox("Offset Shift",    &G.doOffsetShift);

    ImGui::Spacing();
    ImGui::TextDisabled("MIDI Glitches");
    ImGui::Checkbox("Vel Spike",       &G.doVelocitySpike);
    ImGui::SameLine(120);
    ImGui::Checkbox("Pitch Scramble",  &G.doPitchScramble);
    ImGui::Checkbox("Ghost Note",      &G.doGhostNote);

    // ── Ghost note range ──────────────────────────────────────────────────────
    if (G.doGhostNote) {
        ImGui::Spacing();
        ImGui::TextDisabled("Ghost note range");
        ImGui::SetNextItemWidth(60);
        ImGui::InputInt("Min##gn", &G.noteMin);
        G.noteMin = std::max(0, std::min(G.noteMin, G.noteMax - 1));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::InputInt("Max##gn", &G.noteMax);
        G.noteMax = std::max(G.noteMin + 1, std::min(127, G.noteMax));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("Ch##gn", &G.midiChannel);
        G.midiChannel = std::max(1, std::min(16, G.midiChannel));
    }

    ImGui::Separator();

    // ── Quick presets ─────────────────────────────────────────────────────────
    ImGui::TextDisabled("Presets");
    if (ImGui::SmallButton("Subtle")) {
        G.glitchRateHz = 0.2f; G.intensity = 0.3f;
        G.glitchDurMin = 0.05f; G.glitchDurMax = 0.15f;
        G.doJuliaJump = true;  G.doFormulaFlash = false;
        G.doZoomPunch = false; G.doBlendScatter = false;
        G.doPowerSpike= false; G.doOffsetShift  = false;
        G.doVelocitySpike = true; G.doPitchScramble = false; G.doGhostNote = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Moderate")) {
        G.glitchRateHz = 0.5f; G.intensity = 0.5f;
        G.glitchDurMin = 0.08f; G.glitchDurMax = 0.25f;
        G.doJuliaJump = true;  G.doFormulaFlash = true;
        G.doZoomPunch = false; G.doBlendScatter = true;
        G.doPowerSpike= false; G.doOffsetShift  = false;
        G.doVelocitySpike = true; G.doPitchScramble = true; G.doGhostNote = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Extreme")) {
        G.glitchRateHz = 2.0f; G.intensity = 0.9f;
        G.glitchDurMin = 0.05f; G.glitchDurMax = 0.4f;
        G.doJuliaJump = true;  G.doFormulaFlash = true;
        G.doZoomPunch = true;  G.doBlendScatter = true;
        G.doPowerSpike= true;  G.doOffsetShift  = true;
        G.doVelocitySpike = true; G.doPitchScramble = true; G.doGhostNote = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("MIDI only")) {
        G.doJuliaJump = false; G.doFormulaFlash = false;
        G.doZoomPunch = false; G.doBlendScatter = false;
        G.doPowerSpike= false; G.doOffsetShift  = false;
        G.doVelocitySpike = true; G.doPitchScramble = true; G.doGhostNote = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Visual only")) {
        G.doJuliaJump = true;  G.doFormulaFlash = true;
        G.doZoomPunch = true;  G.doBlendScatter = true;
        G.doPowerSpike= true;  G.doOffsetShift  = true;
        G.doVelocitySpike = false; G.doPitchScramble = false; G.doGhostNote = false;
    }

    if (!G.enabled) { ImGui::EndDisabled(); }

    ImGui::End();
}

// ════════════════════════════════════════════════════════════════════════════════
// COLOR SYNTHESIZER  — MIDI-reactive HSL / RGB palette engine
// ════════════════════════════════════════════════════════════════════════════════
void EquationEditor::drawColorSynthPanel() {
    auto& C = m_colorSynth;

    ImGui::Checkbox("Enable Color Synth", &C.enabled);
    if (!C.enabled) { ImGui::BeginDisabled(); }

    // ── Blend mode ────────────────────────────────────────────────────────────
    static const char* kBlendModes[] = {
        "Replace  (synth only)",
        "Multiply (tint palette)",
        "Screen   (lighten)"
    };
    ImGui::SetNextItemWidth(200);
    ImGui::Combo("Blend mode", &C.blendMode, kBlendModes, 3);

    ImGui::Separator();
    ImGui::TextDisabled("── Primary Color (HSL) ─────────────────");

    ImGui::SliderFloat("Hue",        &C.hueBase, 0.0f, 1.0f);
    ImGui::SliderFloat("Saturation", &C.satBase, 0.0f, 1.0f);
    ImGui::SliderFloat("Luminance",  &C.lumBase, 0.0f, 1.0f);

    // Live colour swatch
    {
        // Simple HSL→RGB approximation for the swatch
        auto hsl2rgb_ui = [](float h, float s, float l) -> ImVec4 {
            auto f = [&](float n) {
                float k = fmodf(n + h * 12.0f, 12.0f);
                float a = s * (l < 0.5f ? l : 1.0f - l);
                return l - a * fmaxf(-1.0f, fminf(fminf(k - 3.0f, 9.0f - k), 1.0f));
            };
            return {f(0), f(8), f(4), 1.0f};
        };
        auto col = hsl2rgb_ui(C.hueBase, C.satBase, C.lumBase);
        ImGui::SameLine();
        ImGui::ColorButton("##primary_swatch", col,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {24, 24});
    }

    ImGui::Separator();
    ImGui::TextDisabled("── Alternate Color (HSL) ───────────────");

    ImGui::SliderFloat("Alt Hue",   &C.hueAlt, 0.0f, 1.0f);
    ImGui::SliderFloat("Alt Sat",   &C.satAlt, 0.0f, 1.0f);
    ImGui::SliderFloat("Alt Lum",   &C.lumAlt, 0.0f, 1.0f);
    ImGui::SliderFloat("Alt rate (Hz)",  &C.altRate,  0.01f, 8.0f, "%.2f Hz");

    {
        auto hsl2rgb_ui = [](float h, float s, float l) -> ImVec4 {
            auto f = [&](float n) {
                float k = fmodf(n + h * 12.0f, 12.0f);
                float a = s * (l < 0.5f ? l : 1.0f - l);
                return l - a * fmaxf(-1.0f, fminf(fminf(k - 3.0f, 9.0f - k), 1.0f));
            };
            return {f(0), f(8), f(4), 1.0f};
        };
        auto col = hsl2rgb_ui(C.hueAlt, C.satAlt, C.lumAlt);
        ImGui::SameLine();
        ImGui::ColorButton("##alt_swatch", col,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {24, 24});
    }

    ImGui::Separator();
    ImGui::TextDisabled("── Oscillators ─────────────────────────");

    ImGui::SliderFloat("Hue osc amp",   &C.hueOscAmp,  0.0f, 0.5f);
    ImGui::SliderFloat("Hue osc rate",  &C.hueOscRate, 0.01f, 4.0f, "%.2f Hz");
    ImGui::SliderFloat("Lum osc amp",   &C.lumOscAmp,  0.0f, 0.5f);
    ImGui::SliderFloat("Lum osc rate",  &C.lumOscRate, 0.01f, 4.0f, "%.2f Hz");

    ImGui::Separator();
    ImGui::TextDisabled("── Escape-value Spread ─────────────────");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("How much hue and lum shift across the fractal\n"
                          "detail range (0 = flat colour block).");

    ImGui::SliderFloat("Hue spread",  &C.hueSpread, 0.0f, 1.0f);
    ImGui::SliderFloat("Lum spread",  &C.lumSpread, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::TextDisabled("── MIDI Note Reaction ──────────────────");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("How strongly incoming note-on velocity\n"
                          "flashes hue, saturation, and luminance.");

    ImGui::SliderFloat("Hue sens",   &C.midiHueSens, 0.0f, 1.0f);
    ImGui::SliderFloat("Sat sens",   &C.midiSatSens, 0.0f, 1.0f);
    ImGui::SliderFloat("Lum sens",   &C.midiLumSens, 0.0f, 1.0f);
    ImGui::SliderFloat("Decay (s)",  &C.midiDecay,   0.2f, 8.0f, "%.2f s");

    ImGui::Separator();
    ImGui::TextDisabled("── Quick Presets ───────────────────────");

    // Fire preset
    if (ImGui::SmallButton("Fire")) {
        C.enabled = true; C.blendMode = 1;
        C.hueBase = 0.05f; C.satBase = 1.0f; C.lumBase = 0.5f;
        C.hueAlt  = 0.0f;  C.satAlt  = 1.0f; C.lumAlt  = 0.3f;
        C.altRate = 1.5f;  C.hueOscAmp = 0.04f; C.hueOscRate = 0.8f;
        C.lumOscAmp = 0.2f; C.lumOscRate = 1.2f;
        C.hueSpread = 0.1f; C.lumSpread = 0.5f;
        C.midiLumSens = 0.6f; C.midiHueSens = 0.05f;
    }
    ImGui::SameLine();
    // Ocean preset
    if (ImGui::SmallButton("Ocean")) {
        C.enabled = true; C.blendMode = 1;
        C.hueBase = 0.58f; C.satBase = 0.9f; C.lumBase = 0.45f;
        C.hueAlt  = 0.52f; C.satAlt  = 0.7f; C.lumAlt  = 0.6f;
        C.altRate = 0.3f;  C.hueOscAmp = 0.06f; C.hueOscRate = 0.15f;
        C.lumOscAmp = 0.1f; C.lumOscRate = 0.4f;
        C.hueSpread = 0.08f; C.lumSpread = 0.3f;
        C.midiLumSens = 0.4f; C.midiHueSens = 0.1f;
    }
    ImGui::SameLine();
    // Psychedelic preset
    if (ImGui::SmallButton("Psychedelic")) {
        C.enabled = true; C.blendMode = 2;
        C.hueBase = 0.0f; C.satBase = 1.0f; C.lumBase = 0.5f;
        C.hueAlt  = 0.5f; C.satAlt  = 1.0f; C.lumAlt  = 0.5f;
        C.altRate = 3.0f;  C.hueOscAmp = 0.2f; C.hueOscRate = 1.0f;
        C.lumOscAmp = 0.2f; C.lumOscRate = 2.0f;
        C.hueSpread = 0.5f; C.lumSpread = 0.5f;
        C.midiHueSens = 0.5f; C.midiSatSens = 0.3f; C.midiLumSens = 0.6f;
    }
    // Neon preset
    if (ImGui::SmallButton("Neon")) {
        C.enabled = true; C.blendMode = 2;
        C.hueBase = 0.83f; C.satBase = 1.0f; C.lumBase = 0.6f;
        C.hueAlt  = 0.17f; C.satAlt  = 1.0f; C.lumAlt  = 0.6f;
        C.altRate = 2.0f;  C.hueOscAmp = 0.05f; C.hueOscRate = 3.0f;
        C.lumOscAmp = 0.15f; C.lumOscRate = 3.0f;
        C.hueSpread = 0.3f; C.lumSpread = 0.2f;
        C.midiHueSens = 0.3f; C.midiLumSens = 0.8f; C.midiDecay = 0.6f;
    }
    ImGui::SameLine();
    // Monochrome preset
    if (ImGui::SmallButton("Mono")) {
        C.enabled = true; C.blendMode = 0;
        C.hueBase = 0.0f; C.satBase = 0.0f; C.lumBase = 0.5f;
        C.hueAlt  = 0.0f; C.satAlt  = 0.0f; C.lumAlt  = 0.8f;
        C.altRate = 0.5f;  C.hueOscAmp = 0.0f;
        C.lumOscAmp = 0.3f; C.lumOscRate = 0.8f;
        C.hueSpread = 0.0f; C.lumSpread = 0.6f;
        C.midiLumSens = 0.7f; C.midiDecay = 0.8f;
    }
    ImGui::SameLine();
    // Sunrise preset
    if (ImGui::SmallButton("Sunrise")) {
        C.enabled = true; C.blendMode = 1;
        C.hueBase = 0.08f; C.satBase = 0.95f; C.lumBase = 0.55f;
        C.hueAlt  = 0.72f; C.satAlt  = 0.8f;  C.lumAlt  = 0.35f;
        C.altRate = 0.15f; C.hueOscAmp = 0.03f; C.hueOscRate = 0.1f;
        C.lumOscAmp = 0.08f; C.lumOscRate = 0.2f;
        C.hueSpread = 0.2f; C.lumSpread = 0.4f;
        C.midiHueSens = 0.15f; C.midiLumSens = 0.5f; C.midiDecay = 2.5f;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Off")) {
        C.enabled = false;
    }

    // ── Live indicator ────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Live output:");
    ImGui::SameLine();
    {
        auto hsl2rgb_ui = [](float h, float s, float l) -> ImVec4 {
            auto f = [&](float n) {
                float k = fmodf(n + h * 12.0f, 12.0f);
                float a = s * (l < 0.5f ? l : 1.0f - l);
                return l - a * fmaxf(-1.0f, fminf(fminf(k - 3.0f, 9.0f - k), 1.0f));
            };
            return {f(0), f(8), f(4), 1.0f};
        };
        auto col = hsl2rgb_ui(C.outHSL[0], C.outHSL[1], C.outHSL[2]);
        ImGui::ColorButton("##live_primary", col,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {18, 18});
        ImGui::SameLine();
        auto colA = hsl2rgb_ui(C.outHSLAlt[0], C.outHSLAlt[1], C.outHSLAlt[2]);
        ImGui::ColorButton("##live_alt", colA,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {18, 18});
        ImGui::SameLine();
        ImGui::Text("blend %.2f", C.outAltBlend);
    }

    if (!C.enabled) { ImGui::EndDisabled(); }
}

// ── Distortion / Metaballs ────────────────────────────────────────────────────
//
// Controls for the iridescent metaball shader (distortion.frag).
// Enabling Distortion Mode bypasses the fractal pipeline and renders the
// animated blob field instead.  All other panels remain active so the stream
// settings, MIDI, and glitch effects still apply.

void EquationEditor::drawDistortionPanel() {
    auto& E = m_engine;

    ImGui::Checkbox("Enable Distortion Mode", &E.distortionMode);
    if (E.distortionMode) {
        ImGui::Separator();
        ImGui::TextDisabled("Fractal blend is paused while Distortion Mode is on.");
        ImGui::Spacing();
    }

    bool dis = !E.distortionMode;
    if (dis) ImGui::BeginDisabled();

    ImGui::SliderFloat("Speed",       &E.distortSpeed,   0.1f, 3.0f,  "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Animation rate multiplier");

    int blobs = E.distortBlobs;
    if (ImGui::SliderInt("Blobs", &blobs, 3, 10))
        E.distortBlobs = blobs;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Number of metaballs");

    ImGui::SliderFloat("Glow",        &E.distortGlow,    0.0f, 2.0f,  "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Outer aura intensity");

    ImGui::SliderFloat("Iridescence", &E.distortIrid,    0.5f, 4.0f,  "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Rainbow cycle frequency");

    ImGui::SliderFloat("Outline",     &E.distortOutline, 0.0f, 1.0f,  "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Chromatic edge ring brightness");

    if (dis) ImGui::EndDisabled();
}

// ── Chaos Effects ─────────────────────────────────────────────────────────────
//
// Pre-iteration domain warps inspired by chaos theory.  Applied to the complex
// plane before fractal iteration so the chaotic geometry is baked into the
// fractal structure itself.
//
//  Off         — straight fractal rendering (no warp)
//  Turbulence  — two-level fBm noise warp; smooth, continuously-folding flow
//  Logistic    — logistic map r·x·(1-x) iterated in polar coords; at r→4 the
//                orbit enters full chaos and drives a rotation warp
//  Hénon       — Hénon strange attractor (a=1.4, b=0.3) displacement
//  Shred       — multi-frequency scanline horizontal drift; tape-degradation look

void EquationEditor::drawChaosPanel() {
    auto& E = m_engine;

    static const char* kModeLabels[] = {
        "Off", "Turbulence", "Logistic", "Henon", "Shred"
    };
    ImGui::Combo("Mode", &E.chaosMode, kModeLabels, 5);

    if (E.chaosMode == 0) {
        ImGui::TextDisabled("Select a mode to enable chaos domain warp.");
        return;
    }

    // Mode descriptions
    ImGui::Spacing();
    switch (E.chaosMode) {
    case 1: ImGui::TextDisabled("fBm turbulence — smooth chaotic flow fields"); break;
    case 2: ImGui::TextDisabled("Logistic map — period-doubling bifurcation into chaos"); break;
    case 3: ImGui::TextDisabled("Henon attractor — strange attractor displacement"); break;
    case 4: ImGui::TextDisabled("Shred — scanline drift / signal-loss distortion"); break;
    }
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SliderFloat("Strength", &E.chaosStrength, 0.0f, 1.0f,  "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Overall warp amplitude");

    ImGui::SliderFloat("Scale",    &E.chaosScale,    0.5f, 8.0f,  "%.2f");
    if (ImGui::IsItemHovered()) {
        switch (E.chaosMode) {
        case 1: ImGui::SetTooltip("Noise spatial frequency"); break;
        case 2: ImGui::SetTooltip("Polar coordinate scale for logistic seed"); break;
        case 3: ImGui::SetTooltip("Henon map input scale"); break;
        case 4: ImGui::SetTooltip("Scanline density"); break;
        }
    }

    ImGui::SliderFloat("Speed",    &E.chaosSpeed,    0.0f, 3.0f,  "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Time modulation rate");

    // Live indicator: show whether we're in a visually chaotic regime
    if (E.chaosMode == 2) {
        float r = 3.57f + E.chaosStrength * 0.43f;
        bool chaotic = (r > 3.57f);
        ImGui::Spacing();
        ImGui::TextDisabled("Logistic r = %.3f  (%s)", r,
                            chaotic ? "chaotic regime" : "periodic (increase Strength)");
    }
}
