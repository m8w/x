#pragma once
#include <glm/glm.hpp>

// Holds all fractal and geometry parameters that map to GLSL uniforms.
struct FractalEngine {
    // ── View ──────────────────────────────────────────────────────────────────
    float     zoom        = 1.0f;
    glm::vec2 offset      = {0.0f, 0.0f};

    // ── Iteration ─────────────────────────────────────────────────────────────
    glm::vec2 juliaC      = {-0.7f, 0.27f};
    float     power       = 8.0f;     // Mandelbulb power / z^n exponent
    int       maxIter     = 128;
    float     bailout     = 4.0f;

    // ── Formula A + B cross-blend (fractal.frag u_formula / u_formula_b) ──────
    //  0  z² + c                (Mandelbrot / Julia classic)
    //  1  sin(z) + c
    //  2  exp(z) + c
    //  3  cos(z) + c
    //  4  sinh(z) + c
    //  5  cosh(z) + c
    //  6  Burning Ship          (|re|+i|im|)² + c
    //  7  Tricorn               conj(z)² + c
    //  8  Newton z³−1           convergence coloring
    //  9  Phoenix               z² + Re(c) + Im(c)·z_{n-1}
    // 10  z^n + c               arbitrary real power  (uses u_power)
    // 11  tan(z) + c            complex tangent
    // 12  z·e^z + c             exponential-polynomial
    // 13  Celtic                (|Re(z²)|, Im(z²)) + c
    // 14  Magnet I              ((z²+c−1)/(2z+c−2))²
    // 15  z^z + c               complex self-power
    // 16  Manowar               z² + z_{n-1} + c   (direct memory)
    // 17  Perp Burning Ship     (Re(z), |Im(z)|)² + c
    // 18  Time-spiral           z²·e^(i·param·t) + c  (u_formula_param driven)
    // 19  Cubic+linear          z³ + z + c
    // 20  Cosh-conjugate        cosh(conj(z)) + c
    // 21  Polar→Cartesian warp  map z via polar coords before squaring + c
    int   formula         = 0;     // formula A  (0–21)
    int   formulaB        = 2;     // formula B  (blend slider crossfades A→B)
    float formulaBlend    = 0.0f;  // 0=pure A  1=pure B

    // ── Formula extra parameter (u_formula_param) ─────────────────────────────
    // Free parameter exposed to all formulas.  Formula 18 uses it as
    // rotation speed (rad/s).  Formula 21 uses it as polar warp strength.
    // Can be animated from the UI (linked to u_time * speed).
    float formulaParam    = 1.0f;

    // ── Pixel coordinate injection ────────────────────────────────────────────
    float pixelWeight     = 0.0f;  // 0=off  1=full injection

    // ── Multi-layer repetition ─────────────────────────────────────────────────
    int   layerCount      = 1;     // 1–4 copies
    float layerOffset     = 0.2f;

    // ── SDF geometry coupling ─────────────────────────────────────────────────
    float geoWarp         = 0.25f;

    // ── Euclidean geometry (SDF) ──────────────────────────────────────────────
    int   geoShape        = 0;    // 0=circle 1=polygon 2=star 3=grid
    int   geoSides        = 6;
    float geoRadius       = 0.4f;
    float geoRotation     = 0.0f;
    bool  geoTile         = false;

    // ── Mirror / kaleidoscope ─────────────────────────────────────────────────
    int   geoMirror       = 0;    // 0=none 1=X 2=Y 3=XY
    int   geoKaleid       = 0;    // 0=off  N=segment count (2–16)

    // ── 3-D fractal type (mandelbulb.frag u_fractal_3d) ───────────────────────
    // 0=Mandelbulb  1=Mandelbox  2=Quaternion Julia  3=3D Complex (Peng)  4=Kleinian Limit Set
    int   fractal3D       = 0;
    float mbScale         = 2.0f;
    float mbFold          = 1.0f;
    // Kleinian-only: sphere-inversion radius^2 threshold (shares mbScale/mbFold above)
    float kleinianMinR2   = 0.3f;

