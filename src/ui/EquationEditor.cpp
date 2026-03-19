#include "EquationEditor.h"
#include "AppSettings.h"
#include "midi/MidiOutput.h"
#include "FilePicker.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <ctime>
#include <sys/stat.h>

static const char* kResLabels[] = {"1280x720", "1920x1080", "2560x1440", "3840x2160 (4K)"};
static const int   kResW[]      = {1280, 1920, 2560, 3840};
static const int   kResH[]      = { 720, 1080, 1440, 2160};
static const char* kShapeLabels[] = {"Circle", "Polygon", "Star", "Grid"};

EquationEditor::EquationEditor(FractalEngine& engine, BlendController& blend,
                                GlitchEngine& glitch, ColorSynth& colorSynth,
                                VideoInput& videoIn, VideoInput& overlayIn,
                                StreamOutput& streamOut,
                                MidiInput& midiIn, MidiOutput& midiOut,
                                MidiMapper& midiMapper, MidiGenerator& midiGen)
    : m_engine(engine), m_blend(blend), m_glitch(glitch), m_colorSynth(colorSynth),
      m_videoIn(videoIn), m_overlayIn(overlayIn), m_streamOut(streamOut),
      m_midiIn(midiIn), m_midiOut(midiOut),
      m_midiMapper(midiMapper), m_midiGen(midiGen) {}

void EquationEditor::draw() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({360, 980}, ImGuiCond_Once);
    ImGui::Begin("Fractal Stream Controls");

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen))
        drawPresetsPanel();
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

    // ── Overlay video layer ───────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextUnformatted("Overlay Video Layer");
    ImGui::SliderFloat("Blend##overlay", &m_engine.overlayBlend, 0.0f, 1.0f,
                       "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 = fractal only   0.5 = 50/50   1 = overlay only\n"
                          "Audio is mixed at the same ratio when streaming.");
    if (ImGui::Button("Browse##ovr")) {
        std::string picked = pickVideoFile();
        if (!picked.empty()) {
            snprintf(m_overlayPath, sizeof(m_overlayPath), "%s", picked.c_str());
            m_overlayIn.open(picked);
            m_streamOut.overlayAudioPath = picked;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close##ovr")) {
        m_overlayIn.close();
        m_streamOut.overlayAudioPath = "";
        m_overlayPath[0] = '\0';
    }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##overlaypath", m_overlayPath, sizeof(m_overlayPath),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (m_overlayPath[0] != '\0') {
            m_overlayIn.open(m_overlayPath);
            m_streamOut.overlayAudioPath = m_overlayPath;
        }
    }
    if (m_overlayIn.isOpen())
        ImGui::TextColored({0.4f,0.85f,1.0f,1.0f}, "Overlay: %dx%d  %s",
                           m_overlayIn.width(), m_overlayIn.height(),
                           m_overlayIn.path().c_str());
    else
        ImGui::TextDisabled("No overlay loaded — click Browse to choose a file");
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

// Returns the RTMP base prefix for a known service, nullptr for Custom/unknown.
static const char* matchServiceBase(const std::string& url) {
    for (int i = 0; i < kNumPresets - 1; ++i) { // skip "Custom" (last)
        const char* base = kPresets[i].rtmpBase;
        if (base[0] && url.rfind(base, 0) == 0) return base;
    }
    return nullptr;
}

// True if url is a local filesystem path (no ://)
static bool isLocalPath(const std::string& url) {
    return url.find("://") == std::string::npos;
}

// True if this destination is the permanent Restream entry.
static bool isRestreamDest(const DestSink& s) {
    return s.name == "Restream" ||
           s.url.rfind("rtmp://live.restream.io/live/", 0) == 0;
}

