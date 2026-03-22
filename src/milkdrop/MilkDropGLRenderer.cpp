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
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);  // fullscreen quad via gl_VertexID
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Init / resize / destroy
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::init(const std::string& shadersDir) {
    m_shadersDir = shadersDir;
    std::string vert = shadersDir + "/fractal.vert";  // reuse existing quad vert

    bool ok = true;
    // Use new mesh warp vertex shader instead of fractal.vert
    ok &= m_warpShader     .load(shadersDir + "/milkdrop_warp.vert",
                                  shadersDir + "/milkdrop_warp.frag");
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

    // Empty VAO for fullscreen quad passes (core profile requires a bound VAO)
    glGenVertexArrays(1, &m_quadVAO);

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

    // Warp mesh VAO/VBO (4 floats per vertex: NDC_x, NDC_y, src_uv_x, src_uv_y)
    glGenVertexArrays(1, &m_meshVAO);
    glGenBuffers(1, &m_meshVBO);
    glBindVertexArray(m_meshVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_meshVBO);
    glEnableVertexAttribArray(0);  // a_pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)0);
    glEnableVertexAttribArray(1);  // a_uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(sizeof(float)*2));
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

    // Seed with dim grey so the feedback loop has something to evolve from.
    // Black * decay = black forever, so we must start with non-zero content.
    clearFBO(m_warpFboA, 0.15f, 0.15f, 0.15f, 1.f);
    clearFBO(m_warpFboB, 0.15f, 0.15f, 0.15f, 1.f);
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
    if (m_quadVAO)  glDeleteVertexArrays(1, &m_quadVAO);
    if (m_waveVBO)  glDeleteBuffers(1, &m_waveVBO);
    if (m_waveVAO)  glDeleteVertexArrays(1, &m_waveVAO);
    if (m_shapeVBO) glDeleteBuffers(1, &m_shapeVBO);
    if (m_shapeVAO) glDeleteVertexArrays(1, &m_shapeVAO);
    if (m_meshVBO)  glDeleteBuffers(1, &m_meshVBO);
    if (m_meshVAO)  glDeleteVertexArrays(1, &m_meshVAO);
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

    // Seed warp FBOs with a dim white so the feedback loop has something
    // to evolve from on the first frame (black * decay = black forever).
    if (m_warpFboA && m_warpFboB) {
        clearFBO(m_warpFboA, 0.12f, 0.12f, 0.12f, 1.f);
        clearFBO(m_warpFboB, 0.12f, 0.12f, 0.12f, 1.f);
    }
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
    renderWavePass(audio, time);
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

}

// ---------------------------------------------------------------------------
// Warp pass
// ---------------------------------------------------------------------------