    // ── Chaos domain warp (fractal.frag) ─────────────────────────────────────
    // Applied as a pre-iteration UV warp so chaos geometry is baked into
    // the fractal structure rather than added as a post-process.
    //   0  off
    //   1  Turbulence — two-level fBm smooth chaotic flow
    //   2  Logistic   — logistic-map orbit drives a rotation warp (r→4 = chaos)
    //   3  Hénon      — Hénon attractor (a=1.4, b=0.3) displacement
    //   4  Shred      — multi-freq scanline horizontal drift
    int   chaosMode     = 0;
    float chaosStrength = 0.5f;   // warp amplitude  (0–1)
    float chaosScale    = 2.0f;   // spatial frequency (0.5–8)
    float chaosSpeed    = 1.0f;   // time modulation rate (0–3)

    // ── Overlay video blend ───────────────────────────────────────────────────
    // 0 = fractal only  1 = overlay only  0.5 = 50/50 mix (visual + audio)
    float overlayBlend    = 0.5f;

    // ── Video filters (GIMP-inspired) ─────────────────────────────────────────
    // Color filters 0-11 apply to both streams.
    // Spatial filters 12-18 apply to overlay only (has screen UV).
    int   vidFilter  = 0;       // primary video filter ID
    float vidFilterA = 1.0f;    // param A (brightness/saturation/levels/strength)
    float vidFilterB = 1.0f;    // param B (contrast/frequency/radius)

    int   ovrFilter  = 0;       // overlay filter ID
    float ovrFilterA = 1.0f;    // overlay filter param A
    float ovrFilterB = 1.0f;    // overlay filter param B

    // ── Stream blend mode ─────────────────────────────────────────────────────
    // 0=Normal  1=Multiply  2=Screen  3=Overlay  4=SoftLight  5=HardLight
    // 6=Difference  7=Exclusion  8=ColorDodge  9=ColorBurn  10=Darken  11=Lighten  12=Addition
    int   streamBlendMode = 0;

    // ── Buddhabrot / Nebulabrot (buddhabrot_*.frag/.geom) ───────────────────────
    // When true, Renderer switches entirely to the GPU point-cloud orbit-
    // accumulation pipeline, bypassing the 2-D blend / 3-D raymarch / distortion
    // paths — the same kind of exclusive mode switch as distortionMode above.
    bool  buddhabrotMode = false;
    bool  nebulabrot      = false;  // false=single-channel density  true=RGB 3-threshold composite

    // Samples (random c values) launched per frame. Each sample costs up to
    // 2x its iteration cap in complex iterations on the GPU (one pass to find
    // the escape step, one to emit points up to it) — the main perf/quality dial.
    int   buddhaSamples  = 20000;

    // Iteration caps, hard-capped at 128 by the geometry shader's
    // max_vertices x per-vertex-output-component budget (1024 components is
    // the GL-guaranteed minimum across drivers; 128 verts x ~7 components
    // stays safely under it). Nebulabrot treats all three as independent
    // escape thresholds — R=longest/rarest filigree, G=mid, B=shortest/
    // broadest structure. Buddhabrot uses R alone.
    int   buddhaIterR   = 120;
    int   buddhaIterG   = 50;
    int   buddhaIterB   = 15;
    int   buddhaMinIter = 2;      // skip orbits that escape before this step (cuts boring near-field points)

    // Accumulation decay: the buffer is multiplied by this every frame before
    // new points are added, so density fades instead of freezing into a
    // static long-exposure photo — keeps the image alive on a live stream
    // instead of converging to a fixed picture. 1.0 = never decays.
    float buddhaDecay      = 0.985f;
    float buddhaBrightness = 1.0f;   // tone-map gain before display
    float buddhaGamma      = 0.45f;  // tone-map curve exponent (log-ish compression)

    // ── Hybrid combination iteration ─────────────────────────────────────────
    // Cycles the orbit through up to 4 formulas every buddhaHybridCycle
    // iterations instead of iterating pure Mandelbrot z²+c the whole way.
    // Formula IDs: 0=Mandelbrot  1=Burning Ship  2=Tricorn  3=Celtic
    bool  buddhaHybrid        = false;
    int   buddhaHybridList[4] = {0, 1, 2, 3};
    int   buddhaHybridCount   = 2;   // how many of buddhaHybridList[] cycle through
    int   buddhaHybridCycle   = 5;   // switch formula every N iterations

    // Set true to clear the accumulation buffer on the next render. Mutable
    // so it can be consumed (and cleared) by Renderer::render() even though
    // that method takes FractalEngine by const reference.
    mutable bool buddhaResetRequested = true;