void EquationEditor::drawStreamPanel() {
    // ── Bitrate / Resolution ──────────────────────────────────────────────────
    ImGui::SliderInt("Bitrate (kbps)", &m_bitrateKbps, 500, 16000);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Restream: up to 4500 for 1080p30, 8000 for 1080p60");
    ImGui::Combo("Resolution", &m_resIndex, kResLabels, 4);
    ImGui::Separator();

    // ── Restream pinned entry (always at top, always present) ─────────────────
    // Find it, create it if missing.
    int restreamIdx = -1;
    for (int i = 0; i < m_streamOut.destCount(); i++)
        if (isRestreamDest(m_streamOut.dest(i))) { restreamIdx = i; break; }
    if (restreamIdx < 0) {
        m_streamOut.addDestination("Restream", "rtmp://live.restream.io/live/");
        restreamIdx = m_streamOut.destCount() - 1;
    }
    {
        DestSink& rs = m_streamOut.dest(restreamIdx);
        ImGui::PushID("restream_pin");

        // Status indicator
        if (m_streamOut.isStreaming() && rs.connected)
            ImGui::TextColored({0.2f,1.0f,0.2f,1.0f}, "● LIVE");
        else if (m_streamOut.isStreaming() && !rs.connected)
            ImGui::TextColored({1.0f,0.4f,0.1f,1.0f}, "● ERR ");
        else
            ImGui::TextDisabled("○     ");
        ImGui::SameLine();
        ImGui::Checkbox("##en", &rs.enabled);
        ImGui::SameLine();
        ImGui::TextColored({0.4f,0.85f,1.0f,1.0f}, "Restream");

        // Stream key on its own line — full width, plain text so it's easy to paste
        const char* base = "rtmp://live.restream.io/live/";
        const size_t baseLen = strlen(base);
        // Extract key: skip past any accidentally doubled base prefixes in the
        // stored URL (happens when the user pastes the full URL instead of
        // just the key — the field then shows and saves the correct key only).
        const char* keyStart = rs.url.c_str();
        while (strncmp(keyStart, base, baseLen) == 0) keyStart += baseLen;
        // Persist the normalised URL immediately so the bad doubled value is gone
        if (rs.url != std::string(base) + keyStart) {
            rs.url = std::string(base) + keyStart;
            saveSettings(AppSettings::lastPath());
        }
        char keyBuf[256] = {};
        strncpy(keyBuf, keyStart, sizeof(keyBuf) - 1);
        ImGui::SetNextItemWidth(-28);
        if (ImGui::InputText("##rskey", keyBuf, sizeof(keyBuf))) {
            // Strip any base prefix the user may have accidentally pasted
            const char* k = keyBuf;
            while (strncmp(k, base, baseLen) == 0) k += baseLen;
            rs.url = std::string(base) + k;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            saveSettings(AppSettings::lastPath());   // persist key immediately
        if (ImGui::IsItemHovered() || rs.url == base || rs.url.empty())
            ImGui::SetTooltip("Paste your Restream stream key here\n"
                              "(Dashboard → Stream Setup → Stream Key)");
        ImGui::SameLine();
        if (ImGui::SmallButton("×##rsclr")) {
            rs.url = base;   // wipe back to bare base URL — key is gone
            saveSettings(AppSettings::lastPath());   // persist the clear too
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Clear stream key and reset URL");
        if (rs.url == base || rs.url.empty())
            ImGui::TextDisabled("  ^ paste your Restream stream key above");

        ImGui::PopID();
    }

    // ── Other destinations ────────────────────────────────────────────────────
    int removeIdx = -1;
    bool hasOthers = false;
    for (int i = 0; i < m_streamOut.destCount(); i++)
        if (!isRestreamDest(m_streamOut.dest(i))) { hasOthers = true; break; }

    if (hasOthers) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Additional destinations");
        ImGui::Spacing();
    }

    for (int i = 0; i < m_streamOut.destCount(); i++) {
        DestSink& s = m_streamOut.dest(i);
        if (isRestreamDest(s)) continue; // already drawn above

        ImGui::PushID(i);
        ImGui::Checkbox("##en", &s.enabled);
        ImGui::SameLine();

        if (m_streamOut.isStreaming() && s.connected)
            ImGui::TextColored({0.2f,1.0f,0.2f,1.0f}, "[LIVE]");
        else if (m_streamOut.isStreaming() && !s.connected)
            ImGui::TextColored({1.0f,0.4f,0.1f,1.0f}, "[ERR] ");
        else
            ImGui::TextDisabled("[    ]");
        ImGui::SameLine();

        // Name label (fixed width)
        ImGui::Text("%-10s", s.name.c_str());
        ImGui::SameLine();

        // Key or full URL input
        const char* base = matchServiceBase(s.url);
        char keyBuf[512] = {};
        if (isLocalPath(s.url)) {
            // Local recording path — show plainly, no masking
            strncpy(keyBuf, s.url.c_str(), sizeof(keyBuf) - 1);
            ImGui::SetNextItemWidth(-30);
            if (ImGui::InputText("##lpath", keyBuf, sizeof(keyBuf)))
                s.url = keyBuf;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Local recording path (.mp4 / .mkv)");
        } else if (base) {
            // Known service — show only the stream key (plain text)
            strncpy(keyBuf, s.url.c_str() + strlen(base), sizeof(keyBuf) - 1);
            ImGui::SetNextItemWidth(-30);
            if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf)))
                s.url = std::string(base) + keyBuf;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Stream key — prepended with:\n%s", base);
        } else {
            // Custom — show full URL, password-masked
            strncpy(keyBuf, s.url.c_str(), sizeof(keyBuf) - 1);
            ImGui::SetNextItemWidth(-30);
            if (ImGui::InputText("##url", keyBuf, sizeof(keyBuf),
                                 ImGuiInputTextFlags_Password))
                s.url = keyBuf;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Full RTMP URL (masked)");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeIdx = i;

        ImGui::PopID();
    }
    if (removeIdx >= 0) m_streamOut.removeDestination(removeIdx);

    // ── Add destination ───────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Add another destination:");

    // Service quick-pick buttons (skip Restream — it's always pinned above)
    for (int p = 0; p < kNumPresets; p++) {
        if (strcmp(kPresets[p].label, "Restream") == 0) continue;
        if (p > 0) ImGui::SameLine();
        if (ImGui::SmallButton(kPresets[p].label)) {
            snprintf(m_newName, sizeof(m_newName), "%s", kPresets[p].label);
            snprintf(m_newUrl,  sizeof(m_newUrl),  "%s", kPresets[p].rtmpBase);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Local file")) {
        // Default to Seagate drive → fractal stream/part 1/
        // Falls back to home dir if the drive isn't mounted.
        const char* seagate = "/Volumes/Seagate/fractal stream/part 1";
        const char* home    = getenv("HOME");
        if (!home || !home[0]) home = ".";
        // Pick the Seagate path if the drive is mounted, else home
        struct stat st{};
        const char* dir = (stat("/Volumes/Seagate", &st) == 0) ? seagate : home;
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        snprintf(m_newName, sizeof(m_newName), "Recording");
        snprintf(m_newUrl, sizeof(m_newUrl),
                 "%s/fractal_%04d%02d%02d_%02d%02d%02d.mp4",
                 dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Record locally to an MP4 file.\n"
                          "Saves to: /Volumes/Seagate/fractal stream/part 1/\n"
                          "(falls back to home dir if drive not mounted)\n"
                          "Filename is auto-stamped. Starts/stops with stream.");

    ImGui::SetNextItemWidth(70);
    ImGui::InputText("Name##new", m_newName, sizeof(m_newName));
    ImGui::SameLine();

    // For the add row, detect if a known base has been pre-filled
    {
        const char* addBase = matchServiceBase(std::string(m_newUrl));
        if (addBase) {
            // Show only the key part
            char keyBuf[256] = {};
            strncpy(keyBuf, m_newUrl + strlen(addBase), sizeof(keyBuf) - 1);
            ImGui::SetNextItemWidth(-50);
            if (ImGui::InputText("Key##new", keyBuf, sizeof(keyBuf)))
                snprintf(m_newUrl, sizeof(m_newUrl), "%s%s", addBase, keyBuf);
        } else {
            ImGui::SetNextItemWidth(-50);
            ImGui::InputText("URL##new", m_newUrl, sizeof(m_newUrl));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add") && m_newName[0] && m_newUrl[0]) {
        m_streamOut.addDestination(m_newName, m_newUrl);
        m_newName[0] = '\0';
        m_newUrl[0]  = '\0';
    }

    // ── Audio capture device ──────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Audio source (macOS loopback)");
    {
        char audioBuf[256] = {};
        strncpy(audioBuf, m_streamOut.audioDevice.c_str(), sizeof(audioBuf) - 1);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##audiodev", audioBuf, sizeof(audioBuf)))
            m_streamOut.audioDevice = audioBuf;
        if (ImGui::IsItemDeactivatedAfterEdit())
            saveSettings(AppSettings::lastPath());
        if (ImGui::IsItemHovered() || m_streamOut.audioDevice.empty())
            ImGui::SetTooltip("Leave blank for silent audio track.\n"
                              "Set to a loopback device name to capture system audio,\n"
                              "e.g. \"BlackHole 2ch\" (must be installed separately).");
    }

    // ── Start / Stop ──────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();

    // Check if Restream has a key set
    const DestSink& rs = m_streamOut.dest(restreamIdx);
    const char* rsBase = "rtmp://live.restream.io/live/";
    bool rsKeySet = rs.url.size() > strlen(rsBase);

    if (!m_streamOut.isStreaming()) {
        if (!rsKeySet) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f,0.3f,0.1f,1.0f));
            if (ImGui::Button("Start Stream  ▶"))
                m_streamOut.start(kResW[m_resIndex], kResH[m_resIndex],
                                  m_bitrateKbps, 30);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored({1.0f,0.8f,0.2f,1.0f}, "⚠ No Restream key set");
        } else {
            if (ImGui::Button("Start Stream  ▶"))
                m_streamOut.start(kResW[m_resIndex], kResH[m_resIndex],
                                  m_bitrateKbps, 30);
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
        if (G.liveProg >= 128)
            ImGui::TextColored({0.2f,0.9f,0.7f,1}, "PC bank%d p%d",
                               G.liveProg/128, G.liveProg%128);
        else if (G.liveProg >= 0)
            ImGui::TextColored({0.2f,0.9f,0.7f,1}, "PC %d", G.liveProg);
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
        ImGui::TextDisabled("Auto Program Change");
        ImGui::SameLine();
        ImGui::TextDisabled("— Surge XT / any synth  (values >127 send Bank Select CC0 + PC)");
        ImGui::Checkbox("Enable PC##G", &G.pgEnabled);
        if (G.pgEnabled) {
            // Every N steps — full available width
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("Every N steps##G", &G.pgEvery, 1, 64);
            G.pgEvery = std::max(1, G.pgEvery);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("How many steps between each patch change");

            // Patch min/max — full width, no 127 cap
            // Ctrl+click on any slider to type an exact number
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("Patch min##G", &G.pgMin, 0, 16383);
            G.pgMin = std::max(0, std::min(G.pgMin, G.pgMax - 1));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+click to type exact value\n"
                                  "Surge XT: bank×128 + patch_index");

            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("Patch max##G", &G.pgMax, 1, 16383);
            G.pgMax = std::max(G.pgMin + 1, G.pgMax);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+click to type exact value");

            // Live display of current bank+patch when > 127
            if (G.pgMin > 127 || G.pgMax > 127) {
                ImGui::TextColored({0.4f,0.9f,0.6f,1},
                    "Bank %d patch %d  →  Bank %d patch %d",
                    G.pgMin/128, G.pgMin%128, G.pgMax/128, G.pgMax%128);
            } else {
                ImGui::TextDisabled("Range: %d – %d  (map PC→FormulaA to drive formula changes)",
                                    G.pgMin, G.pgMax);
            }

            // Quick range presets for Surge XT
            ImGui::Spacing();
            ImGui::TextDisabled("Quick range:");
            if (ImGui::SmallButton("Formula (0-10)"))    { G.pgMin=0;   G.pgMax=10;   }
            ImGui::SameLine();
            if (ImGui::SmallButton("PC 0-127"))          { G.pgMin=0;   G.pgMax=127;  }
            ImGui::SameLine();
            if (ImGui::SmallButton("Surge Bank 0"))      { G.pgMin=0;   G.pgMax=127;  }
            ImGui::SameLine();
            if (ImGui::SmallButton("Surge Bank 1"))      { G.pgMin=128; G.pgMax=255;  }
            ImGui::SameLine();
            if (ImGui::SmallButton("Surge Bank 2"))      { G.pgMin=256; G.pgMax=383;  }

            if (ImGui::SmallButton("Banks 0-3"))         { G.pgMin=0;   G.pgMax=511;  }
            ImGui::SameLine();
            if (ImGui::SmallButton("Banks 0-7"))         { G.pgMin=0;   G.pgMax=1023; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Banks 0-15"))        { G.pgMin=0;   G.pgMax=2047; }
        }

        if (!G.enabled) { ImGui::EndDisabled(); }
    }

    ImGui::Separator();

    // ════════════════════════════════════════════════════════════════════════════
    // SURGE XT
    // ════════════════════════════════════════════════════════════════════════════
    drawSurgeXTSection();

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

    ImGui::SliderFloat("Min duration (s)", &G.glitchDurMin, 0.1f, 5.0f, "%.2f");
    if (G.glitchDurMin > G.glitchDurMax) G.glitchDurMax = G.glitchDurMin;
    ImGui::SliderFloat("Max duration (s)", &G.glitchDurMax,
                       G.glitchDurMin, 5.0f, "%.2f");

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
        G.glitchDurMin = 1.0f; G.glitchDurMax = 2.0f;
        G.doJuliaJump = true;  G.doFormulaFlash = false;
        G.doZoomPunch = false; G.doBlendScatter = false;
        G.doPowerSpike= false; G.doOffsetShift  = false;
        G.doVelocitySpike = true; G.doPitchScramble = false; G.doGhostNote = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Moderate")) {
        G.glitchRateHz = 0.5f; G.intensity = 0.5f;
        G.glitchDurMin = 1.5f; G.glitchDurMax = 3.5f;
        G.doJuliaJump = true;  G.doFormulaFlash = true;
        G.doZoomPunch = false; G.doBlendScatter = true;
        G.doPowerSpike= false; G.doOffsetShift  = false;
        G.doVelocitySpike = true; G.doPitchScramble = true; G.doGhostNote = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Extreme")) {
        G.glitchRateHz = 2.0f; G.intensity = 0.9f;
        G.glitchDurMin = 2.0f; G.glitchDurMax = 5.0f;
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

// ── INI helpers (file-local) ──────────────────────────────────────────────────

using IniMap = std::unordered_map<std::string, std::string>;

static IniMap parseIni(const std::string& path) {
    IniMap m;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return m;
    char line[1024];
    std::string section;
    while (fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) { *end = '\0'; section = line + 1; }
            continue;
        }
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        m[section + "." + line] = eq + 1;
    }
    fclose(f);
    return m;
}
static float       ini_f(const IniMap& m, const std::string& k, float def)
    { auto it = m.find(k); return it != m.end() ? (float)atof(it->second.c_str()) : def; }
