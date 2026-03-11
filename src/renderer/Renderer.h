#pragma once
#include <GL/glew.h>
#include <vector>
#include "ShaderProgram.h"
#include "VideoTexture.h"
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    void init();
    void render(int width, int height, float time,
                const FractalEngine& engine,
                const BlendController& blend,
                const VideoTexture& videoTex);

    // Read back FBO pixels (RGB24) for RTMP encoding.
    // Returns pointer to internal buffer (valid until next call).
    const uint8_t* fboPixels(int width, int height);

private:
    ShaderProgram m_shaderBlend;     // fractal.vert + fractal.frag
    ShaderProgram m_shaderBulb;      // fractal.vert + mandelbulb.frag
    GLuint        m_vao       = 0;
    GLuint        m_fbo       = 0;
    GLuint        m_fboTex    = 0;
    int           m_fboW      = 0;
    int           m_fboH      = 0;
    std::vector<uint8_t> m_pixels;

    void ensureFBO(int w, int h);
    void uploadUniforms(ShaderProgram& prog, int w, int h, float time,
                        const FractalEngine& engine,
                        const BlendController& blend);
};
