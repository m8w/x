#include "Renderer.h"
#include "entropy/Entropy.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

// SHADERS_DIR is defined by CMake
#ifndef SHADERS_DIR
#define SHADERS_DIR "shaders"
#endif

Renderer::~Renderer() {
    if (m_pbo[0])       glDeleteBuffers(2, m_pbo);
    if (m_vao)          glDeleteVertexArrays(1, &m_vao);
    if (m_fbo)          glDeleteFramebuffers(1, &m_fbo);
    if (m_fboTex)       glDeleteTextures(1, &m_fboTex);
    if (m_spectralFbo)  glDeleteFramebuffers(1, &m_spectralFbo);
    if (m_spectralTex)  glDeleteTextures(1, &m_spectralTex);
    if (m_buddhaFbo)    glDeleteFramebuffers(1, &m_buddhaFbo);
    if (m_buddhaTex[0]) glDeleteTextures(2, m_buddhaTex);
}

void Renderer::init() {
    std::string sd = SHADERS_DIR;
    m_shaderBlend.loadFromFiles   (sd + "/fractal.vert", sd + "/fractal.frag");
    m_shaderBulb.loadFromFiles    (sd + "/fractal.vert", sd + "/mandelbulb.frag");
    m_shaderDistort.loadFromFiles (sd + "/fractal.vert", sd + "/distortion.frag");
    m_shaderSpectral.loadFromFiles(sd + "/fractal.vert", sd + "/spectral.frag");
    m_shaderBuddhaPoints.loadFromFiles(sd + "/buddhabrot_points.vert",
                                       sd + "/buddhabrot_points.geom",
                                       sd + "/buddhabrot_points.frag");
    m_shaderBuddhaDecay.loadFromFiles  (sd + "/fractal.vert", sd + "/buddhabrot_decay.frag");
    m_shaderBuddhaDisplay.loadFromFiles(sd + "/fractal.vert", sd + "/buddhabrot_display.frag");

    // Empty VAO — vertex positions are generated in the vertex shader
    glGenVertexArrays(1, &m_vao);
}

