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
                                VideoInput& videoIn, StreamOutput& streamOut)
    : m_engine(engine), m_blend(blend),
      m_videoIn(videoIn), m_streamOut(streamOut) {}

void EquationEditor::draw() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({340, 620}, ImGuiCond_Once);
    ImGui::Begin("Fractal Stream Controls");

    if (ImGui::CollapsingHeader("Blend", ImGuiTreeNodeFlags_DefaultOpen))
        drawBlendPanel();
    if (ImGui::CollapsingHeader("Fractal Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        drawFractalPanel();
    if (ImGui::CollapsingHeader("Euclidean Geometry"))
        drawGeometryPanel();
    if (ImGui::CollapsingHeader("Video Input", ImGuiTreeNodeFlags_DefaultOpen))
        drawVideoPanel();
    if (ImGui::CollapsingHeader("Stream Output"))
        drawStreamPanel();

    ImGui::End();
}

void EquationEditor::drawBlendPanel() {
    ImGui::SliderFloat("Mandelbrot",  &m_blend.mandelbrot, 0.0f, 1.0f);
    ImGui::SliderFloat("Julia",       &m_blend.julia,      0.0f, 1.0f);
    ImGui::SliderFloat("Mandelbulb",  &m_blend.mandelbulb, 0.0f, 1.0f);
    ImGui::SliderFloat("Euclidean",   &m_blend.euclidean,  0.0f, 1.0f);

    float total = m_blend.mandelbrot + m_blend.julia +
                  m_blend.mandelbulb + m_blend.euclidean;
    ImGui::Separator();
    ImGui::Text("Total blend: %.2f", total);
    if (ImGui::Button("Normalize")) {
        if (total > 0.001f) {
            m_blend.mandelbrot /= total;
            m_blend.julia      /= total;
            m_blend.mandelbulb /= total;
            m_blend.euclidean  /= total;
        }
    }
}

void EquationEditor::drawFractalPanel() {
    ImGui::DragFloat2("Julia C (re,im)", &m_engine.juliaC.x,
                      0.001f, -2.0f, 2.0f, "%.4f");
    ImGui::DragFloat("Mandelbulb power", &m_engine.power,
                     0.1f, 2.0f, 16.0f, "%.1f");
    ImGui::SliderInt("Max iterations",   &m_engine.maxIter, 16, 512);
    ImGui::DragFloat("Bailout radius",   &m_engine.bailout, 0.1f, 2.0f, 10.0f);
    ImGui::Separator();
    ImGui::DragFloat("Zoom",  &m_engine.zoom,   0.01f, 0.1f, 1000.0f, "%.3f",
                     ImGuiSliderFlags_Logarithmic);
    ImGui::DragFloat2("Offset (x,y)",    &m_engine.offset.x, 0.001f);

    if (ImGui::Button("Reset view")) {
        m_engine.zoom   = 1.0f;
        m_engine.offset = {0.0f, 0.0f};
    }

    // Animate Julia C along a circle
    static bool animJulia = false;
    static float animSpeed = 0.3f;
    ImGui::Checkbox("Animate Julia C", &animJulia);
    if (animJulia) {
        ImGui::SameLine();
        ImGui::SliderFloat("##speed", &animSpeed, 0.01f, 2.0f);
        double t = ImGui::GetTime();
        m_engine.juliaC.x = (float)(0.7885 * cos(t * animSpeed));
        m_engine.juliaC.y = (float)(0.7885 * sin(t * animSpeed));
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

void EquationEditor::drawStreamPanel() {
    ImGui::InputText("RTMP URL", m_rtmpUrl, sizeof(m_rtmpUrl));
    ImGui::InputText("Stream key", m_streamKey, sizeof(m_streamKey),
                     ImGuiInputTextFlags_Password);

    // Build full URL on the fly (append key if not already in URL)
    auto buildURL = [&]() -> std::string {
        std::string u = m_rtmpUrl;
        if (m_streamKey[0] != '\0' && u.find(m_streamKey) == std::string::npos) {
            if (u.back() != '/') u += '/';
            u += m_streamKey;
        }
        return u;
    };

    ImGui::SliderInt("Bitrate (kbps)", &m_bitrateKbps, 500, 40000);
    ImGui::Combo("Resolution", &m_resIndex, kResLabels, 4);

    ImGui::Separator();
    if (!m_streamOut.isStreaming()) {
        if (ImGui::Button("Start Stream")) {
            std::string fullUrl = buildURL();
            m_streamOut.start(fullUrl,
                              kResW[m_resIndex], kResH[m_resIndex],
                              m_bitrateKbps, 30);
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f,0.1f,0.1f,1.0f));
        if (ImGui::Button("Stop Stream")) m_streamOut.stop();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored({0.2f,1.0f,0.2f,1.0f}, "LIVE");
    }
}
