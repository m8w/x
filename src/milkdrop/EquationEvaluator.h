#pragma once
#include "MilkDropPreset.h"
#include "../audio/IAudioCapture.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// MilkDropUniforms — the complete per-frame variable table
//
// Populated by EquationEvaluator::evaluate() each frame.
// Passed to MilkDropGLRenderer (Phase 4) to set GLSL uniforms.
// ---------------------------------------------------------------------------
struct MilkDropUniforms {
    // View transform (written by per_frame equations)
    float zoom  = 1.0f;   // zoom (1 = none)
    float rot   = 0.0f;   // rotation radians/frame
    float warp  = 0.0f;   // warp amount
    float cx    = 0.5f;   // warp centre x
    float cy    = 0.5f;   // warp centre y
    float dx    = 0.0f;   // warp translation x
    float dy    = 0.0f;   // warp translation y
    float sx    = 1.0f;   // scale x
    float sy    = 1.0f;   // scale y
    float decay = 0.98f;  // feedback decay

    // Color
    float gamma = 1.0f;
    float r = 0, g = 0, b = 0, a = 0;   // ambient

    // Echo
    float videoEchoAlpha       = 0.0f;
    float videoEchoZoom        = 1.0f;
    int   videoEchoOrientation = 0;

    // Audio (read-only in equations, written from AudioData each frame)
    float bass   = 0, mid    = 0, treble  = 0, vol     = 0;
    float bass_att = 0, mid_att = 0, treble_att = 0, vol_att = 0;

    // Time
    float time  = 0;
    float fps   = 60.0f;
    float frame = 0;

    // Transition blend progress (0→1)
    float progress = 0.0f;

    // q-variables (q1–q32, indices 0–31)
    float q[32]{};

    // Warp / composite HLSL (forwarded to GPU — stored, not evaluated by CPU)
    // Nothing here for now; added in Phase 4 when the renderer needs them.
};

// ---------------------------------------------------------------------------
// EquationEvaluator — wraps projectm-eval to run MilkDrop preset equations
//
// Usage:
//   evaluator.loadPreset(preset);         // compile all equation strings
//   evaluator.evaluate(uniforms, audio, time, fps, frame);  // each frame
// ---------------------------------------------------------------------------
class EquationEvaluator {
public:
    EquationEvaluator();
    ~EquationEvaluator();

    // Compile all equation strings from the preset.
    // Safe to call mid-stream — tears down old context first.
    void loadPreset(const MilkDropPreset& preset);

    // Run per-frame equations and update uniforms.
    // Call once per render frame after loadPreset().
    void evaluate(MilkDropUniforms& uniforms,
                  const AudioData& audio,
                  float time, float fps, float frame);

    // True once loadPreset() has successfully compiled at least one equation.
    bool isReady() const { return m_ready; }

private:
    void destroyContext();
    void buildContext(const PresetParameters& params);

    // Opaque projectm-eval handles
    struct projectm_eval_context* m_ctx  = nullptr;
    struct projectm_eval_code*    m_init = nullptr;  // per_frame_init
    struct projectm_eval_code*    m_perFrame = nullptr;

    // Variable pointers — registered once, written/read every frame
    // View
    float* m_zoom  = nullptr; float* m_rot   = nullptr;
    float* m_warp  = nullptr; float* m_cx    = nullptr;
    float* m_cy    = nullptr; float* m_dx    = nullptr;
    float* m_dy    = nullptr; float* m_sx    = nullptr;
    float* m_sy    = nullptr; float* m_decay = nullptr;
    // Color
    float* m_gamma = nullptr;
    float* m_r     = nullptr; float* m_g = nullptr;
    float* m_b     = nullptr; float* m_a = nullptr;
    // Audio (inputs)
    float* m_bass      = nullptr; float* m_mid       = nullptr;
    float* m_treble    = nullptr; float* m_vol       = nullptr;
    float* m_bass_att  = nullptr; float* m_mid_att   = nullptr;
    float* m_treble_att= nullptr; float* m_vol_att   = nullptr;
    // Time
    float* m_time  = nullptr; float* m_fps   = nullptr;
    float* m_frame = nullptr; float* m_progress = nullptr;
    // q-variables
    float* m_q[32]{};

    bool m_ready = false;
    bool m_initRan = false;   // per_frame_init runs exactly once per loadPreset()
};