void Renderer::ensureFBO(int w, int h) {
    if (m_fboW == w && m_fboH == h) return;
    if (m_fbo)    glDeleteFramebuffers(1, &m_fbo);
    if (m_fboTex) glDeleteTextures(1, &m_fboTex);

    glGenTextures(1, &m_fboTex);
    glBindTexture(GL_TEXTURE_2D, m_fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_fboTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_fboW = w; m_fboH = h;
    m_pixels.resize(w * h * 3);
}

void Renderer::setSpectralParams(bool enabled, bool onStream,
                                  const float band[4], const float visGain[4]) {
    m_spectralEnabled  = enabled;
    m_spectralOnStream = onStream;
    for (int i = 0; i < 4; i++) {
        m_fftBand[i]    = band[i];
        m_fftVisGain[i] = visGain[i];
    }
}

void Renderer::ensureSpectralFBO(int w, int h) {
    if (m_spectralW == w && m_spectralH == h) return;
    if (m_spectralFbo) glDeleteFramebuffers(1, &m_spectralFbo);
    if (m_spectralTex) glDeleteTextures(1, &m_spectralTex);

    glGenTextures(1, &m_spectralTex);
    glBindTexture(GL_TEXTURE_2D, m_spectralTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_spectralFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_spectralFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_spectralTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_spectralW = w;
    m_spectralH = h;
}

bool Renderer::ensureBuddhaFBO(int w, int h) {
    if (m_buddhaW == w && m_buddhaH == h) return false;
    if (m_buddhaFbo)    glDeleteFramebuffers(1, &m_buddhaFbo);
    if (m_buddhaTex[0]) glDeleteTextures(2, m_buddhaTex);

    glGenTextures(2, m_buddhaTex);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, m_buddhaTex[i]);
        // Float format: accumulated hit counts routinely exceed 1.0 before
        // tone-mapping, and additive blending needs the precision.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glGenFramebuffers(1, &m_buddhaFbo);

    m_buddhaW   = w;
    m_buddhaH   = h;
    m_buddhaIdx = 0;
    return true;
}

void Renderer::renderBuddhabrot(int w, int h, const FractalEngine& eng) {
    bool recreated = ensureBuddhaFBO(w, h);
    // A freshly (re)allocated texture holds undefined data, so force a clear
    // this frame regardless of whether the caller asked for one.
    bool reset = eng.buddhaResetRequested || recreated;

    int writeIdx = 1 - m_buddhaIdx;
    int readIdx  = m_buddhaIdx;

    glBindFramebuffer(GL_FRAMEBUFFER, m_buddhaFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_buddhaTex[writeIdx], 0);
    glViewport(0, 0, w, h);

    // ── Decay pass: carry the previous frame forward, scaled down ───────────
    if (reset) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        m_shaderBuddhaDecay.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_buddhaTex[readIdx]);
        m_shaderBuddhaDecay.setInt  ("u_prev_tex", 0);
        m_shaderBuddhaDecay.setFloat("u_decay",    eng.buddhaDecay);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // ── Points pass: iterate orbits on the GPU, additively accumulate ───────
    m_shaderBuddhaPoints.use();
    m_shaderBuddhaPoints.setFloat2("u_resolution", (float)w, (float)h);
    m_shaderBuddhaPoints.setFloat ("u_zoom",       eng.zoom);
    m_shaderBuddhaPoints.setFloat2("u_offset",     eng.offset.x, eng.offset.y);
    m_shaderBuddhaPoints.setFloat ("u_bailout",    eng.bailout);
    // Entropy-seeded: a fresh random value each frame so the accumulated
    // sample set keeps covering new points rather than repeating a fixed
    // hash sequence (ties into the dual hardware/quantum RNG).
    m_shaderBuddhaPoints.setFloat ("u_seed",       globalEntropy().randomF() * 97931.0f);
    m_shaderBuddhaPoints.setBool  ("u_nebulabrot", eng.nebulabrot);
    m_shaderBuddhaPoints.setInt   ("u_iter_r",     std::min(eng.buddhaIterR, 128));
    m_shaderBuddhaPoints.setInt   ("u_iter_g",     std::min(eng.buddhaIterG, 128));
    m_shaderBuddhaPoints.setInt   ("u_iter_b",     std::min(eng.buddhaIterB, 128));
    m_shaderBuddhaPoints.setInt   ("u_min_iter",   eng.buddhaMinIter);
    m_shaderBuddhaPoints.setBool  ("u_hybrid",       eng.buddhaHybrid);
    m_shaderBuddhaPoints.setInt   ("u_hybrid_count", eng.buddhaHybridCount);
    m_shaderBuddhaPoints.setInt   ("u_hybrid_cycle", eng.buddhaHybridCycle);
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "u_hybrid_list[%d]", i);
        m_shaderBuddhaPoints.setInt(name, eng.buddhaHybridList[i]);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, std::max(0, eng.buddhaSamples));
    glDisable(GL_BLEND);

    // ── Display pass: tone-map into the main FBO so the rest of the pipeline
    //    (spectral post-process, stream/record readback) sees it like any
    //    other fractal mode ──────────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, w, h);
    m_shaderBuddhaDisplay.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_buddhaTex[writeIdx]);
    m_shaderBuddhaDisplay.setInt  ("u_accum_tex",  0);
    m_shaderBuddhaDisplay.setBool ("u_nebulabrot", eng.nebulabrot);
    m_shaderBuddhaDisplay.setFloat("u_brightness", eng.buddhaBrightness);
    m_shaderBuddhaDisplay.setFloat("u_gamma",      eng.buddhaGamma);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_buddhaIdx = writeIdx;
    eng.buddhaResetRequested = false;   // mutable field — fine on a const&
}

