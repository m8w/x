#pragma once
#include "../gl_includes.h"
#include "../renderer/ShaderProgram.h"
#include "MilkDropPreset.h"
#include "EquationEvaluator.h"
#include "../audio/IAudioCapture.h"
#include <string>
#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
// MilkDropGLRenderer — OpenGL port of MilkDropRenderer.swift (Metal)
//
// Render pipeline (called once per frame from main.cpp):
//   1. Warp pass  : distort ping-pong feedback texture → warpWrite FBO
//   2. Wave pass  : render audio waveform lines → wave FBO
//   3. Shape pass : render preset shapes (triangle fans) → shape FBO
//   4. Composite  : warp + wave + shape + optional fractal → output FBO
//   5. Blend      : if transitioning, blend outgoing/incoming composites
//
// The fractal renderer's FBO texture is passed in each frame as
// fractalTexture — zero coupling to the existing Renderer class.
// ---------------------------------------------------------------------------

class MilkDropGLRenderer {
public:
    MilkDropGLRenderer()  = default;
    ~MilkDropGLRenderer();

    // Call once after OpenGL context is current.
    void init(const std::string& shadersDir);

    // Call when window/FBO size changes.
    void resize(int w, int h);

    // Load a preset (compiles equations, resets evaluator).
    void loadPreset(const MilkDropPreset& preset);

    // Begin a transition to a new preset.
    void beginTransition(const MilkDropPreset& next,
                         int blendType     = 0,   // TransitionBlend enum value
                         float duration    = 2.5f);

    // Render one frame. fractalTex may be 0 if fractal overlay is disabled.
    void render(float time, float dt,
                const AudioData& audio,
                GLuint fractalTex,
                float fractalBlend = 0.4f);

    // Output texture (the final composite — bind as input to RTMP readback).
    GLuint outputTexture() const { return m_outputTex; }
    GLuint outputFbo()     const { return m_outFbo; }

    // Blit the output FBO directly to the default framebuffer (the window).
    void blitToScreen(int w, int h);

    // True if at least one preset has been loaded.
    bool hasPreset() const { return m_hasPreset; }

    // CPU readback of the output FBO for RTMP encode.
    // Returns a pointer to an internal RGB buffer (valid until next call).
    const uint8_t* readPixels(int w, int h);

    // True once init() succeeded.
    bool isReady() const { return m_ready; }

    // Control fractal overlay
    bool  fractalEnabled = false;

private:
    void createFBOs(int w, int h);
    void destroyFBOs();

    GLuint makeFBO(GLuint& tex, int w, int h, bool withAlpha = false);
    void   renderFullscreenQuad();
    void   clearFBO(GLuint fbo, float r, float g, float b, float a);

    void renderWarpPass   (float time);
    void renderWavePass   (const AudioData& audio, float time);
    void renderShapePass  ();
    void renderComposite  (GLuint fractalTex, float fractalBlend);
    void renderBlendPass  ();

    // Shader programs (all use fractal.vert for fullscreen passes)
    ShaderProgram m_warpShader;
    ShaderProgram m_waveShader;
    ShaderProgram m_shapeShader;
    ShaderProgram m_compositeShader;
    ShaderProgram m_blendShader;

    // FBO + texture pairs (ping-pong warp, wave, shape, output, transition)
    GLuint m_warpFboA = 0, m_warpTexA = 0;
    GLuint m_warpFboB = 0, m_warpTexB = 0;
    GLuint m_waveFbo  = 0, m_waveTex  = 0;
    GLuint m_shapeFbo = 0, m_shapeTex = 0;
    GLuint m_outFbo   = 0, m_outputTex = 0;
    GLuint m_blendFbo = 0, m_blendTex  = 0;
    bool   m_pingPong = false;

    // Empty VAO for fullscreen quad passes (core profile requires a VAO bound)
    GLuint m_quadVAO = 0;

    // Wave / shape geometry VAO + VBO (dynamic vertex data)
    GLuint m_waveVAO = 0, m_waveVBO = 0;
    GLuint m_shapeVAO = 0, m_shapeVBO = 0;

    // Equation evaluator
    EquationEvaluator m_evaluator;
    MilkDropUniforms  m_uniforms;

    // Current and next preset
    MilkDropPreset m_current;
    MilkDropPreset m_next;
    bool  m_hasPreset = false;

    // Transition state
    bool  m_transitioning      = false;
    float m_transitionProgress = 0.f;
    float m_transitionDuration = 2.5f;
    int   m_blendType          = 0;

    // During transition: the two capture textures to blend between
    GLuint m_transA = 0, m_transB = 0;

    int  m_w = 0, m_h = 0;
    bool m_ready = false;

    std::string m_shadersDir;

    // CPU readback buffer (resized on demand)
    std::vector<uint8_t> m_readPixBuf;
};