void MilkDropGLRenderer::renderWarpPass(float time) {
    GLuint readTex  = m_pingPong ? m_warpTexA : m_warpTexB;  // note: already toggled
    GLuint writeFbo = m_pingPong ? m_warpFboB : m_warpFboA;

    // ── Build displaced mesh on CPU ───────────────────────────────────────
    float aspect = (m_h > 0) ? (float)m_w / (float)m_h : 1.f;

    // Compute per-vertex source UVs: MESH_W * MESH_H grid
    // Each vertex: [NDC_x, NDC_y, src_u, src_v]
    static thread_local std::vector<float> meshVerts;
    const int GW = MESH_W, GH = MESH_H;
    const int quadCount = (GW-1) * (GH-1);
    meshVerts.resize(quadCount * 6 * 4);  // 6 verts/quad (2 triangles), 4 floats/vert

    // Per-vertex UV helper — computes source UV from (possibly preset-modified) params
    auto computeSrcUV = [&](float vx, float vy, const EquationEvaluator::VertexParams& p)
        -> std::pair<float,float>
    {
        float cx  = p.cx, cy  = p.cy;
        float zoom = std::max(p.zoom, 0.001f);
        float dx  = p.dx, dy  = p.dy;
        float sx  = p.sx, sy  = p.sy;
        float rot  = p.rot;
        float warp = std::max(p.warp, 0.5f);

        float uvCx = (vx - cx) * aspect;
        float uvCy = (vy - cy);
        uvCx /= zoom; uvCy /= zoom;
        float c = cosf(rot), s = sinf(rot);
        float rx = uvCx*c - uvCy*s, ry = uvCx*s + uvCy*c;
        uvCx = rx; uvCy = ry;
        uvCx *= sx; uvCy *= sy;
        uvCx += dx * 2.0f; uvCy += dy * 2.0f;
        float t2 = time * warp * 0.5f;
        uvCx += sinf(t2*1.11f + uvCy*3.0f) * warp * 0.03f;
        uvCy += cosf(t2*0.93f + uvCx*2.5f) * warp * 0.03f;
        return { uvCx / aspect + cx, uvCy + cy };
    };

    // Build 2D grid first, then triangulate
    struct V4 { float px, py, ux, uy; };
    static thread_local std::vector<V4> grid;
    grid.resize(GW * GH);
    for (int r = 0; r < GH; ++r) {
        for (int c = 0; c < GW; ++c) {
            float vx = (float)c / (float)(GW-1);
            float vy = (float)r / (float)(GH-1);
            EquationEvaluator::VertexParams vp;
            m_evaluator.evaluateVertex(vx, vy, m_uniforms, vp);
            auto [su, sv] = computeSrcUV(vx, vy, vp);
            grid[r*GW+c] = { vx*2.f-1.f, vy*2.f-1.f, su, sv };
        }
    }
    // Triangulate (2 triangles per quad)
    int idx = 0;
    for (int r = 0; r < GH-1; ++r) {
        for (int c = 0; c < GW-1; ++c) {
            V4* vs[4] = {
                &grid[ r   *GW+c],   // TL
                &grid[(r+1)*GW+c],   // BL
                &grid[ r   *GW+c+1], // TR
                &grid[(r+1)*GW+c+1]  // BR
            };
            // Tri 1: TL, BL, TR
            // Tri 2: BL, BR, TR
            for (auto* v : {vs[0],vs[1],vs[2], vs[1],vs[3],vs[2]}) {
                meshVerts[idx++] = v->px;
                meshVerts[idx++] = v->py;
                meshVerts[idx++] = v->ux;
                meshVerts[idx++] = v->uy;
            }
        }
    }

    // ── Upload and render ─────────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, writeFbo);
    glViewport(0, 0, m_w, m_h);

    m_warpShader.use();
    m_warpShader.setFloat("u_decay", m_uniforms.decay);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, readTex);
    m_warpShader.setInt("u_prev", 0);

    glBindVertexArray(m_meshVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_meshVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(meshVerts.size() * sizeof(float)),
                 meshVerts.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(meshVerts.size() / 4));
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Wave pass
// ---------------------------------------------------------------------------

// Build a thick ribbon (GL_TRIANGLE_STRIP) from a poly-line.
// macOS core profile doesn't support glLineWidth > 1, so we extrude manually.
static std::vector<float> buildRibbon(const float* pts, int count, float halfW) {
    std::vector<float> ribbon;
    ribbon.reserve(count * 4);
    for (int i = 0; i < count; ++i) {
        int p = std::max(0, i - 1);
        int n = std::min(count - 1, i + 1);
        float tx = pts[n * 2] - pts[p * 2];
        float ty = pts[n * 2 + 1] - pts[p * 2 + 1];
        float len = sqrtf(tx * tx + ty * ty);
        float nx = (len > 1e-6f) ? -ty / len : 0.f;
        float ny = (len > 1e-6f) ?  tx / len : 1.f;
        ribbon.push_back(pts[i * 2]     + nx * halfW);
        ribbon.push_back(pts[i * 2 + 1] + ny * halfW);
        ribbon.push_back(pts[i * 2]     - nx * halfW);
        ribbon.push_back(pts[i * 2 + 1] - ny * halfW);
    }
    return ribbon;
}

