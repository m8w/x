#pragma once
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include "stream/VideoInput.h"
#include "stream/StreamOutput.h"
#include "midi/MidiInput.h"
#include "midi/MidiMapper.h"
#include "midi/MidiGenerator.h"
#include <string>

class EquationEditor {
public:
    EquationEditor(FractalEngine& engine, BlendController& blend,
                   VideoInput& videoIn, StreamOutput& streamOut,
                   MidiInput& midiIn, MidiMapper& midiMapper,
                   MidiGenerator& midiGen);
    void draw();   // Call once per frame after ImGui::NewFrame()

private:
    FractalEngine&   m_engine;
    BlendController& m_blend;
    VideoInput&      m_videoIn;
    StreamOutput&    m_streamOut;
    MidiInput&       m_midiIn;
    MidiMapper&      m_midiMapper;
    MidiGenerator&   m_midiGen;

    // Stream panel state
    int  m_bitrateKbps = 4000;
    int  m_resIndex    = 1;   // default 1080p
    char m_videoPath[512] = "";

    // Add-destination form
    char m_newName[64]  = "";
    char m_newUrl[512]  = "";

    void drawBlendPanel();
    void drawFractalPanel();
    void drawGeometryPanel();
    void drawVideoPanel();
    void drawStreamPanel();
    void drawAnimPanel();
    void drawMidiWindow();   // UI2 — separate floating ImGui window
};
