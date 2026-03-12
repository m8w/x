#include "EquationEditor.h"
#include "FilePicker.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>

static const char* kResLabels[] = {"1280x720", "1920x1080", "2560x1440", "3840x2160 (4K)"};
static const int   kResW[]      = {1280, 1920, 2560, 3840};
static const int   kResH[]      = { 720, 1080, 1440, 2160};
static const char* kShapeLabels[] = {"Circle", "Polygon", "Star", "Grid"};

EquationEditor::EquationEditor(FractalEngine& engine, BlendController& blend,
                                VideoInput& videoIn, StreamOutput& streamOut,
                                MidiInput& midiIn, MidiMapper& midiMapper,
                                MidiGenerator& midiGen)
    : m_engine(engine), m_blend(blend),
      m_videoIn(videoIn), m_streamOut(streamOut),
      m_midiIn(midiIn), m_midiMapper(midiMapper), m_midiGen(midiGen) {}

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
    if (ImGui::CollapsingHeader("Stream Output"))
        drawStreamPanel();

    ImGui::End();

    // UI2 — MIDI Mapper (separate window)
    drawMidiWindow();
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
    "z\xc2\xb2 + c  (Mandelbrot)",      //  0
    "sin(z) + c",                         //  1
    "exp(z) + c",                         //  2
    "cos(z) + c",                         //  3
    "sinh(z) + c",                        //  4
    "cosh(z) + c",                        //  5
    "Burning Ship",                       //  6
    "Tricorn",                            //  7
    "Newton z\xc2\xb3\xe2\x88\x92" "1",   //  8
    "Phoenix",                            //  9
    "z\xe2\x81\xbf + c  (power)",        // 10
};
static const char* k3DTypes[] = {
    "Mandelbulb",
    "Mandelbox",
    "Quaternion Julia",
};

void EquationEditor::drawFractalPanel() {
    // ── Iteration formula A × B cross-blend ──────────────────────────────────
    ImGui::TextDisabled("Formula A  \xe2\x86\x94  Formula B");
    ImGui::Combo("Formula A##sel", &m_engine.formula,  kFormulas, 11);
    ImGui::Combo("Formula B##sel", &m_engine.formulaB, kFormulas, 11);
    ImGui::SliderFloat("A \xe2\x86\x94 B blend", &m_engine.formulaBlend, 0.0f, 1.0f);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 = pure Formula A   1 = pure Formula B   0.5 = crossfade");

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
    ImGui::SliderInt("Bitrate (kbps)", &m_bitrateKbps, 500, 40000);
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
    if (ImGui::CollapsingHeader("MIDI Port", ImGuiTreeNodeFlags_DefaultOpen)) {
        int nPorts = m_midiIn.portCount();
        static int selectedPort = 0;

        if (nPorts == 0) {
            ImGui::TextColored({1,0.4f,0.4f,1}, "No MIDI ports found");
        } else {
            static char portBuf[1024]; portBuf[0] = '\0';
            for (int i = 0; i < nPorts; i++) {
                auto nm = m_midiIn.portName(i);
                size_t pos = strlen(portBuf);
                strncpy(portBuf+pos, nm.c_str(), sizeof(portBuf)-pos-2);
                portBuf[strlen(portBuf)+1] = '\0';
            }
            ImGui::SetNextItemWidth(240);
            ImGui::Combo("Port##midi", &selectedPort, portBuf);
            ImGui::SameLine();
            if (!m_midiIn.isOpen()) {
                if (ImGui::Button("Connect")) m_midiIn.open(selectedPort);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.1f,0.1f,1));
                if (ImGui::Button("Disconnect")) m_midiIn.close();
                ImGui::PopStyleColor();
            }
        }
        if (m_midiIn.isOpen()) {
            auto last = m_midiIn.lastMessage();
            int  type = (last.status & 0xF0), ch = (last.status & 0x0F)+1;
            const char* tn = type==0xB0?"CC":type==0x90?"NoteOn":type==0x80?"NoteOff":type==0xC0?"PC":"—";
            ImGui::TextColored({0.2f,1,0.2f,1},"● %s",
                               m_midiIn.portName(m_midiIn.openedPort()).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("Last: %s ch%d #%d v%d", tn,ch,last.data1,last.data2);
        } else {
            ImGui::TextColored({0.5f,0.5f,0.5f,1},"○ Not connected");
        }
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
                for (auto& m : offs) m_midiMapper.apply(m, m_engine, m_blend);
            }
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (ImGui::Button("♪ One Note")) {
            // Fire a single note and NoteOff via MidiMapper
            auto msgs = G.fireOneNote();
            for (auto& m : msgs) m_midiMapper.apply(m, m_engine, m_blend);
        }
        ImGui::SameLine();
        if (ImGui::Button("⚠ Panic")) {
            // NoteOff all active notes via MidiMapper
            std::vector<MidiInput::Message> offs;
            G.stop(offs);
            for (auto& m : offs) m_midiMapper.apply(m, m_engine, m_blend);
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