void Renderer::uploadUniforms(ShaderProgram& prog, int w, int h, float time,
                               const FractalEngine& eng,
                               const BlendController& blend,
                               const ColorSynth& cs) {
    float bw[5];
    blend.weights(bw);

    prog.setFloat2("u_resolution",      (float)w, (float)h);
    prog.setFloat ("u_time",            time);
    prog.setFloat ("u_blend_mandelbrot",bw[0]);
    prog.setFloat ("u_blend_julia",     bw[1]);
    prog.setFloat ("u_blend_mandelbulb",bw[2]);
    prog.setFloat ("u_blend_euclidean", bw[3]);
    prog.setFloat ("u_blend_diff",      bw[4]);
    prog.setFloat2("u_julia_c",         eng.juliaC.x, eng.juliaC.y);
    prog.setFloat ("u_power",           eng.power);
    prog.setInt   ("u_max_iter",        eng.maxIter);
    prog.setFloat ("u_bailout",         eng.bailout);
    prog.setFloat ("u_zoom",            eng.zoom);
    prog.setFloat2("u_offset",          eng.offset.x, eng.offset.y);
    prog.setInt   ("u_geo_shape",       eng.geoShape);
    prog.setInt   ("u_geo_sides",       eng.geoSides);
    prog.setFloat ("u_geo_radius",      eng.geoRadius);
    prog.setFloat ("u_geo_rotation",    eng.geoRotation);
    prog.setBool  ("u_geo_tile",        eng.geoTile);
    prog.setInt   ("u_geo_mirror",      eng.geoMirror);
    prog.setInt   ("u_geo_kaleid",      eng.geoKaleid);
    // Formula A + B cross-blend, pixel injection, layers, SDF coupling
    prog.setInt   ("u_formula",         eng.formula);
    prog.setInt   ("u_formula_b",       eng.formulaB);
    prog.setFloat ("u_formula_blend",   eng.formulaBlend);
    prog.setFloat ("u_formula_param",   eng.formulaParam);
    prog.setFloat ("u_pixel_weight",    eng.pixelWeight);
    prog.setInt   ("u_layer_count",     eng.layerCount);
    prog.setFloat ("u_layer_offset",    eng.layerOffset);
    prog.setFloat ("u_geo_warp",        eng.geoWarp);
    // 3-D fractal type (mandelbulb.frag)
    prog.setInt   ("u_fractal_3d",      eng.fractal3D);
    prog.setFloat ("u_mb_scale",        eng.mbScale);
    prog.setFloat ("u_mb_fold",         eng.mbFold);
    // Chaos domain warp
    prog.setInt   ("u_chaos_mode",      eng.chaosMode);
    prog.setFloat ("u_chaos_strength",  eng.chaosStrength);
    prog.setFloat ("u_chaos_scale",     eng.chaosScale);
    prog.setFloat ("u_chaos_speed",     eng.chaosSpeed);
    prog.setInt   ("u_video_tex",       0);  // texture unit 0
    prog.setInt   ("u_overlay_tex",     1);  // texture unit 1
    prog.setFloat ("u_overlay_blend",   eng.overlayBlend);
    // Video filters + blend mode
    prog.setInt   ("u_vid_filter",      eng.vidFilter);
    prog.setFloat ("u_vid_fa",          eng.vidFilterA);
    prog.setFloat ("u_vid_fb",          eng.vidFilterB);
    prog.setInt   ("u_ovr_filter",      eng.ovrFilter);
    prog.setFloat ("u_ovr_fa",          eng.ovrFilterA);
    prog.setFloat ("u_ovr_fb",          eng.ovrFilterB);
    prog.setInt   ("u_stream_blend_mode", eng.streamBlendMode);

    // ── Color Synthesizer ─────────────────────────────────────────────────────
    prog.setBool  ("u_cs_enabled",     cs.enabled);
    prog.setFloat3("u_cs_hsl",         cs.outHSL[0],    cs.outHSL[1],    cs.outHSL[2]);
    prog.setFloat3("u_cs_hsl_alt",     cs.outHSLAlt[0], cs.outHSLAlt[1], cs.outHSLAlt[2]);
    prog.setFloat ("u_cs_alt_blend",   cs.outAltBlend);
    prog.setInt   ("u_cs_mode",        cs.blendMode);
    prog.setFloat ("u_cs_opacity",     cs.opacity);
    prog.setFloat ("u_cs_hue_spread",  cs.hueSpread);
    prog.setFloat ("u_cs_lum_spread",  cs.lumSpread);
    // Spectrum mode uniforms
    bool spectrum = cs.enabled && cs.synthMode == 2;
    prog.setBool  ("u_cs_spectrum",    spectrum);
    prog.setFloat ("u_cs_spec_offset", cs.specOffset);
    prog.setFloat ("u_cs_spec_range",  cs.specRange);
    prog.setFloat ("u_cs_spec_density",cs.specDensity);
    prog.setFloat ("u_cs_spec_sat",    cs.specSat);
    prog.setFloat ("u_cs_spec_lum",    cs.specLum);

    // ── Per-fractal layer compositing ─────────────────────────────────────────
    prog.setBool("u_per_fractal_blend", eng.perFractalBlend);
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "u_fl_opacity[%d]", i);
        prog.setFloat(name, eng.fractalOpacity[i]);
        snprintf(name, sizeof(name), "u_fl_blend[%d]", i);
        prog.setInt(name, eng.fractalBlendMode[i]);
        snprintf(name, sizeof(name), "u_fl_order[%d]", i);
        prog.setInt(name, eng.fractalLayerOrder[i]);
    }
}

