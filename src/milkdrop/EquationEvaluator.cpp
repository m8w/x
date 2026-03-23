// EquationEvaluator.cpp — wraps projectm-eval for MilkDrop per-frame equations

#include "EquationEvaluator.h"

// PRJM_F_SIZE=4 is set via CMake target_compile_definitions on projectM_eval (PUBLIC),
// so PRJM_EVAL_F == float when this file is compiled.
#include <projectm-eval/api/projectm-eval.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <sstream>

// Required stubs — single-threaded, no locking needed
void projectm_eval_memory_host_lock_mutex()   {}
void projectm_eval_memory_host_unlock_mutex() {}

// ---------------------------------------------------------------------------

EquationEvaluator::EquationEvaluator()  = default;
EquationEvaluator::~EquationEvaluator() { destroyContext(); }

void EquationEvaluator::destroyContext() {
    if (m_perVertex) { projectm_eval_code_destroy(m_perVertex); m_perVertex = nullptr; }
    if (m_perFrame) { projectm_eval_code_destroy(m_perFrame); m_perFrame = nullptr; }
    if (m_init)     { projectm_eval_code_destroy(m_init);     m_init     = nullptr; }
    if (m_ctx)      { projectm_eval_context_destroy(m_ctx);   m_ctx      = nullptr; }
    m_ready    = false;
    m_initRan  = false;
    std::memset(m_q, 0, sizeof(m_q));
}

// ---------------------------------------------------------------------------
// loadPreset — compile all equation strings and register variables
// ---------------------------------------------------------------------------

static PRJM_EVAL_F* reg(struct projectm_eval_context* ctx, const char* name) {
    return projectm_eval_context_register_variable(ctx, name);
}

void EquationEvaluator::buildContext(const PresetParameters& params) {
    m_ctx = projectm_eval_context_create(nullptr, nullptr);
    if (!m_ctx) {
        fprintf(stderr, "[EquationEvaluator] Failed to create context\n");
        return;
    }

    // ── Register all standard MilkDrop variables ──────────────────────────
    m_zoom  = reg(m_ctx, "zoom");  m_rot   = reg(m_ctx, "rot");
    m_warp  = reg(m_ctx, "warp");  m_cx    = reg(m_ctx, "cx");
    m_cy    = reg(m_ctx, "cy");    m_dx    = reg(m_ctx, "dx");
    m_dy    = reg(m_ctx, "dy");    m_sx    = reg(m_ctx, "sx");
    m_sy    = reg(m_ctx, "sy");    m_decay = reg(m_ctx, "decay");
    m_gamma = reg(m_ctx, "gamma");
    m_r     = reg(m_ctx, "r");     m_g = reg(m_ctx, "g");
    m_b     = reg(m_ctx, "b");     m_a = reg(m_ctx, "a");

    m_bass       = reg(m_ctx, "bass");      m_mid       = reg(m_ctx, "mid");
    m_treble     = reg(m_ctx, "treble");    m_vol       = reg(m_ctx, "vol");
    m_bass_att   = reg(m_ctx, "bass_att");  m_mid_att   = reg(m_ctx, "mid_att");
    m_treble_att = reg(m_ctx, "treb_att");  m_vol_att   = reg(m_ctx, "vol_att");

    m_time     = reg(m_ctx, "time");
    m_fps      = reg(m_ctx, "fps");
    m_frame    = reg(m_ctx, "frame");
    m_progress = reg(m_ctx, "progress");

    for (int i = 0; i < 32; ++i) {
        std::string name = "q" + std::to_string(i + 1);
        m_q[i] = reg(m_ctx, name.c_str());
    }

    // ── Join per-frame equations into one code string ─────────────────────
    // MilkDrop separates equations with semicolons; projectm-eval expects that.
    auto joinEqs = [](const std::vector<std::string>& eqs) -> std::string {
        std::ostringstream ss;
        for (const auto& eq : eqs) {
            if (!eq.empty()) ss << eq << ";";
        }
        return ss.str();
    };

    // per_frame_init — runs once on preset load
    if (!params.perFrameInit.empty()) {
        m_init = projectm_eval_code_compile(m_ctx, params.perFrameInit.c_str());
        if (!m_init) {
            int line, col;
            fprintf(stderr, "[EquationEvaluator] per_frame_init compile error: %s (L%d C%d)\n",
                    projectm_eval_get_error(m_ctx, &line, &col), line, col);
        }
    }

    // per_frame — runs every frame
    std::string perFrameCode = joinEqs(params.perFrame);
    if (!perFrameCode.empty()) {
        m_perFrame = projectm_eval_code_compile(m_ctx, perFrameCode.c_str());
        if (!m_perFrame) {
            int line, col;
            fprintf(stderr, "[EquationEvaluator] per_frame compile error: %s (L%d C%d)\n",
                    projectm_eval_get_error(m_ctx, &line, &col), line, col);
        }
    }

    // per_pixel (per-vertex) — runs once per mesh vertex
    std::string perVertexCode = joinEqs(params.perVertex);
    if (!perVertexCode.empty()) {
        m_perVertex = projectm_eval_code_compile(m_ctx, perVertexCode.c_str());
        if (!m_perVertex) {
            int line, col;
            fprintf(stderr, "[EquationEvaluator] per_pixel compile error: %s (L%d C%d)\n",
                    projectm_eval_get_error(m_ctx, &line, &col), line, col);
        }
    }
    // Register per-vertex position variables
    m_x   = reg(m_ctx, "x");
    m_y   = reg(m_ctx, "y");
    m_rad = reg(m_ctx, "rad");
    m_ang = reg(m_ctx, "ang");
    m_ready = (m_init != nullptr || m_perFrame != nullptr || m_perVertex != nullptr);
}

