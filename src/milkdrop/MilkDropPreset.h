#pragma once
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// MilkDrop preset data model
// Ported from MilkDropMac/Presets/MilkDropPreset.swift
// ---------------------------------------------------------------------------

// Transition blend modes (15 MilkDrop 3 types)
enum class TransitionBlend {
    Zoom, Side, Plasma, Cercle, Plasma2, Plasma3, Snail,
    Triangle, Donuts, Corner, Patches, Checkerboard, Bubbles, Stars, Cisor
};

enum class TransitionType { Smooth, Hardcut, Instant };

// ---------------------------------------------------------------------------
// PresetWave — one audio waveform overlay (up to 16 per preset)
// ---------------------------------------------------------------------------
struct PresetWave {
    int   id        = 0;
    bool  enabled   = false;
    int   samples   = 512;
    int   sep       = 0;
    float scaling   = 1.0f;
    float smoothing = 0.5f;
    float r = 1, g = 1, b = 1, a = 1;
    bool  useDots   = false;
    bool  drawThick = false;
    bool  additive  = false;
    std::vector<std::string> perPoint;  // per_point_N= equations
};

// ---------------------------------------------------------------------------
// PresetShape — one geometric shape overlay (up to 16 per preset)
// ---------------------------------------------------------------------------
struct PresetShape {
    int   id           = 0;
    bool  enabled      = false;
    int   sides        = 4;
    bool  additive     = false;
    bool  thickOutline = false;
    bool  textured     = false;
    float x = 0.5f, y = 0.5f;
    float radius  = 0.1f;
    float ang     = 0.0f;
    float tex_ang = 0.0f;
    float tex_zoom = 1.0f;
    float r = 1, g = 1, b = 1, a = 1;
    float r2 = 1, g2 = 1, b2 = 1, a2 = 1;
    float border_r = 1, border_g = 1, border_b = 1, border_a = 0.5f;
    std::vector<std::string> perFrame;  // per_frame_N= equations
};

// ---------------------------------------------------------------------------
// PresetParameters — all parsed variables for one MilkDrop preset
// ---------------------------------------------------------------------------
struct PresetParameters {
    // General
    float rating          = 3.0f;
    float gamma           = 1.0f;
    float videoEchoAlpha  = 0.0f;
    float videoEchoZoom   = 1.0f;
    int   videoEchoOrientation = 0;

    // Warp / feedback
    float warpScale    = 1.0f;
    float warpSpeed    = 1.0f;
    float zoomAmount   = 1.0f;   // 1 = no zoom
    float rotatAmount  = 0.0f;   // radians per frame
    float warpX        = 0.0f;
    float warpY        = 0.0f;
    float centreX      = 0.5f;
    float centreY      = 0.5f;
    float szx          = 1.0f;
    float szy          = 1.0f;
    float decay        = 0.98f;  // feedback decay

    // Ambient color
    float r = 0, g = 0, b = 0, a = 0;

    // Equations
    std::string              perFrameInit;
    std::vector<std::string> perFrame;    // per_frame_N=
    std::vector<std::string> perVertex;   // per_pixel_N=
    std::string              warpHLSL;    // MilkDrop 2+ warp pixel shader (stored, not executed)
    std::string              compHLSL;    // MilkDrop 2+ composite pixel shader (stored, not executed)

    // Legacy global wave (nWaveMode format used by classic presets)
    int   legacyWaveMode    = 0;     // 0-7
    float legacyWaveR       = 1.f;
    float legacyWaveG       = 1.f;
    float legacyWaveB       = 1.f;
    float legacyWaveA       = 0.8f;  // MilkDrop default alpha
    float legacyWaveScale   = 1.f;
    float legacyWaveSmooth  = 0.5f;
    bool  legacyWaveAdditive = false;
    bool  legacyWaveDots    = false;

    // Waves / shapes
    std::vector<PresetWave>  waves;
    std::vector<PresetShape> shapes;

    // Hardcut config embedded in preset
    bool  hardcutEnabled = false;
    float hardcutBass    = 0.8f;
    float hardcutTreble  = 0.5f;
    float hardcutDelay   = 3.0f;

    TransitionBlend blendType = TransitionBlend::Zoom;
};

// ---------------------------------------------------------------------------
// MilkDropPreset — one .milk or .milk2 file
// ---------------------------------------------------------------------------
struct MilkDropPreset {
    std::string path;           // absolute path on disk (empty = unsaved)
    std::string name;           // display name (filename without extension)
    std::string rawData;        // raw .milk text (empty until load() called)
    bool        isDoublePreset = false;  // .milk2
    bool        isFavorite     = false;
    int         rating         = 0;     // 0–5 stars
    std::string author;
    long long   mtime          = 0;     // file modification time (seconds since epoch)

    // Parsed parameters — populated by parseParameters()
    PresetParameters params;
    bool             paramsParsed = false;

    // Load raw text from disk (lazy)
    bool load();

    // Parse rawData → params (calls load() first if rawData is empty)
    void parseParameters();

    std::string fileExtension() const { return isDoublePreset ? "milk2" : "milk"; }
};
