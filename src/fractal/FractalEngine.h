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
    int   fractal3D       = 0;
    float mbScale         = 2.0f;
    float mbFold          = 1.0f;
};
