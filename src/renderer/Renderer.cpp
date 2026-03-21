#include "Renderer.h"
#include <cstdio>
#include <cstring>
#include <string>

// SHADERS_DIR is defined by CMake
#ifndef SHADERS_DIR
#define SHADERS_DIR "shaders"
#endif

Renderer::~Renderer() {
    if (m_pbo[0]) glDeleteBuffers(2, m_pbo);
    if (m_vao)    glDeleteVertexArrays(1, &m_vao);
    if (m_fbo)    glDeleteFramebuffers(1, &m_fbo);
    if (m_fboTex) glDeleteTextures(1, &m_fboTex);
}

void Renderer::init() {
    std::string sd = SHADERS_DIR;
    m_shaderBlend.loadFromFiles  (sd + "/fractal.vert", sd + "/fractal.frag");
    m_shaderBulb.loadFromFiles   (sd + "/fractal.vert", sd + "/mandelbulb.frag");
    m_shaderDistort.loadFromFiles(sd + "/fractal.vert", sd + "/distortion.frag");

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

    // ── Color Synthesizer ─────────────────────────────────────────────────────
    prog.setBool  ("u_cs_enabled",    cs.enabled);
    prog.setFloat3("u_cs_hsl",        cs.outHSL[0],    cs.outHSL[1],    cs.outHSL[2]);
    prog.setFloat3("u_cs_hsl_alt",    cs.outHSLAlt[0], cs.outHSLAlt[1], cs.outHSLAlt[2]);
    prog.setFloat ("u_cs_alt_blend",  cs.outAltBlend);
    prog.setInt   ("u_cs_mode",       cs.blendMode);
    prog.setFloat ("u_cs_hue_spread", cs.hueSpread);
    prog.setFloat ("u_cs_lum_spread", cs.lumSpread);
}

void Renderer::render(int width, int height, float time,
                       const FractalEngine& engine,
                       const BlendController& blend,
                       const VideoTexture& videoTex,
                       const VideoTexture& overlayTex,
                       const ColorSynth& colorSynth) {
    ensureFBO(width, height);

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
        // Suppress overlay blend when no real overlay is loaded (1×1 white fallback)
        if (overlayTex.width() <= 1 && overlayTex.height() <= 1)
            prog->setFloat("u_overlay_blend", 0.0f);
    }

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Blit FBO to default framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
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

    // Queue async readback into writeIdx — GPU DMA, returns immediately
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
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