void MilkDropGLRenderer::renderWavePass(const AudioData& audio, float time) {
    clearFBO(m_waveFbo, 0, 0, 0, 0);  // transparent black
    glBindFramebuffer(GL_FRAMEBUFFER, m_waveFbo);
    glViewport(0, 0, m_w, m_h);

    // Write raw RGBA directly — no alpha blending.
    // With SRC_ALPHA/ONE_MINUS_SRC_ALPHA the stored alpha gets squared
    // (0.28² ≈ 0.08), making waves invisible in the composite pass.
    // composite.frag does the final mix(warp, wave.rgb, wave.a) itself.
    glDisable(GL_BLEND);

    {
        // Always generate an animated synth waveform and add it to the real
        // audio signal (boosted to a visible scale).  Real mic audio is often
        // in the 0.001 – 0.01 range which looks like a flat line, so we
        // normalise it to ±0.25 and layer the synth on top for constant motion.
        float boosted[512];
        float gain = (audio.rms > 1e-5f) ? std::min(0.25f / audio.rms, 30.f) : 0.f;
        for (int i = 0; i < 512; ++i) {
            float tt = (float)i / 511.f;
            float s = sinf(time * 1.7f + tt * 6.2832f *  2.f) * 0.18f
                    + sinf(time * 1.1f + tt * 6.2832f *  5.f) * 0.09f
                    + sinf(time * 2.3f + tt * 6.2832f * 11.f) * 0.04f;
            boosted[i] = s + audio.waveform[i] * gain;
        }
        const float* waveData = boosted;

        m_waveShader.use();
        glBindVertexArray(m_waveVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_waveVBO);

        const auto& params = m_current.params;

        // ── New-style wave_N objects ─────────────────────────────────────────
        for (const auto& wave : params.waves) {
            if (!wave.enabled) continue;
            int count = std::min(wave.samples, 512);
            if (count < 2) continue;

            std::vector<float> pts(count * 2);
            for (int i = 0; i < count; ++i) {
                float t   = (float)i / (float)(count - 1);
                float amp = waveData[i] * wave.scaling;
                pts[i * 2]     = t;
                pts[i * 2 + 1] = 0.5f + amp * 0.3f;
            }

            m_waveShader.setVec4("u_wave_color", wave.r, wave.g, wave.b, wave.a);

            if (wave.useDots) {
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pts.size() * sizeof(float)),
                             pts.data(), GL_STREAM_DRAW);
                glDrawArrays(GL_POINTS, 0, count);
            } else {
                auto ribbon = buildRibbon(pts.data(), count, 0.022f);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ribbon.size() * sizeof(float)),
                             ribbon.data(), GL_STREAM_DRAW);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, count * 2);
            }
        }

        // ── Legacy global wave (classic .milk presets without wave_N) ────────
        // Always render: if the preset has waves disabled (legacyWaveA==0) and no
        // wave_N objects, fall back to a dim white oscilloscope so there is always
        // visible content for the warp feedback loop to evolve.
        if (params.waves.empty()) {
            const auto& p = params;
            float waveA = (p.legacyWaveA > 0.01f) ? p.legacyWaveA : 0.55f;
            float waveR = (p.legacyWaveA > 0.01f) ? p.legacyWaveR : 1.0f;
            float waveG = (p.legacyWaveA > 0.01f) ? p.legacyWaveG : 1.0f;
            float waveB = (p.legacyWaveA > 0.01f) ? p.legacyWaveB : 1.0f;
            int count = 512;
            std::vector<float> pts(count * 2);

            float scale = (p.legacyWaveScale > 0.f) ? p.legacyWaveScale : 1.f;
            for (int i = 0; i < count; ++i) {
                float t   = (float)i / (float)(count - 1);
                float amp = waveData[i] * scale;
                if (p.legacyWaveMode >= 4) {
                    float angle = t * 6.2831853f;
                    float r     = 0.35f + amp * 0.10f;
                    pts[i * 2]     = 0.5f + r * cosf(angle);
                    pts[i * 2 + 1] = 0.5f + r * sinf(angle);
                } else {
                    // Oscilloscope — synth keeps it animated, warp smears it into patterns
                    pts[i * 2]     = t;
                    pts[i * 2 + 1] = 0.5f + amp * 0.35f;
                }
            }

            m_waveShader.setVec4("u_wave_color", waveR, waveG, waveB, waveA);

            if (p.legacyWaveDots) {
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pts.size() * sizeof(float)),
                             pts.data(), GL_STREAM_DRAW);
                glDrawArrays(GL_POINTS, 0, count);
            } else {
                auto ribbon = buildRibbon(pts.data(), count, 0.022f);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ribbon.size() * sizeof(float)),
                             ribbon.data(), GL_STREAM_DRAW);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, count * 2);
            }

        }

        glBindVertexArray(0);
    }

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

    // Select the texture that was just written by renderWarpPass.
    // pingPong was already toggled before the passes ran:
    //   pingPong=true  → warp wrote to warpFboB → read warpTexB
    //   pingPong=false → warp wrote to warpFboA → read warpTexA
    GLuint warpResult = m_pingPong ? m_warpTexB : m_warpTexA;

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

    // Copy composite back into the same slot warp just wrote — so next frame's
    // warp reads this composite (not the raw warp output which would skip the overlay).
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outFbo);
    GLuint feedbackFbo = m_pingPong ? m_warpFboB : m_warpFboA;
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

void MilkDropGLRenderer::blitToScreen(int w, int h) {
    if (!m_outFbo) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, m_w, m_h, 0, 0, w, h,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
