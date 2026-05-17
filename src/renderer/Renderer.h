#pragma once
#include "gl_includes.h"
#include <vector>
#include "ShaderProgram.h"
#include "VideoTexture.h"
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include "fractal/ColorSynth.h"

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    void init();
    void render(int width, int height, float time,
                const FractalEngine& engine,
                const BlendController& blend,
                const VideoTexture& videoTex,
                const VideoTexture& overlayTex,
                const ColorSynth& colorSynth);

    // Read back FBO pixels (RGB24) for RTMP encoding.
    // Returns pointer to internal buffer (valid until next call).
    const uint8_t* fboPixels(int width, int height);

    // Set FFT/AFT spectral post-process parameters.
    // Call once per frame before render(); params are used in the same call.
    // enabled  = show spectral distortion in preview
    // onStream = also send distorted output to stream (use spectral FBO for encoding)
    void setSpectralParams(bool enabled, bool onStream,
                           const float band[4], const float visGain[4]);

private:
    ShaderProgram m_shaderBlend;      // fractal.vert + fractal.frag
    ShaderProgram m_shaderBulb;       // fractal.vert + mandelbulb.frag
    ShaderProgram m_shaderDistort;    // fractal.vert + distortion.frag
    ShaderProgram m_shaderSpectral;   // fractal.vert + spectral.frag
    GLuint        m_vao       = 0;
    GLuint        m_fbo       = 0;
    GLuint        m_fboTex    = 0;
    int           m_fboW      = 0;
    int           m_fboH      = 0;
    // Spectral post-process FBO (ping-pong target)
    GLuint        m_spectralFbo = 0;
    GLuint        m_spectralTex = 0;
    int           m_spectralW   = 0;
    int           m_spectralH   = 0;
    // Spectral params set by setSpectralParams() each frame
    bool          m_spectralEnabled  = false;
    bool          m_spectralOnStream = true;
    float         m_fftBand[4]       = {0.0f, 0.0f, 0.0f, 0.0f};
    float         m_fftVisGain[4]    = {1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<uint8_t> m_pixels;

    // Double-buffered PBOs for async glReadPixels (no GPU pipeline stall)
    GLuint m_pbo[2]  = {0, 0};
    int    m_pboIdx  = 0;
    int    m_pboW    = 0;
    int    m_pboH    = 0;

    void ensureFBO(int w, int h);
    void ensureSpectralFBO(int w, int h);
    void uploadUniforms(ShaderProgram& prog, int w, int h, float time,
                        const FractalEngine& engine,
                        const BlendController& blend,
                        const ColorSynth& colorSynth);
};