static int         ini_i(const IniMap& m, const std::string& k, int   def)
    { auto it = m.find(k); return it != m.end() ? atoi(it->second.c_str()) : def; }
static bool        ini_b(const IniMap& m, const std::string& k, bool  def)
    { auto it = m.find(k); return it != m.end() ? (it->second != "0") : def; }
static std::string ini_s(const IniMap& m, const std::string& k, const std::string& def)
    { auto it = m.find(k); return it != m.end() ? it->second : def; }

// ── saveSettings ──────────────────────────────────────────────────────────────

void EquationEditor::saveSettings(const std::string& path) const {
    AppSettings::ensureDirs();
    FILE* f = fopen(path.c_str(), "w");
    if (!f) { fprintf(stderr, "AppSettings: cannot write %s\n", path.c_str()); return; }

    const auto& E = m_engine;
    const auto& B = m_blend;
    const auto& G = m_glitch;
    const auto& C = m_colorSynth;
    const auto& MG = m_midiGen;

    // [fractal]
    fprintf(f, "[fractal]\n");
    fprintf(f, "zoom=%f\noffset_x=%f\noffset_y=%f\n", E.zoom, E.offset.x, E.offset.y);
    fprintf(f, "julia_c_x=%f\njulia_c_y=%f\n", E.juliaC.x, E.juliaC.y);
    fprintf(f, "power=%f\nmax_iter=%d\nbailout=%f\n", E.power, E.maxIter, E.bailout);
    fprintf(f, "formula=%d\nformula_b=%d\nformula_blend=%f\nformula_param=%f\n",
            E.formula, E.formulaB, E.formulaBlend, E.formulaParam);
    fprintf(f, "pixel_weight=%f\nlayer_count=%d\nlayer_offset=%f\n",
            E.pixelWeight, E.layerCount, E.layerOffset);
    fprintf(f, "geo_shape=%d\ngeo_sides=%d\ngeo_radius=%f\ngeo_rotation=%f\n",
            E.geoShape, E.geoSides, E.geoRadius, E.geoRotation);
    fprintf(f, "geo_tile=%d\ngeo_warp=%f\ngeo_mirror=%d\ngeo_kaleid=%d\n",
            (int)E.geoTile, E.geoWarp, E.geoMirror, E.geoKaleid);
    fprintf(f, "fractal_3d=%d\nmb_scale=%f\nmb_fold=%f\n",
            E.fractal3D, E.mbScale, E.mbFold);
    fprintf(f, "chaos_mode=%d\nchaos_strength=%f\nchaos_scale=%f\nchaos_speed=%f\n",
            E.chaosMode, E.chaosStrength, E.chaosScale, E.chaosSpeed);
    fprintf(f, "distortion_mode=%d\ndistort_speed=%f\ndistort_blobs=%d\n",
            (int)E.distortionMode, E.distortSpeed, E.distortBlobs);
    fprintf(f, "distort_glow=%f\ndistort_irid=%f\ndistort_outline=%f\n",
            E.distortGlow, E.distortIrid, E.distortOutline);

    // [blend]
    fprintf(f, "\n[blend]\n");
    fprintf(f, "mandelbrot=%f\njulia=%f\nmandelbulb=%f\neuclidean=%f\ndiff=%f\n",
            B.mandelbrot, B.julia, B.mandelbulb, B.euclidean, B.diff);

    // [color]
    fprintf(f, "\n[color]\n");
    fprintf(f, "enabled=%d\n", (int)C.enabled);
    fprintf(f, "hue_base=%f\nsat_base=%f\nlum_base=%f\n", C.hueBase, C.satBase, C.lumBase);
    fprintf(f, "hue_alt=%f\nsat_alt=%f\nlum_alt=%f\nalt_rate=%f\n",
            C.hueAlt, C.satAlt, C.lumAlt, C.altRate);
    fprintf(f, "hue_osc_amp=%f\nhue_osc_rate=%f\n", C.hueOscAmp, C.hueOscRate);
    fprintf(f, "lum_osc_amp=%f\nlum_osc_rate=%f\n", C.lumOscAmp, C.lumOscRate);
    fprintf(f, "hue_spread=%f\nlum_spread=%f\n", C.hueSpread, C.lumSpread);
    fprintf(f, "midi_hue_sens=%f\nmidi_sat_sens=%f\nmidi_lum_sens=%f\nmidi_decay=%f\n",
            C.midiHueSens, C.midiSatSens, C.midiLumSens, C.midiDecay);
    fprintf(f, "blend_mode=%d\n", C.blendMode);

    // [glitch]
    fprintf(f, "\n[glitch]\n");
    fprintf(f, "enabled=%d\nrate_hz=%f\ndur_min=%f\ndur_max=%f\nintensity=%f\n",
            (int)G.enabled, G.glitchRateHz, G.glitchDurMin, G.glitchDurMax, G.intensity);
    fprintf(f, "julia_jump=%d\nformula_flash=%d\nzoom_punch=%d\nblend_scatter=%d\n",
            (int)G.doJuliaJump, (int)G.doFormulaFlash, (int)G.doZoomPunch, (int)G.doBlendScatter);
    fprintf(f, "power_spike=%d\noffset_shift=%d\nvel_spike=%d\npitch_scramble=%d\nghost_note=%d\n",
            (int)G.doPowerSpike, (int)G.doOffsetShift,
            (int)G.doVelocitySpike, (int)G.doPitchScramble, (int)G.doGhostNote);
    fprintf(f, "midi_channel=%d\nnote_min=%d\nnote_max=%d\n",
            G.midiChannel, G.noteMin, G.noteMax);

    // [midi_gen]
    fprintf(f, "\n[midi_gen]\n");
    fprintf(f, "enabled=%d\nnote_enabled=%d\n", (int)MG.enabled, (int)MG.noteEnabled);
    fprintf(f, "note_min=%d\nnote_max=%d\nroot_key=%d\nscale=%d\n",
            MG.noteMin, MG.noteMax, MG.rootKey, (int)MG.scale);
    fprintf(f, "vel_min=%d\nvel_max=%d\nchannel=%d\nchord_size=%d\n",
            MG.velMin, MG.velMax, MG.channel, MG.chordSize);
    fprintf(f, "bpm=%f\nstep_rate_idx=%d\nnote_len_idx=%d\nrest_prob=%f\nhumanize=%d\n",
            MG.bpm, MG.stepRateIdx, MG.noteLenIdx, MG.restProb, (int)MG.humanize);
    fprintf(f, "pg_enabled=%d\npg_every=%d\npg_min=%d\npg_max=%d\nmidi_thru=%d\n",
            (int)MG.pgEnabled, MG.pgEvery, MG.pgMin, MG.pgMax, (int)MG.midiThru);

    // [midi_mappings]
    const auto& maps = m_midiMapper.mappings();
    fprintf(f, "\n[midi_mappings]\ncount=%d\n", (int)maps.size());
    for (int i = 0; i < (int)maps.size(); i++) {
        const auto& mm = maps[i];
        fprintf(f, "%d_type=%d\n%d_channel=%d\n%d_number=%d\n%d_param=%d\n"
                   "%d_min=%f\n%d_max=%f\n%d_label=%s\n",
                i, mm.msgType, i, mm.channel, i, mm.number, i, (int)mm.param,
                i, mm.minVal, i, mm.maxVal, i, mm.label);
    }

    // [stream]
    fprintf(f, "\n[stream]\n");
    fprintf(f, "bitrate_kbps=%d\nres_index=%d\naudio_device=%s\nvideo_path=%s\n",
            m_bitrateKbps, m_resIndex,
            m_streamOut.audioDevice.c_str(), m_videoPath);
    fprintf(f, "overlay_path=%s\noverlay_blend=%.3f\n",
            m_overlayPath, m_engine.overlayBlend);
    fprintf(f, "surge_bank=%d\nsurge_patch=%d\nsurge_auto=%d\nsurge_adv_secs=%.2f\n",
            m_surgeBank, m_surgePatch, (int)m_surgeAutoAdvance, m_surgeAdvanceSecs);
    int ndest = m_streamOut.destCount();
    fprintf(f, "dest_count=%d\n", ndest);
    for (int i = 0; i < ndest; i++) {
        const auto& d = m_streamOut.dest(i);
        fprintf(f, "dest%d_name=%s\ndest%d_url=%s\ndest%d_enabled=%d\n",
                i, d.name.c_str(), i, d.url.c_str(), i, (int)d.enabled);
    }

    fclose(f);
}