void EquationEvaluator::loadPreset(const MilkDropPreset& preset) {
    destroyContext();

    // Ensure parameters are parsed
    MilkDropPreset mutable_preset = preset;
    mutable_preset.parseParameters();

    buildContext(mutable_preset.params);

    // Seed view variables from preset defaults
    if (m_ctx) {
        const auto& p = mutable_preset.params;
        if (m_zoom)  *m_zoom  = p.zoomAmount;
        if (m_rot)   *m_rot   = p.rotatAmount;
        if (m_warp)  *m_warp  = p.warpScale;
        if (m_cx)    *m_cx    = p.centreX;
        if (m_cy)    *m_cy    = p.centreY;
        if (m_sx)    *m_sx    = p.szx;
        if (m_sy)    *m_sy    = p.szy;
        if (m_decay) *m_decay = p.decay;
        if (m_gamma) *m_gamma = p.gamma;
        if (m_r)     *m_r     = p.r;
        if (m_g)     *m_g     = p.g;
        if (m_b)     *m_b     = p.b;
        if (m_a)     *m_a     = p.a;
    }
}

// ---------------------------------------------------------------------------
// evaluate — run per-frame equations, then copy results into uniforms
// ---------------------------------------------------------------------------

void EquationEvaluator::evaluate(MilkDropUniforms& u,
                                  const AudioData& audio,
                                  float time, float fps, float frame) {
    if (!m_ctx) return;

    // ── Write inputs ──────────────────────────────────────────────────────
    if (m_bass)        *m_bass        = audio.bass;
    if (m_mid)         *m_mid         = audio.mid;
    if (m_treble)      *m_treble      = audio.treble;
    if (m_vol)         *m_vol         = audio.rms;
    if (m_bass_att)    *m_bass_att    = audio.bassAttn;
    if (m_mid_att)     *m_mid_att     = audio.mid;      // use smoothed mid as att
    if (m_treble_att)  *m_treble_att  = audio.treble;
    if (m_vol_att)     *m_vol_att     = audio.rms;
    if (m_time)        *m_time        = (float)time;
    if (m_fps)         *m_fps         = (float)fps;
    if (m_frame)       *m_frame       = (float)frame;
    if (m_progress)    *m_progress    = (float)u.progress;

    // ── Run per_frame_init exactly once ───────────────────────────────────
    if (m_init && !m_initRan) {
        projectm_eval_code_execute(m_init);
        m_initRan = true;
    }

    // ── Run per_frame ─────────────────────────────────────────────────────
    if (m_perFrame) {
        projectm_eval_code_execute(m_perFrame);
    }

    // ── Read results back into uniforms ───────────────────────────────────
    if (m_zoom)  u.zoom  = *m_zoom;
    if (m_rot)   u.rot   = *m_rot;
    if (m_warp)  u.warp  = *m_warp;
    if (m_cx)    u.cx    = *m_cx;
    if (m_cy)    u.cy    = *m_cy;
    if (m_dx)    u.dx    = *m_dx;
    if (m_dy)    u.dy    = *m_dy;
    if (m_sx)    u.sx    = *m_sx;
    if (m_sy)    u.sy    = *m_sy;
    if (m_decay) u.decay = *m_decay;
    if (m_gamma) u.gamma = *m_gamma;
    if (m_r)     u.r     = *m_r;
    if (m_g)     u.g     = *m_g;
    if (m_b)     u.b     = *m_b;
    if (m_a)     u.a     = *m_a;

    for (int i = 0; i < 32; ++i)
        if (m_q[i]) u.q[i] = *m_q[i];

    // Audio passthrough (read-only in equations, still expose in uniforms)
    u.bass   = audio.bass;  u.mid    = audio.mid;
    u.treble = audio.treble; u.vol   = audio.rms;
    u.time   = time;  u.fps = fps;  u.frame = frame;
}

