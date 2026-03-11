#include "VideoTexture.h"
// AVFrame is already included via VideoTexture.h

VideoTexture::VideoTexture() {
    // Create a 1x1 white fallback texture so the shader always has
    // something valid to sample before a video file is loaded.
    glGenTextures(1, &m_texId);
    glBindTexture(GL_TEXTURE_2D, m_texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    const uint8_t white[3] = {255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, white);
    m_w = 1; m_h = 1;
}

VideoTexture::~VideoTexture() {
    if (m_texId) glDeleteTextures(1, &m_texId);
}

void VideoTexture::upload(const AVFrame* frame) {
    if (!frame) return;
    int w = frame->width, h = frame->height;

    glBindTexture(GL_TEXTURE_2D, m_texId);
    // Reallocate storage if the video resolution changed
    if (w != m_w || h != m_h) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        m_w = w; m_h = h;
    }
    // frame->linesize[0] may be padded; upload row by row if needed
    if (frame->linesize[0] == w * 3) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_RGB, GL_UNSIGNED_BYTE, frame->data[0]);
    } else {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0] / 3);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_RGB, GL_UNSIGNED_BYTE, frame->data[0]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
}

void VideoTexture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_texId);
}