// ── loadSettings ──────────────────────────────────────────────────────────────

void EquationEditor::loadSettings(const std::string& path) {
    IniMap m = parseIni(path);
    if (m.empty()) return;

    auto& E  = m_engine;
    auto& B  = m_blend;
    auto& G  = m_glitch;
    auto& C  = m_colorSynth;
    auto& MG = m_midiGen;

    // [fractal]
    E.zoom           = ini_f(m, "fractal.zoom",           E.zoom);
    E.offset.x       = ini_f(m, "fractal.offset_x",       E.offset.x);
    E.offset.y       = ini_f(m, "fractal.offset_y",       E.offset.y);
    E.juliaC.x       = ini_f(m, "fractal.julia_c_x",      E.juliaC.x);
    E.juliaC.y       = ini_f(m, "fractal.julia_c_y",      E.juliaC.y);
    E.power          = ini_f(m, "fractal.power",           E.power);
    E.maxIter        = ini_i(m, "fractal.max_iter",        E.maxIter);
    E.bailout        = ini_f(m, "fractal.bailout",         E.bailout);
    E.formula        = ini_i(m, "fractal.formula",         E.formula);
    E.formulaB       = ini_i(m, "fractal.formula_b",       E.formulaB);
    E.formulaBlend   = ini_f(m, "fractal.formula_blend",   E.formulaBlend);
    E.formulaParam   = ini_f(m, "fractal.formula_param",   E.formulaParam);
    E.pixelWeight    = ini_f(m, "fractal.pixel_weight",    E.pixelWeight);
    E.layerCount     = ini_i(m, "fractal.layer_count",     E.layerCount);
    E.layerOffset    = ini_f(m, "fractal.layer_offset",    E.layerOffset);
    E.geoShape       = ini_i(m, "fractal.geo_shape",       E.geoShape);
    E.geoSides       = ini_i(m, "fractal.geo_sides",       E.geoSides);
    E.geoRadius      = ini_f(m, "fractal.geo_radius",      E.geoRadius);
    E.geoRotation    = ini_f(m, "fractal.geo_rotation",    E.geoRotation);
    E.geoTile        = ini_b(m, "fractal.geo_tile",        E.geoTile);
    E.geoWarp        = ini_f(m, "fractal.geo_warp",        E.geoWarp);
    E.geoMirror      = ini_i(m, "fractal.geo_mirror",      E.geoMirror);
    E.geoKaleid      = ini_i(m, "fractal.geo_kaleid",      E.geoKaleid);
    E.fractal3D      = ini_i(m, "fractal.fractal_3d",      E.fractal3D);
    E.mbScale        = ini_f(m, "fractal.mb_scale",        E.mbScale);
    E.mbFold         = ini_f(m, "fractal.mb_fold",         E.mbFold);
    E.chaosMode      = ini_i(m, "fractal.chaos_mode",      E.chaosMode);
    E.chaosStrength  = ini_f(m, "fractal.chaos_strength",  E.chaosStrength);
    E.chaosScale     = ini_f(m, "fractal.chaos_scale",     E.chaosScale);
    E.chaosSpeed     = ini_f(m, "fractal.chaos_speed",     E.chaosSpeed);
    E.distortionMode = ini_b(m, "fractal.distortion_mode", E.distortionMode);
    E.distortSpeed   = ini_f(m, "fractal.distort_speed",   E.distortSpeed);
    E.distortBlobs   = ini_i(m, "fractal.distort_blobs",   E.distortBlobs);
    E.distortGlow    = ini_f(m, "fractal.distort_glow",    E.distortGlow);
    E.distortIrid    = ini_f(m, "fractal.distort_irid",    E.distortIrid);
    E.distortOutline = ini_f(m, "fractal.distort_outline", E.distortOutline);

    // [blend]
    B.mandelbrot = ini_f(m, "blend.mandelbrot", B.mandelbrot);
    B.julia      = ini_f(m, "blend.julia",      B.julia);
    B.mandelbulb = ini_f(m, "blend.mandelbulb", B.mandelbulb);
    B.euclidean  = ini_f(m, "blend.euclidean",  B.euclidean);
    B.diff       = ini_f(m, "blend.diff",       B.diff);

    // [color]
    C.enabled      = ini_b(m, "color.enabled",       C.enabled);
    C.hueBase      = ini_f(m, "color.hue_base",      C.hueBase);
    C.satBase      = ini_f(m, "color.sat_base",      C.satBase);
    C.lumBase      = ini_f(m, "color.lum_base",      C.lumBase);
    C.hueAlt       = ini_f(m, "color.hue_alt",       C.hueAlt);
    C.satAlt       = ini_f(m, "color.sat_alt",       C.satAlt);
    C.lumAlt       = ini_f(m, "color.lum_alt",       C.lumAlt);
    C.altRate      = ini_f(m, "color.alt_rate",      C.altRate);
    C.hueOscAmp    = ini_f(m, "color.hue_osc_amp",   C.hueOscAmp);
    C.hueOscRate   = ini_f(m, "color.hue_osc_rate",  C.hueOscRate);
    C.lumOscAmp    = ini_f(m, "color.lum_osc_amp",   C.lumOscAmp);
    C.lumOscRate   = ini_f(m, "color.lum_osc_rate",  C.lumOscRate);
    C.hueSpread    = ini_f(m, "color.hue_spread",    C.hueSpread);
    C.lumSpread    = ini_f(m, "color.lum_spread",    C.lumSpread);
    C.midiHueSens  = ini_f(m, "color.midi_hue_sens", C.midiHueSens);
    C.midiSatSens  = ini_f(m, "color.midi_sat_sens", C.midiSatSens);
    C.midiLumSens  = ini_f(m, "color.midi_lum_sens", C.midiLumSens);
    C.midiDecay    = ini_f(m, "color.midi_decay",    C.midiDecay);
    C.blendMode    = ini_i(m, "color.blend_mode",    C.blendMode);

    // [glitch]
    G.enabled        = ini_b(m, "glitch.enabled",       G.enabled);
    G.glitchRateHz   = ini_f(m, "glitch.rate_hz",       G.glitchRateHz);
    G.glitchDurMin   = ini_f(m, "glitch.dur_min",       G.glitchDurMin);
    G.glitchDurMax   = ini_f(m, "glitch.dur_max",       G.glitchDurMax);
    G.intensity      = ini_f(m, "glitch.intensity",     G.intensity);
    G.doJuliaJump    = ini_b(m, "glitch.julia_jump",    G.doJuliaJump);
    G.doFormulaFlash = ini_b(m, "glitch.formula_flash", G.doFormulaFlash);
    G.doZoomPunch    = ini_b(m, "glitch.zoom_punch",    G.doZoomPunch);
    G.doBlendScatter = ini_b(m, "glitch.blend_scatter", G.doBlendScatter);
    G.doPowerSpike   = ini_b(m, "glitch.power_spike",   G.doPowerSpike);
    G.doOffsetShift  = ini_b(m, "glitch.offset_shift",  G.doOffsetShift);
    G.doVelocitySpike  = ini_b(m, "glitch.vel_spike",     G.doVelocitySpike);
    G.doPitchScramble  = ini_b(m, "glitch.pitch_scramble",G.doPitchScramble);
    G.doGhostNote      = ini_b(m, "glitch.ghost_note",    G.doGhostNote);
    G.midiChannel    = ini_i(m, "glitch.midi_channel",  G.midiChannel);
    G.noteMin        = ini_i(m, "glitch.note_min",      G.noteMin);
    G.noteMax        = ini_i(m, "glitch.note_max",      G.noteMax);

    // [midi_gen]
    MG.enabled      = ini_b(m, "midi_gen.enabled",       MG.enabled);
    MG.noteEnabled  = ini_b(m, "midi_gen.note_enabled",  MG.noteEnabled);
    MG.noteMin      = ini_i(m, "midi_gen.note_min",      MG.noteMin);
    MG.noteMax      = ini_i(m, "midi_gen.note_max",      MG.noteMax);
    MG.rootKey      = ini_i(m, "midi_gen.root_key",      MG.rootKey);
    MG.scale        = (GenScale)ini_i(m, "midi_gen.scale", (int)MG.scale);
    MG.velMin       = ini_i(m, "midi_gen.vel_min",       MG.velMin);
    MG.velMax       = ini_i(m, "midi_gen.vel_max",       MG.velMax);
    MG.channel      = ini_i(m, "midi_gen.channel",       MG.channel);
    MG.chordSize    = ini_i(m, "midi_gen.chord_size",    MG.chordSize);
    MG.bpm          = ini_f(m, "midi_gen.bpm",           MG.bpm);
    MG.stepRateIdx  = ini_i(m, "midi_gen.step_rate_idx", MG.stepRateIdx);
    MG.noteLenIdx   = ini_i(m, "midi_gen.note_len_idx",  MG.noteLenIdx);
    MG.restProb     = ini_f(m, "midi_gen.rest_prob",     MG.restProb);
    MG.humanize     = ini_b(m, "midi_gen.humanize",      MG.humanize);
    MG.pgEnabled    = ini_b(m, "midi_gen.pg_enabled",    MG.pgEnabled);
    MG.pgEvery      = ini_i(m, "midi_gen.pg_every",      MG.pgEvery);
    MG.pgMin        = ini_i(m, "midi_gen.pg_min",        MG.pgMin);
    MG.pgMax        = ini_i(m, "midi_gen.pg_max",        MG.pgMax);
    MG.midiThru     = ini_b(m, "midi_gen.midi_thru",     MG.midiThru);

    // [midi_mappings]
    int nmaps = ini_i(m, "midi_mappings.count", 0);
    if (nmaps > 0) {
        m_midiMapper.mappings().clear();
        for (int i = 0; i < nmaps; i++) {
            std::string pfx = "midi_mappings." + std::to_string(i);
            MidiMapping mm{};
            mm.msgType = ini_i(m, pfx + "_type",    0);
            mm.channel = ini_i(m, pfx + "_channel", 0);
            mm.number  = ini_i(m, pfx + "_number",  0);
            mm.param   = (MidiParam)ini_i(m, pfx + "_param", 0);
            mm.minVal  = ini_f(m, pfx + "_min",     0.0f);
            mm.maxVal  = ini_f(m, pfx + "_max",     1.0f);
            std::string lbl = ini_s(m, pfx + "_label", "");
            strncpy(mm.label, lbl.c_str(), sizeof(mm.label) - 1);
            m_midiMapper.add(mm);
        }
    }

    // [stream]
    m_bitrateKbps = ini_i(m, "stream.bitrate_kbps", m_bitrateKbps);
    m_resIndex    = ini_i(m, "stream.res_index",    m_resIndex);
    m_surgeBank         = ini_i(m, "stream.surge_bank",      m_surgeBank);
    m_surgePatch        = ini_i(m, "stream.surge_patch",     m_surgePatch);
    m_surgeAutoAdvance  = ini_i(m, "stream.surge_auto",      (int)m_surgeAutoAdvance);
    m_surgeAdvanceSecs  = ini_f(m, "stream.surge_adv_secs",  m_surgeAdvanceSecs);
    {
        std::string dev = ini_s(m, "stream.audio_device", m_streamOut.audioDevice);
        m_streamOut.audioDevice = dev;
    }
    {
        std::string vp = ini_s(m, "stream.video_path", "");
        if (!vp.empty()) {
            strncpy(m_videoPath, vp.c_str(), sizeof(m_videoPath) - 1);
            m_videoIn.open(vp);
        }
    }
    {
        std::string op = ini_s(m, "stream.overlay_path", "");
        if (!op.empty()) {
            strncpy(m_overlayPath, op.c_str(), sizeof(m_overlayPath) - 1);
            m_overlayIn.open(op);
            m_streamOut.overlayAudioPath = op;
        }
        m_engine.overlayBlend = ini_f(m, "stream.overlay_blend", m_engine.overlayBlend);
    }
    int ndest = ini_i(m, "stream.dest_count", 0);
    if (ndest > 0) {
        // Remove existing destinations, then restore saved ones
        while (m_streamOut.destCount() > 0) m_streamOut.removeDestination(0);
        for (int i = 0; i < ndest; i++) {
            std::string pfx = "stream.dest" + std::to_string(i);
            std::string dname = ini_s(m, pfx + "_name", "");
            std::string durl  = ini_s(m, pfx + "_url",  "");
            bool den          = ini_b(m, pfx + "_enabled", true);
            if (!dname.empty()) {
                m_streamOut.addDestination(dname, durl);
                m_streamOut.dest(m_streamOut.destCount() - 1).enabled = den;
            }
        }
    }

    // Ensure the Restream destination is always present (user's primary service)
    bool hasRestream = false;
    for (int i = 0; i < m_streamOut.destCount(); i++) {
        const auto& d = m_streamOut.dest(i);
        if (d.name == "Restream" ||
            d.url.rfind("rtmp://live.restream.io/live/", 0) == 0) {
            hasRestream = true; break;
        }
    }
    if (!hasRestream)
        m_streamOut.addDestination("Restream", "rtmp://live.restream.io/live/");
}

