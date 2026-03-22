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
#include "milkdrop/PresetManager.h"
#include "milkdrop/MilkDropGLRenderer.h"
#include "audio/IAudioCapture.h"
#include "audio/BeatDetector.h"
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

    // Accessors queried by main.cpp each frame
    bool  streamMilkDrop()   const { return m_streamMilkDrop; }
    float mdFractalBlend()   const { return m_mdFractalBlend; }
    bool  mdFractalOverlay() const { return m_mdFractalOverlay; }

    // Wire in MilkDrop subsystems (call after construction, before first draw()).
    void setMilkDrop(PresetManager* pm, MilkDropGLRenderer* md,
                     IAudioCapture* audio, BeatDetector* beat);

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

    // MilkDrop subsystems (optional — null until setMilkDrop() called)
    PresetManager*      m_presetMgr  = nullptr;
    MilkDropGLRenderer* m_mdRenderer = nullptr;
    IAudioCapture*      m_audio      = nullptr;
    BeatDetector*       m_beatDet    = nullptr;

    // MilkDrop panel state
    char  m_mdSearch[256]       = "";
    int   m_mdSelectedIdx       = -1;
    float m_mdFractalBlend      = 0.4f;
    float m_mdPresetDuration    = 12.0f;
    bool  m_mdAutoAdvance       = false;
    bool  m_mdFractalOverlay    = false;
    bool  m_streamMilkDrop      = true;   // stream MD output when active (default on)
    int   m_mdBlendType         = 0;      // transition type
    float m_mdAutoTimer         = 0.f;
    // Hardcut config exposed in UI
    float m_hardcutLowThreshold  = 0.8f;
    float m_hardcutHighThreshold = 0.5f;
    float m_hardcutMinDelay      = 3.0f;
    int   m_hardcutMode          = 2;     // BassAndTreble

    void drawMilkDropPanel();
    void drawAudioPanel();

    // Surge XT browser state
    int   m_surgeBank        = 0;
    int   m_surgePatch       = 0;
    bool  m_surgeAutoAdvance = false;
    float m_surgeAdvanceSecs = 4.0f;
    float m_surgeLastAdvance = 0.0f;  // ImGui time of last auto-step

    void applyDefaultSurgeMappings();
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
    void drawSurgeXTSection();
};
