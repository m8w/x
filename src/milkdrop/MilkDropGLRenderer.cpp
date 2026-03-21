// MilkDropGLRenderer.cpp — OpenGL port of MilkDropRenderer.swift

#include "MilkDropGLRenderer.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

GLuint MilkDropGLRenderer::makeFBO(GLuint& tex, int w, int h, bool withAlpha) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0,
                 withAlpha ? GL_RGBA8 : GL_RGB8,
                 w, h, 0,
                 withAlpha ? GL_RGBA : GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "[MilkDropGLRenderer] Incomplete FBO!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

void MilkDropGLRenderer::clearFBO(GLuint fbo, float r, float g, float b, float a) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MilkDropGLRenderer::renderFullscreenQuad() {
    // fractal.vert generates positions from gl_VertexID — no VAO needed
    glDrawArrays(GL_TRIANGLES, 0, 3);  // one oversized triangle covers the screen
}

// ---------------------------------------------------------------------------
// Init / resize / destroy
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::init(const std::string& shadersDir) {
    m_shadersDir = shadersDir;
    std::string vert = shadersDir + "/fractal.vert";  // reuse existing quad vert

    bool ok = true;
    ok &= m_warpShader     .load(vert, shadersDir + "/milkdrop_warp.frag");
    ok &= m_waveShader     .load(shadersDir + "/milkdrop_wave.vert",
                                 shadersDir + "/milkdrop_wave.frag");
    ok &= m_shapeShader    .load(shadersDir + "/milkdrop_shapes.vert",
                                 shadersDir + "/milkdrop_shapes.frag");
    ok &= m_compositeShader.load(vert, shadersDir + "/milkdrop_composite.frag");
    ok &= m_blendShader    .load(vert, shadersDir + "/milkdrop_blend.frag");

    if (!ok) {
        fprintf(stderr, "[MilkDropGLRenderer] Shader compilation failed\n");
        return;
    }

    // Wave VAO / VBO (dynamic, will be orphaned each frame)
    glGenVertexArrays(1, &m_waveVAO);
    glGenBuffers(1, &m_waveVBO);
    glBindVertexArray(m_waveVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_waveVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    glBindVertexArray(0);

    // Shape VAO / VBO
    glGenVertexArrays(1, &m_shapeVAO);
    glGenBuffers(1, &m_shapeVBO);
    glBindVertexArray(m_shapeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_shapeVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    glBindVertexArray(0);

    m_ready = true;
}

void MilkDropGLRenderer::createFBOs(int w, int h) {
    destroyFBOs();
    m_w = w; m_h = h;

    m_warpFboA = makeFBO(m_warpTexA, w, h, false);
    m_warpFboB = makeFBO(m_warpTexB, w, h, false);
    m_waveFbo  = makeFBO(m_waveTex,  w, h, true);   // RGBA (waves have alpha)
    m_shapeFbo = makeFBO(m_shapeTex, w, h, true);
    m_outFbo   = makeFBO(m_outputTex, w, h, false);
    m_blendFbo = makeFBO(m_blendTex,  w, h, false);

    // Allocate two plain textures for transition captures (no separate FBO needed,
    // we copy the composite output into them at transition-start time)
    glGenTextures(1, &m_transA);
    glBindTexture(GL_TEXTURE_2D, m_transA);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &m_transB);
    glBindTexture(GL_TEXTURE_2D, m_transB);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Black-fill both ping-pong warp buffers so first frame isn't garbage
    clearFBO(m_warpFboA, 0, 0, 0, 1);
    clearFBO(m_warpFboB, 0, 0, 0, 1);
}

void MilkDropGLRenderer::destroyFBOs() {
    auto delFBO = [](GLuint& f, GLuint& t) {
        if (f) { glDeleteFramebuffers(1, &f); f = 0; }
        if (t) { glDeleteTextures(1, &t);     t = 0; }
    };
    delFBO(m_warpFboA, m_warpTexA); delFBO(m_warpFboB, m_warpTexB);
    delFBO(m_waveFbo,  m_waveTex);  delFBO(m_shapeFbo, m_shapeTex);
    delFBO(m_outFbo,   m_outputTex); delFBO(m_blendFbo, m_blendTex);
    if (m_transA) { glDeleteTextures(1, &m_transA); m_transA = 0; }
    if (m_transB) { glDeleteTextures(1, &m_transB); m_transB = 0; }
}

MilkDropGLRenderer::~MilkDropGLRenderer() {
    destroyFBOs();
    if (m_waveVBO)  glDeleteBuffers(1, &m_waveVBO);
    if (m_waveVAO)  glDeleteVertexArrays(1, &m_waveVAO);
    if (m_shapeVBO) glDeleteBuffers(1, &m_shapeVBO);
    if (m_shapeVAO) glDeleteVertexArrays(1, &m_shapeVAO);
}

void MilkDropGLRenderer::resize(int w, int h) {
    if (w == m_w && h == m_h) return;
    createFBOs(w, h);
}

// ---------------------------------------------------------------------------
// Preset management
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::loadPreset(const MilkDropPreset& preset) {
    m_current   = preset;
    m_current.parseParameters();
    m_hasPreset = true;
    m_evaluator.loadPreset(m_current);

    // Seed uniforms from preset defaults
    const auto& p = m_current.params;
    m_uniforms.zoom  = p.zoomAmount;
    m_uniforms.rot   = p.rotatAmount;
    m_uniforms.warp  = p.warpScale;
    m_uniforms.cx    = p.centreX;
    m_uniforms.cy    = p.centreY;
    m_uniforms.sx    = p.szx;
    m_uniforms.sy    = p.szy;
    m_uniforms.decay = p.decay;
    m_uniforms.gamma = p.gamma;
}

void MilkDropGLRenderer::beginTransition(const MilkDropPreset& next,
                                          int blendType, float duration) {
    // Snapshot current output into transA
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outFbo);
    glBindTexture(GL_TEXTURE_2D, m_transA);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_w, m_h);

    m_next = next;
    m_next.parseParameters();
    m_blendType          = blendType;
    m_transitionDuration = duration;
    m_transitionProgress = 0.f;
    m_transitioning      = true;
}