// ── Presets panel ─────────────────────────────────────────────────────────────

void EquationEditor::drawPresetsPanel() {
    // Refresh list when needed
    if (m_presetListDirty) {
        m_presetList = AppSettings::listPresets();
        m_presetListDirty = false;
    }

    // Save row
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##pname", m_presetName, sizeof(m_presetName));
    ImGui::SameLine();
    if (ImGui::Button("Save") && m_presetName[0] != '\0') {
        saveSettings(AppSettings::presetPath(m_presetName));
        m_presetListDirty = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Save current settings as a named preset");

    ImGui::SameLine();
    if (ImGui::Button("Save 'last'"))
        saveSettings(AppSettings::lastPath());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Overwrite the auto-saved last session now");

    ImGui::Spacing();

    // Preset list
    if (m_presetList.empty()) {
        ImGui::TextDisabled("No presets yet — type a name and click Save");
        return;
    }

    int deleteIdx = -1;
    for (int i = 0; i < (int)m_presetList.size(); i++) {
        ImGui::PushID(i);
        if (ImGui::SmallButton("Load")) {
            loadSettings(AppSettings::presetPath(m_presetList[i]));
            // Copy name into the name field so "Save" overwrites it easily
            strncpy(m_presetName, m_presetList[i].c_str(), sizeof(m_presetName) - 1);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) deleteIdx = i;
        ImGui::SameLine();
        ImGui::TextUnformatted(m_presetList[i].c_str());
        ImGui::PopID();
    }

    if (deleteIdx >= 0) {
        std::string p = AppSettings::presetPath(m_presetList[deleteIdx]);
        remove(p.c_str());
        m_presetListDirty = true;
    }
}

// ── Surge XT constants ────────────────────────────────────────────────────────

// Factory bank names — approximate alphabetical order matching a standard
// Surge XT installation.  User banks follow from index 17 onwards.
static const char* kSurgeBankNames[] = {
    "0  Bass",            "1  Brass",           "2  Chip",
    "3  Drone & Atmos",   "4  Keys",             "5  Lead",
    "6  Orchestral",      "7  Pad",              "8  Piano",
    "9  Plucked",         "10 Seq & Arps",       "11 Splits & Layers",
    "12 String",          "13 Synth",            "14 Template",
    "15 Voc & Bell",      "16 Wind & Blow",
    "17 User 0",          "18 User 1",           "19 User 2",
    "20 User 3",
};
static constexpr int kSurgeBankNameCount = 21;

// Default CC41–48 → fractal param mappings (Surge XT Macros 1–8)
struct SurgeDefaultMap {
    int cc; MidiParam param; float minV; float maxV; const char* label;
};
static const SurgeDefaultMap kSurgeMaps[] = {
    { 41, MidiParam::JuliaCX,          -2.0f,  2.0f,  "M1 → Julia X"   },
    { 42, MidiParam::JuliaCY,          -2.0f,  2.0f,  "M2 → Julia Y"   },
    { 43, MidiParam::Zoom,              0.1f,  8.0f,  "M3 → Zoom"      },
    { 44, MidiParam::FormulaBlend,      0.0f,  1.0f,  "M4 → FmlaBlend" },
    { 45, MidiParam::ColorHue,          0.0f,  1.0f,  "M5 → Color Hue" },
    { 46, MidiParam::GeoWarp,           0.0f,  1.0f,  "M6 → Geo Warp"  },
    { 47, MidiParam::Power,             1.0f, 12.0f,  "M7 → Power"     },
    { 48, MidiParam::BlendMandelbrot,   0.0f,  1.0f,  "M8 → M.Blend"   },
};
static constexpr int kSurgeMapsCount = 8;

// ── applyDefaultSurgeMappings ─────────────────────────────────────────────────

void EquationEditor::applyDefaultSurgeMappings() {
    auto& maps = m_midiMapper.mappings();

    // Remove any existing CC41–48 entries so we start clean
    maps.erase(
        std::remove_if(maps.begin(), maps.end(), [](const MidiMapping& m) {
            return m.msgType == 0 && m.number >= 41 && m.number <= 48;
        }),
        maps.end()
    );

    // Add the 8 standard Surge XT macro mappings
    for (const auto& sm : kSurgeMaps) {
        MidiMapping mm{};
        mm.msgType = 0;        // CC
        mm.channel = 0;        // any channel
        mm.number  = sm.cc;
        mm.param   = sm.param;
        mm.minVal  = sm.minV;
        mm.maxVal  = sm.maxV;
        strncpy(mm.label, sm.label, sizeof(mm.label) - 1);
        m_midiMapper.add(mm);
    }
}

// ── drawSurgeXTSection ────────────────────────────────────────────────────────

void EquationEditor::drawSurgeXTSection() {
    if (!ImGui::CollapsingHeader("Surge XT")) return;

    const uint8_t ch0 = (uint8_t)(std::max(1, m_midiGen.channel) - 1);

    // Helper: send CC0 (bank) + PC (patch) to MIDI out immediately
    auto sendNow = [&]() {
        m_midiOut.sendRaw(0xB0 | ch0, 0, (uint8_t)m_surgeBank);   // CC0 bank
        m_midiOut.sendRaw(0xC0 | ch0, (uint8_t)m_surgePatch);      // PC
    };

    // ── Patch browser ─────────────────────────────────────────────────────────
    ImGui::TextDisabled("Patch browser — sends immediately to MIDI out");
    ImGui::Spacing();

    // Bank input
    ImGui::SetNextItemWidth(55);
    if (ImGui::InputInt("Bank##surge", &m_surgeBank)) {
        m_surgeBank = std::max(0, m_surgeBank);
        sendNow();
    }
    ImGui::SameLine();
    if (m_surgeBank < kSurgeBankNameCount)
        ImGui::TextColored({0.6f,0.9f,1.0f,1}, "%s", kSurgeBankNames[m_surgeBank]);
    else
        ImGui::TextDisabled("User bank %d", m_surgeBank);

    // Patch slider (full width)
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderInt("Patch##surge", &m_surgePatch, 0, 127)) {
        m_surgePatch = std::max(0, std::min(127, m_surgePatch));
        sendNow();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+click to type exact patch number\n"
                          "Combined index: %d", m_surgeBank * 128 + m_surgePatch);

    // ── Auto-advance timer ────────────────────────────────────────────────────
    float now = (float)ImGui::GetTime();
    if (m_surgeAutoAdvance && m_midiOut.isOpen()) {
        if (now - m_surgeLastAdvance >= m_surgeAdvanceSecs) {
            m_surgeLastAdvance = now;
            if (m_surgePatch < 127) { m_surgePatch++; }
            else                    { m_surgeBank++;  m_surgePatch = 0; }
            sendNow();
        }
    }

    // Auto-advance checkbox + interval
    if (ImGui::Checkbox("Auto-advance##surge", &m_surgeAutoAdvance)) {
        m_surgeLastAdvance = now;   // reset timer on toggle
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Automatically step to the next patch every N seconds.\n"
                          "Wraps to next bank at patch 127.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::InputFloat("s##surgeadv", &m_surgeAdvanceSecs, 0.5f, 2.0f, "%.1f");
    m_surgeAdvanceSecs = std::max(0.1f, m_surgeAdvanceSecs);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Seconds between automatic patch advances");

    // Progress bar showing time until next auto-step
    if (m_surgeAutoAdvance) {
        float pct = std::min(1.0f, (now - m_surgeLastAdvance) / m_surgeAdvanceSecs);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::ProgressBar(pct, {-1, 0}, "");
    }

    // Manual navigation + send
    if (ImGui::ArrowButton("##surgeprev", ImGuiDir_Left)) {
        if (m_surgePatch > 0)      { m_surgePatch--; }
        else if (m_surgeBank > 0)  { m_surgeBank--;  m_surgePatch = 127; }
        m_surgeLastAdvance = now;
        sendNow();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous patch (resets auto timer)");
    ImGui::SameLine();
    if (ImGui::ArrowButton("##surgenext", ImGuiDir_Right)) {
        if (m_surgePatch < 127) { m_surgePatch++; }
        else                    { m_surgeBank++;  m_surgePatch = 0; }
        m_surgeLastAdvance = now;
        sendNow();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next patch (resets auto timer)");
    ImGui::SameLine();
    if (ImGui::Button("Send Now")) { m_surgeLastAdvance = now; sendNow(); }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Send CC0 bank=%d + PC %d  (combined index %d)",
                          m_surgeBank, m_surgePatch, m_surgeBank*128 + m_surgePatch);
    ImGui::SameLine();
    ImGui::TextDisabled("= patch #%d", m_surgeBank * 128 + m_surgePatch);

    // MIDI out status reminder
    if (!m_midiOut.isOpen())
        ImGui::TextColored({1.0f,0.6f,0.2f,1}, "  ⚠ MIDI out not connected — connect above");

    ImGui::Separator();

    // ── Macro CC mapping display (CC41–48) ────────────────────────────────────
    ImGui::TextDisabled("Macro CCs  (CC41–48 = Surge XT Macros 1–8)");
    ImGui::Spacing();

    // Show each CC41–48 and what it's currently mapped to in MidiMapper
    const auto& maps = m_midiMapper.mappings();
    for (const auto& sm : kSurgeMaps) {
        // Find active mapping for this CC
        const MidiMapping* found = nullptr;
        for (const auto& mm : maps)
            if (mm.msgType == 0 && mm.number == sm.cc) { found = &mm; break; }

        ImGui::TextColored({0.7f,0.7f,0.7f,1}, "CC%-3d", sm.cc);
        ImGui::SameLine();
        if (found) {
            ImGui::TextColored({0.3f,1.0f,0.5f,1}, "→ %-16s",
                               midiParamName(found->param));
            ImGui::SameLine();
            ImGui::TextDisabled("[%.2f .. %.2f]", found->minVal, found->maxVal);
        } else {
            ImGui::TextColored({0.5f,0.5f,0.5f,1}, "  (not mapped)");
            ImGui::SameLine();
            ImGui::TextDisabled("suggested: %s", sm.label);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ── Apply / Save buttons ──────────────────────────────────────────────────
    if (ImGui::Button("Apply default Surge XT mappings")) {
        applyDefaultSurgeMappings();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Maps CC41–48 to:\n"
                          "M1 Julia X  M2 Julia Y  M3 Zoom  M4 Formula Blend\n"
                          "M5 Color Hue  M6 Geo Warp  M7 Power  M8 M.Blend\n"
                          "Existing CC41–48 mappings are replaced.");

    ImGui::SameLine();
    if (ImGui::Button("Apply + Save as 'SurgeXT'")) {
        applyDefaultSurgeMappings();
        // Also set sensible generator PC defaults for Surge XT
        m_midiGen.pgEnabled  = true;
        m_midiGen.pgEvery    = 8;
        m_midiGen.pgMin      = m_surgeBank * 128;
        m_midiGen.pgMax      = m_surgeBank * 128 + 127;
        saveSettings(AppSettings::presetPath("SurgeXT"));
        m_presetListDirty = true;
        strncpy(m_presetName, "SurgeXT", sizeof(m_presetName) - 1);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Apply the 8 macro mappings, enable PC auto-change\n"
                          "for the currently selected bank, then save as\n"
                          "~/.fractal_stream/presets/SurgeXT.ini");

    ImGui::Spacing();
    ImGui::TextDisabled("In Surge XT: right-click a Macro knob → Assign MIDI CC → move the knob");
}