// ---------------------------------------------------------------------------
// restoreContextFromUniforms — called after all mesh vertices are evaluated
// ---------------------------------------------------------------------------

void EquationEvaluator::restoreContextFromUniforms(const MilkDropUniforms& u) {
    if (!m_ctx) return;
    if (m_zoom) *m_zoom = u.zoom;
    if (m_rot)  *m_rot  = u.rot;
    if (m_warp) *m_warp = u.warp;
    if (m_cx)   *m_cx   = u.cx;
    if (m_cy)   *m_cy   = u.cy;
    if (m_dx)   *m_dx   = u.dx;
    if (m_dy)   *m_dy   = u.dy;
    if (m_sx)   *m_sx   = u.sx;
    if (m_sy)   *m_sy   = u.sy;
    if (m_decay)*m_decay= u.decay;
    if (m_gamma)*m_gamma= u.gamma;
    if (m_r)    *m_r    = u.r;
    if (m_g)    *m_g    = u.g;
    if (m_b)    *m_b    = u.b;
    if (m_a)    *m_a    = u.a;
}

// ---------------------------------------------------------------------------
// evaluateVertex — run per_pixel equations for one mesh vertex
// ---------------------------------------------------------------------------

void EquationEvaluator::evaluateVertex(float vx, float vy,
                                        const MilkDropUniforms& globals,
                                        VertexParams& out) {
    // Always start with globals
    out = { globals.zoom, globals.rot, globals.warp,
            globals.cx,   globals.cy,  globals.dx,
            globals.dy,   globals.sx,  globals.sy  };

    if (!m_ctx || !m_perVertex) return;

    // Seed view variables to per-frame values so equations start from there
    if (m_zoom) *m_zoom = globals.zoom;
    if (m_rot)  *m_rot  = globals.rot;
    if (m_warp) *m_warp = globals.warp;
    if (m_cx)   *m_cx   = globals.cx;
    if (m_cy)   *m_cy   = globals.cy;
    if (m_dx)   *m_dx   = globals.dx;
    if (m_dy)   *m_dy   = globals.dy;
    if (m_sx)   *m_sx   = globals.sx;
    if (m_sy)   *m_sy   = globals.sy;

    // Set per-vertex position inputs (MilkDrop convention: [-1,1])
    float x = vx * 2.0f - 1.0f;
    float y = vy * 2.0f - 1.0f;
    if (m_x)   *m_x   = x;
    if (m_y)   *m_y   = y;
    if (m_rad) *m_rad = sqrtf(x*x + y*y);
    if (m_ang) *m_ang = atan2f(y, x);

    projectm_eval_code_execute(m_perVertex);

    // Read back modified values
    if (m_zoom) out.zoom = *m_zoom;
    if (m_rot)  out.rot  = *m_rot;
    if (m_warp) out.warp = *m_warp;
    if (m_cx)   out.cx   = *m_cx;
    if (m_cy)   out.cy   = *m_cy;
    if (m_dx)   out.dx   = *m_dx;
    if (m_dy)   out.dy   = *m_dy;
    if (m_sx)   out.sx   = *m_sx;
    if (m_sy)   out.sy   = *m_sy;
}