// ---------------------------------------------------------------------------
// Main render
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::render(float time, float dt,
                                 const AudioData& audio,
                                 GLuint fractalTex, float fractalBlend) {
    if (!m_ready || m_w == 0 || m_h == 0) return;

    // Advance transition
    if (m_transitioning) {
        m_transitionProgress += dt / m_transitionDuration;
        if (m_transitionProgress >= 1.0f) {
            m_transitionProgress = 1.0f;
            m_transitioning      = false;
            loadPreset(m_next);
        }
    }

    // Update uniforms from equations
    m_uniforms.fps   = (dt > 0.f) ? (1.f / dt) : 60.f;
    m_uniforms.frame += 1.f;
    m_uniforms.progress = m_transitionProgress;

    if (m_hasPreset)
        m_evaluator.evaluate(m_uniforms, audio, time, m_uniforms.fps, m_uniforms.frame);

    // Save viewport and set ours
    GLint prevFBO; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glViewport(0, 0, m_w, m_h);

    // Select ping-pong textures
    GLuint readTex  = m_pingPong ? m_warpTexB : m_warpTexA;
    GLuint writeFBO = m_pingPong ? m_warpFboA : m_warpFboB;
    m_pingPong = !m_pingPong;

    renderWarpPass(time);           // uses readTex, writes to writeFBO
    renderWavePass(audio);
    renderShapePass();
    renderComposite(fractalTex, fractalBlend);

    // During transition, capture this frame into transB then blend
    if (m_transitioning) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outFbo);
        glBindTexture(GL_TEXTURE_2D, m_transB);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_w, m_h);
        renderBlendPass();
    }

    // Restore framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

    (void)readTex; (void)writeFBO; // suppress unused warnings
}

