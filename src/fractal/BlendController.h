#pragma once

struct BlendController {
    float mandelbrot = 1.0f;
    float julia      = 0.0f;
    float mandelbulb = 0.0f;
    float euclidean  = 0.0f;
    float diff       = 0.0f;  // 5th blend: de Jong differential flow field

    // Returns the 5-component normalised weight array for uploading as uniforms
    void weights(float out[5]) const {
        float sum = mandelbrot + julia + mandelbulb + euclidean + diff + 1e-6f;
        out[0] = mandelbrot / sum;
        out[1] = julia      / sum;
        out[2] = mandelbulb / sum;
        out[3] = euclidean  / sum;
        out[4] = diff       / sum;
    }
};
