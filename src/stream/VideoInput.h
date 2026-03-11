#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <string>

// Decodes local video files (any format FFmpeg supports) and returns
// RGB24 AVFrames ready for upload to a GL texture.
class VideoInput {
public:
    VideoInput();
    ~VideoInput();

    bool open(const std::string& path);
    void close();
    bool isOpen() const { return m_fmtCtx != nullptr; }

    // Returns the next decoded frame in RGB24.
    // Caller must call releaseFrame() when done.
    // Returns nullptr if no frame available (end of file or error).
    AVFrame* nextFrame();
    void     releaseFrame(AVFrame* frame);

    int width()  const { return m_width; }
    int height() const { return m_height; }
    const std::string& path() const { return m_path; }

private:
    AVFormatContext* m_fmtCtx    = nullptr;
    AVCodecContext*  m_codecCtx  = nullptr;
    SwsContext*      m_swsCtx    = nullptr;
    AVFrame*         m_frame     = nullptr;   // decoded (any format)
    AVFrame*         m_frameRGB  = nullptr;   // converted RGB24
    AVPacket*        m_pkt       = nullptr;
    int              m_streamIdx = -1;
    int              m_width     = 0;
    int              m_height    = 0;
    AVPixelFormat    m_lastPixFmt= AV_PIX_FMT_NONE;
    std::string      m_path;

    bool initCodec();
    void ensureSwsCtx(AVPixelFormat srcFmt, int w, int h);
};