// ---------------------------------------------------------------------------
// Warp pass
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::renderWarpPass(float time) {
    GLuint readTex  = m_pingPong ? m_warpTexA : m_warpTexB;  // note: already toggled
    GLuint writeFbo = m_pingPong ? m_warpFboB : m_warpFboA;

    glBindFramebuffer(GL_FRAMEBUFFER, writeFbo);
    glViewport(0, 0, m_w, m_h);

    m_warpShader.use();
    m_warpShader.setFloat("u_time",   time);
    m_warpShader.setFloat("u_zoom",   m_uniforms.zoom);
    m_warpShader.setFloat("u_rot",    m_uniforms.rot);
    m_warpShader.setFloat("u_warp",   m_uniforms.warp);
    m_warpShader.setFloat("u_cx",     m_uniforms.cx);
    m_warpShader.setFloat("u_cy",     m_uniforms.cy);
    m_warpShader.setFloat("u_dx",     m_uniforms.dx);
    m_warpShader.setFloat("u_dy",     m_uniforms.dy);
    m_warpShader.setFloat("u_sx",     m_uniforms.sx);
    m_warpShader.setFloat("u_sy",     m_uniforms.sy);
    m_warpShader.setFloat("u_decay",  m_uniforms.decay);
    m_warpShader.setFloat("u_gamma",  m_uniforms.gamma);
    float aspect = (m_h > 0) ? (float)m_w / (float)m_h : 1.f;
    m_warpShader.setFloat("u_aspect", aspect);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, readTex);
    m_warpShader.setInt("u_prev", 0);

    renderFullscreenQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Wave pass
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::renderWavePass(const AudioData& audio) {
    clearFBO(m_waveFbo, 0, 0, 0, 0);  // transparent black
    glBindFramebuffer(GL_FRAMEBUFFER, m_waveFbo);
    glViewport(0, 0, m_w, m_h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!m_hasPreset) goto wave_done;

    {
        m_waveShader.use();
        glBindVertexArray(m_waveVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_waveVBO);

        const auto& params = m_current.params;
        for (const auto& wave : params.waves) {
            if (!wave.enabled) continue;
            int count = std::min(wave.samples, 512);
            if (count < 2) continue;

            // Build vertex positions
            std::vector<float> verts(count * 2);
            for (int i = 0; i < count; ++i) {
                float t   = (float)i / (float)(count - 1);
                float amp = audio.waveform[i] * wave.scaling;
                verts[i * 2 + 0] = t;
                verts[i * 2 + 1] = 0.5f + amp * 0.3f;
            }

            glBufferData(GL_ARRAY_BUFFER,
                         (GLsizeiptr)(verts.size() * sizeof(float)),
                         verts.data(), GL_STREAM_DRAW);

            m_waveShader.setVec4("u_wave_color",
                                  wave.r, wave.g, wave.b, wave.a);

            GLenum prim = wave.useDots ? GL_POINTS : GL_LINE_STRIP;
            glDrawArrays(prim, 0, count);
        }
        glBindVertexArray(0);
    }

wave_done:
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Shape pass
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::renderShapePass() {
    clearFBO(m_shapeFbo, 0, 0, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shapeFbo);
    glViewport(0, 0, m_w, m_h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!m_hasPreset) goto shape_done;

    {
        m_shapeShader.use();
        glBindVertexArray(m_shapeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_shapeVBO);

        float aspect = (m_h > 0) ? (float)m_w / (float)m_h : 1.f;
        float aspectInv = (aspect > 0) ? 1.f / aspect : 1.f;

        const auto& params = m_current.params;
        for (const auto& shape : params.shapes) {
            if (!shape.enabled) continue;
            int sides = std::max(shape.sides, 3);

            // Build triangle fan as separate triangles
            std::vector<float> verts;
            verts.reserve(sides * 6);
            for (int i = 0; i < sides; ++i) {
                float a1 = ((float)i      / (float)sides) * 2.f * 3.14159265f + shape.ang;
                float a2 = ((float)(i + 1)/ (float)sides) * 2.f * 3.14159265f + shape.ang;
                verts.push_back(shape.x);
                verts.push_back(shape.y);
                verts.push_back(shape.x + cosf(a1) * shape.radius * aspectInv);
                verts.push_back(shape.y + sinf(a1) * shape.radius);
                verts.push_back(shape.x + cosf(a2) * shape.radius * aspectInv);
                verts.push_back(shape.y + sinf(a2) * shape.radius);
            }

            glBufferData(GL_ARRAY_BUFFER,
                         (GLsizeiptr)(verts.size() * sizeof(float)),
                         verts.data(), GL_STREAM_DRAW);

            m_shapeShader.setVec4("u_shape_color",
                                   shape.r,  shape.g,  shape.b,  shape.a);
            m_shapeShader.setVec4("u_shape_color2",
                                   shape.r2, shape.g2, shape.b2, shape.a2);
            m_shapeShader.setVec2("u_shape_center", shape.x, shape.y);
            m_shapeShader.setFloat("u_shape_radius", shape.radius);

            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 2));
        }
        glBindVertexArray(0);
    }

shape_done:
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Composite pass
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::renderComposite(GLuint fractalTex, float fractalBlend) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_outFbo);
    glViewport(0, 0, m_w, m_h);

    // Select which warp texture is the "latest" composite
    GLuint warpResult = m_pingPong ? m_warpTexA : m_warpTexB;

    m_compositeShader.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, warpResult);
    m_compositeShader.setInt("u_warp_tex", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_waveTex);
    m_compositeShader.setInt("u_wave_tex", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_shapeTex);
    m_compositeShader.setInt("u_shape_tex", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, fractalTex > 0 ? fractalTex : m_shapeTex);
    m_compositeShader.setInt("u_fractal_tex", 3);

    m_compositeShader.setFloat("u_brightness",      1.0f);
    m_compositeShader.setFloat("u_gamma",           m_uniforms.gamma);
    m_compositeShader.setFloat("u_time",            m_uniforms.time);
    m_compositeShader.setFloat("u_fractal_blend",   fractalBlend);
    m_compositeShader.setInt  ("u_fractal_enabled", (fractalEnabled && fractalTex > 0) ? 1 : 0);

    renderFullscreenQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Copy composite back into the warp feedback loop (next frame warp reads this)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outFbo);
    GLuint feedbackFbo = m_pingPong ? m_warpFboA : m_warpFboB;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, feedbackFbo);
    glBlitFramebuffer(0, 0, m_w, m_h, 0, 0, m_w, m_h,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Blend (transition) pass
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::renderBlendPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_blendFbo);
    glViewport(0, 0, m_w, m_h);

    m_blendShader.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_transA);
    m_blendShader.setInt("u_tex_a", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_transB);
    m_blendShader.setInt("u_tex_b", 1);

    m_blendShader.setFloat("u_blend",      m_transitionProgress);
    m_blendShader.setInt  ("u_blend_type", m_blendType);
    m_blendShader.setFloat("u_time",       m_uniforms.time);

    renderFullscreenQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Point output at blend result
    // (swap m_outputTex ↔ m_blendTex so outputTexture() returns the blended frame)
    std::swap(m_outputTex, m_blendTex);
    std::swap(m_outFbo,    m_blendFbo);
}

const uint8_t* MilkDropGLRenderer::readPixels(int w, int h) {
    m_readPixBuf.resize((size_t)w * h * 3);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outFbo);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, m_readPixBuf.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return m_readPixBuf.data();
}
