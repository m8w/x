#pragma once
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include "stream/VideoInput.h"
#include "stream/StreamOutput.h"
#include <string>

class EquationEditor {
public:
    EquationEditor(FractalEngine& engine, BlendController& blend,
                   VideoInput& videoIn, StreamOutput& streamOut);
    void draw();   // Call once per frame after ImGui::NewFrame()

private:
    FractalEngine&  m_engine;
    BlendController& m_blend;
    VideoInput&     m_videoIn;
    StreamOutput&   m_streamOut;

    // Stream panel state
    char m_rtmpUrl[512]  = "rtmp://live.restream.io/live/";
    char m_streamKey[256]= "";
    int  m_bitrateKbps   = 4000;
    int  m_resIndex      = 0;   // 0=720p 1=1080p 2=1440p
    char m_videoPath[512]= "";

    void drawBlendPanel();
    void drawFractalPanel();
    void drawGeometryPanel();
    void drawVideoPanel();
    void drawStreamPanel();
};
