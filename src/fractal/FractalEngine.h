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
    //  0  z2 + c                (Mandelbrot / Julia classic)
    //  1  sin(z) + c
    //  2  exp(z) + c
    //  3  cos(z) + c
    //  4  sinh(z) + c
    //  5  cosh(z) + c
    //  6  Burning Ship          (|re|+i|im|)^2 + c
    //  7  Tricorn               conj(z)^2 + c
    //  8  Newton z3-1           convergence coloring
    //  9  Phoenix               z^2 + Re(c) + Im(c)*z_{n-1}
    // 10  z^n + c               arbitrary real power
    int   formula         = 0;     // formula A
    int   formulaB        = 2;     // formula B  (blend slider crossfades A→B)
    float formulaBlend    = 0.0f;  // 0=pure A  1=pure B

    // ── Pixel coordinate injection ────────────────────────────────────────────
    // Adds screen-space p to the iteration seed, making each pixel's position
    // a live variable inside the equation.
    float pixelWeight     = 0.0f;  // 0=off  1=full injection

    // ── Multi-layer repetition ─────────────────────────────────────────────────
    // Runs the same iteration N times with spatially offset starting points
    // and averages the results — creates layered / woven visual depth.
    int   layerCount      = 1;     // 1–4 copies
    float layerOffset     = 0.2f;  // spatial gap between layers

    // ── SDF geometry coupling ─────────────────────────────────────────────────
    float geoWarp         = 0.25f; // 0=no warp  1=max warp

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
    //  0  Mandelbulb   (spherical power-n)
    //  1  Mandelbox    (fold + scale IFS)
    //  2  Quaternion Julia  (4-D, 3-D cross-section)
    int   fractal3D       = 0;
    float mbScale         = 2.0f;
    float mbFold          = 1.0f;
};
