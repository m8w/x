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
#include <vector>

class EquationEditor {
public:
    EquationEditor(FractalEngine& engine, BlendController& blend,
                   GlitchEngine& glitch, ColorSynth& colorSynth,
                   VideoInput& videoIn, StreamOutput& streamOut,
                   MidiInput& midiIn, MidiOutput& midiOut,
                   MidiMapper& midiMapper, MidiGenerator& midiGen);
    void draw();   // Call once per frame after ImGui::NewFrame()

    // Persist all panel state to / from an INI file.
    void saveSettings(const std::string& path) const;
    void loadSettings(const std::string& path);

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

    // Preset panel state
    char                     m_presetName[64] = "";
    std::vector<std::string> m_presetList;
    bool                     m_presetListDirty = true;

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
    void drawPresetsPanel();
};
