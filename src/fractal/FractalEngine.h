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

    // ── Formula selector (fractal.frag u_formula) ─────────────────────────────
    //  0  z2 + c                (Mandelbrot / Julia classic)
    //  1  sin(z) + c            (Sinus fractal)
    //  2  exp(z) + c            (Exponential spirals)
    //  3  cos(z) + c
    //  4  sinh(z) + c
    //  5  cosh(z) + c
    //  6  Burning Ship          (|re|+i|im|)^2 + c
    //  7  Tricorn               conj(z)^2 + c
    //  8  Newton z3-1           convergence coloring
    //  9  Phoenix               z^2 + Re(c) + Im(c)*z_{n-1}
    // 10  z^n + c               arbitrary real power (uses power field)
    int   formula         = 0;
    float formulaBlend    = 1.0f;  // 0=pure z2+c  1=pure formula

    // ── SDF geometry coupling ─────────────────────────────────────────────────
    float geoWarp         = 0.25f; // 0=no warp  1=max warp

    // ── Euclidean geometry (SDF) ──────────────────────────────────────────────
    int   geoShape        = 0;    // 0=circle 1=polygon 2=star 3=grid
    int   geoSides        = 6;
    float geoRadius       = 0.4f;
    float geoRotation     = 0.0f;
    bool  geoTile         = false;

    // ── 3-D fractal type (mandelbulb.frag u_fractal_3d) ───────────────────────
    //  0  Mandelbulb   (spherical power-n)
    //  1  Mandelbox    (fold + scale IFS)
    //  2  Quaternion Julia  (4-D, 3-D cross-section)
    int   fractal3D       = 0;
    float mbScale         = 2.0f;
    float mbFold          = 1.0f;
};
