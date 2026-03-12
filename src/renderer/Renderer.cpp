#include "Renderer.h"
#include <cstdio>
#include <string>

// SHADERS_DIR is defined by CMake
#ifndef SHADERS_DIR
#define SHADERS_DIR "shaders"
#endif

Renderer::~Renderer() {
    if (m_vao)    glDeleteVertexArrays(1, &m_vao);
    if (m_fbo)    glDeleteFramebuffers(1, &m_fbo);
    if (m_fboTex) glDeleteTextures(1, &m_fboTex);
}

void Renderer::init() {
    std::string sd = SHADERS_DIR;
    m_shaderBlend.loadFromFiles(sd + "/fractal.vert", sd + "/fractal.frag");
    m_shaderBulb.loadFromFiles (sd + "/fractal.vert", sd + "/mandelbulb.frag");

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
                               const BlendController& blend) {
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
    // Formula A + B cross-blend, pixel injection, layers, SDF coupling
    prog.setInt   ("u_formula",         eng.formula);
    prog.setInt   ("u_formula_b",       eng.formulaB);
    prog.setFloat ("u_formula_blend",   eng.formulaBlend);
    prog.setFloat ("u_pixel_weight",    eng.pixelWeight);
    prog.setInt   ("u_layer_count",     eng.layerCount);
    prog.setFloat ("u_layer_offset",    eng.layerOffset);
    prog.setFloat ("u_geo_warp",        eng.geoWarp);
    // 3-D fractal type (mandelbulb.frag)
    prog.setInt   ("u_fractal_3d",      eng.fractal3D);
    prog.setFloat ("u_mb_scale",        eng.mbScale);
    prog.setFloat ("u_mb_fold",         eng.mbFold);
    prog.setInt   ("u_video_tex",       0);  // texture unit 0
}

void Renderer::render(int width, int height, float time,
                       const FractalEngine& engine,
                       const BlendController& blend,
                       const VideoTexture& videoTex) {
    ensureFBO(width, height);

    // Choose shader: use Mandelbulb shader when its weight dominates
    float bw[4]; blend.weights(bw);
    ShaderProgram& prog = (bw[2] > 0.5f) ? m_shaderBulb : m_shaderBlend;

    // Render into FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    prog.use();
    uploadUniforms(prog, width, height, time, engine, blend);
    videoTex.bind(0);

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
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, m_pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return m_pixels.data();
}
