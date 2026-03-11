#pragma once
#include "gl_includes.h"
extern "C" {
#include <libavutil/frame.h>
}

// Manages a single GL_TEXTURE_2D that is updated each frame with video data.
class VideoTexture {
public:
    VideoTexture();
    ~VideoTexture();

    // Upload RGB24 frame from FFmpeg to GPU. Creates texture on first call.
    void upload(const AVFrame* frame);
    void bind(int unit = 0) const;
    GLuint id() const { return m_texId; }
    bool   valid() const { return m_texId != 0; }

private:
    GLuint m_texId = 0;
    int    m_w     = 0;
    int    m_h     = 0;
};
