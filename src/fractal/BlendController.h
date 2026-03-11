#pragma once

struct BlendController {
    float mandelbrot = 1.0f;
    float julia      = 0.0f;
    float mandelbulb = 0.0f;
    float euclidean  = 0.0f;

    // Returns the 4-component array for uploading as a uniform
    void weights(float out[4]) const {
        float sum = mandelbrot + julia + mandelbulb + euclidean + 1e-6f;
        out[0] = mandelbrot / sum;
        out[1] = julia      / sum;
        out[2] = mandelbulb / sum;
        out[3] = euclidean  / sum;
    }
};
