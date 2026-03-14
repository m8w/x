#pragma once
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include "fractal/GlitchEngine.h"
#include "fractal/ColorSynth.h"
#include "stream/VideoInput.h"
#include "stream/StreamOutput.h"
#include "midi/MidiInput.h"
#include "midi/MidiOutput.h"
#include "midi/MidiMapper.h"
#include "midi/MidiGenerator.h"
#include <string>

class EquationEditor {
public:
    EquationEditor(FractalEngine& engine, BlendController& blend,
                   GlitchEngine& glitch, ColorSynth& colorSynth,
                   VideoInput& videoIn, StreamOutput& streamOut,
                   MidiInput& midiIn, MidiOutput& midiOut,
                   MidiMapper& midiMapper, MidiGenerator& midiGen);
    void draw();   // Call once per frame after ImGui::NewFrame()

private:
    FractalEngine&   m_engine;
    BlendController& m_blend;
    GlitchEngine&    m_glitch;
    ColorSynth&      m_colorSynth;
    VideoInput&      m_videoIn;
    StreamOutput&    m_streamOut;
    MidiInput&       m_midiIn;
    MidiOutput&      m_midiOut;
    MidiMapper&      m_midiMapper;
    MidiGenerator&   m_midiGen;

    // Stream panel state
    int  m_bitrateKbps = 2500;
    int  m_resIndex    = 1;
    char m_videoPath[512] = "";

    char m_newName[64]  = "";
    char m_newUrl[512]  = "";

    void drawBlendPanel();
    void drawFractalPanel();
    void drawGeometryPanel();
    void drawVideoPanel();
    void drawStreamPanel();
    void drawAnimPanel();
    void drawMidiWindow();
    void drawGlitchPanel();
    void drawColorSynthPanel();
    void drawDistortionPanel();
    void drawChaosPanel();
};
