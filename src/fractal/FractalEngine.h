#pragma once
#include <glm/glm.hpp>

// Holds all fractal and geometry parameters that map to GLSL uniforms.
struct FractalEngine {
    // Fractal
    glm::vec2 juliaC       = {-0.7f, 0.27f};
    float     power        = 8.0f;          // Mandelbulb power
    int       maxIter      = 128;
    float     bailout      = 4.0f;
    float     zoom         = 1.0f;
    glm::vec2 offset       = {0.0f, 0.0f};

    // Euclidean geometry (SDF)
    int   geoShape    = 0;   // 0=circle 1=polygon 2=star 3=grid
    int   geoSides    = 6;
    float geoRadius   = 0.4f;
    float geoRotation = 0.0f;
    bool  geoTile     = false;
};