    // ── Flame / IFS Chaos-Game (flame_*.vert/.geom/.frag) ───────────────────────
    // When true, Renderer switches to the GPU chaos-game point-cloud pipeline —
    // same exclusive-mode pattern as buddhabrotMode/distortionMode, and shares
    // its decay/ping-pong/display-pass architecture (RGBA32F: RGB=summed
    // color, A=hit count, vs. Buddhabrot's plain RGB32F density).
    bool  flameMode    = false;
    int   flameSamples = 20000;   // chaos-game walks launched per frame
    int   flameSteps   = 60;      // plotted iterations per walk, capped at 128 (GS max_vertices budget)
    int   flameBurnIn  = 20;      // discarded warm-up iterations before plotting starts

    float flameDecay      = 0.985f;  // per-frame buffer fade, same meaning as buddhaDecay
    float flameBrightness = 1.0f;    // log-density tone-map gain
    float flameGamma      = 0.6f;    // post-curve shaping on top of the log tone map

    // Up to 4 affine+variation "xforms" (flam3 terminology). Each step of the
    // chaos game picks one at random, weighted by flameWeight[] (normalised
    // before upload), applies its affine transform then its variation, and
    // blends the running color coordinate toward flameColor[] — the classic
    // fractal-flame recipe. Variation IDs: 0=Linear 1=Sinusoidal 2=Spherical
    // 3=Swirl 4=Horseshoe 5=Polar 6=Handkerchief 7=Spiral.
    // Default: a 4-corner contraction IFS with varied variations per slot —
    // a reasonable non-trivial starting point, fully tunable from the UI.
    float flameA[4] = { 0.5f,  0.5f, -0.5f,  0.5f};
    float flameB[4] = { 0.0f,  0.0f,  0.0f,  0.0f};
    float flameC[4] = { 0.0f,  0.0f,  0.0f,  0.0f};
    float flameD[4] = { 0.5f,  0.5f,  0.5f, -0.5f};
    float flameE[4] = { 0.0f,  0.5f,  0.5f,  0.0f};
    float flameF[4] = { 0.0f,  0.5f,  0.0f, -0.5f};
    int   flameVariation[4] = {1, 3, 6, 2};
    float flameWeight[4]    = {0.25f, 0.25f, 0.25f, 0.25f};
    float flameColor[4]     = {0.0f, 0.33f, 0.66f, 1.0f};
    int   flameXformCount   = 4;   // how many of the 4 slots are active (1-4)

    mutable bool flameResetRequested = true;

    // ── Quaternion Feedback (quat_feedback.frag) ────────────────────────────────
    // A post-process feedback-trail pass, NOT an exclusive mode — it layers on
    // top of whatever is already on screen (2-D blend, raymarch, Buddhabrot,
    // Flame, even Distortion), chained right after the spectral pass. Each
    // frame the previous feedback frame is re-sampled at a UV offset driven
    // by a 3-D quaternion rotation (two independent rotation rates composed
    // non-commutatively), giving a braided, self-crossing trail instead of a
    // flat 2-D spin. "Real"/"imaginary" rate naming is evocative, not a
    // physics claim — see CLAUDE.md for the honest translation notes.
    bool  quatFeedbackEnabled = false;
    float quatFeedbackAmount  = 0.5f;    // 0=fractal only  1=feedback trail dominates
    float quatFeedbackDecay   = 0.96f;   // per-frame trail fade
    float quatFeedbackWarp    = 0.02f;   // UV-offset magnitude from the rotation
    float quatRealRate = 1.0f;    // "real time" rotation rate (rad/s)
    float quatImagRate = 0.37f;   // "imaginary time" rotation rate (rad/s) — deliberately
                                   // a non-integer ratio to quatRealRate so the combined
                                   // rotation axis drifts instead of repeating
    glm::vec3 quatAxis = {0.41f, 0.71f, 0.58f};  // i,j,k axis weights (normalised in-shader)

    // ── Distortion mode (distortion.frag) ────────────────────────────────────
    // When true, the Renderer switches to the iridescent metaball shader and
    // bypasses the fractal blend pipeline entirely.
    bool  distortionMode = false;
    float distortSpeed   = 1.0f;   // animation rate multiplier (0.1–3)
    int   distortBlobs   = 6;      // active metaball count (3–10)
    float distortGlow    = 1.0f;   // outer aura intensity (0–2)
    float distortIrid    = 2.0f;   // rainbow cycle frequency (0.5–4)
    float distortOutline = 0.6f;   // edge ring brightness (0–1)
};
