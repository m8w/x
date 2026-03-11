#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <string>
#include <cstdint>

// Encodes rendered OpenGL frames and pushes them to a Restream RTMP endpoint.
class StreamOutput {
public:
    StreamOutput();
    ~StreamOutput();

    // url  = "rtmp://live.restream.io/live/<key>"
    bool start(const std::string& url, int width, int height,
               int bitrate_kbps = 4000, int fps = 30);
    void stop();
    bool isStreaming() const { return m_streaming; }

    // data = packed RGB24 pixels from glReadPixels, row 0 = bottom
    void pushFrame(const uint8_t* rgbData, int width, int height);

    // Settings (set before start())
    std::string url;
    int  bitrate_kbps = 4000;
    int  fps          = 30;

private:
    AVFormatContext* m_fmtCtx   = nullptr;
    AVCodecContext*  m_codecCtx = nullptr;
    AVStream*        m_stream   = nullptr;
    SwsContext*      m_swsCtx   = nullptr;
    AVFrame*         m_frame    = nullptr;
    AVPacket*        m_pkt      = nullptr;
    int64_t          m_pts      = 0;
    bool             m_streaming= false;

    bool encodeAndSend(AVFrame* frame);
};