void Renderer::render(int width, int height, float time,
                       const FractalEngine& engine,
                       const BlendController& blend,
                       const VideoTexture& videoTex,
                       const VideoTexture& overlayTex,
                       const ColorSynth& colorSynth) {
    ensureFBO(width, height);

    if (engine.buddhabrotMode) {
        // Exclusive mode switch, same pattern as distortionMode below —
        // writes its tone-mapped output straight into m_fbo/m_fboTex so the
        // spectral pass and stream/record readback need no changes at all.
        renderBuddhabrot(width, height, engine);
    } else {
        // Choose shader
        float bw[5]; blend.weights(bw);
        ShaderProgram* prog;
        if (engine.distortionMode) {
            prog = &m_shaderDistort;
        } else {
            prog = (bw[2] > 0.5f) ? &m_shaderBulb : &m_shaderBlend;
        }

        // Render into FBO
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        prog->use();
        if (engine.distortionMode) {
            prog->setFloat2("u_resolution",   (float)width, (float)height);
            prog->setFloat ("u_time",         time);
            prog->setFloat ("u_dist_speed",   engine.distortSpeed);
            prog->setInt   ("u_dist_blobs",   engine.distortBlobs);
            prog->setFloat ("u_dist_glow",    engine.distortGlow);
            prog->setFloat ("u_dist_irid",    engine.distortIrid);
            prog->setFloat ("u_dist_outline", engine.distortOutline);
        } else {
            uploadUniforms(*prog, width, height, time, engine, blend, colorSynth);
            videoTex.bind(0);
            overlayTex.bind(1);
            prog->setFloat2("u_overlay_size", (float)overlayTex.width(), (float)overlayTex.height());
            // Suppress overlay blend when no real overlay is loaded (1×1 white fallback)
            if (overlayTex.width() <= 1 && overlayTex.height() <= 1)
                prog->setFloat("u_overlay_blend", 0.0f);
        }

        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // ── Spectral post-process pass ───────────────────────────────────────────
    GLuint outputFbo = m_fbo;
    if (m_spectralEnabled) {
        ensureSpectralFBO(width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, m_spectralFbo);
        glViewport(0, 0, width, height);

        m_shaderSpectral.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_fboTex);
        m_shaderSpectral.setInt       ("u_fbo_tex",        0);
        m_shaderSpectral.setFloat2    ("u_resolution",    (float)width, (float)height);
        m_shaderSpectral.setFloat     ("u_time",          time);
        m_shaderSpectral.setFloatArray("u_fft_band",      m_fftBand,    4);
        m_shaderSpectral.setFloatArray("u_fft_vis_gain",  m_fftVisGain, 4);

        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        outputFbo = m_spectralFbo;
    }

    // Blit to default framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, outputFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

const uint8_t* Renderer::fboPixels(int width, int height) {
    int byteSize = width * height * 3;

    // (Re)allocate both PBOs whenever the resolution changes
    if (width != m_pboW || height != m_pboH) {
        if (m_pbo[0]) glDeleteBuffers(2, m_pbo);
        glGenBuffers(2, m_pbo);
        for (int i = 0; i < 2; i++) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, byteSize, nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        m_pixels.resize(byteSize);
        m_pboW   = width;
        m_pboH   = height;
        m_pboIdx = 0;
    }

    int writeIdx = m_pboIdx;
    int readIdx  = 1 - m_pboIdx;

    // When spectral is active AND onStream, encode the post-processed frame
    GLuint readFbo = (m_spectralEnabled && m_spectralOnStream) ? m_spectralFbo : m_fbo;

    // Queue async readback into writeIdx — GPU DMA, returns immediately
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[writeIdx]);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE,
                 nullptr);  // nullptr = write into PBO at offset 0
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // Map readIdx PBO — this is the *previous* frame, DMA is already complete
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[readIdx]);
    void* ptr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (ptr) {
        memcpy(m_pixels.data(), ptr, byteSize);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    m_pboIdx = readIdx;  // ping-pong for next call
    return m_pixels.data();
}
