#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>
#include <libavdevice/avdevice.h>
}
#include <string>
#include <vector>

// Decodes local video files or live capture devices (webcam/camera).
// Returns RGB24 AVFrames ready for upload to a GL texture.
class VideoInput {
public:
    VideoInput();
    ~VideoInput();

    bool open(const std::string& path);           // local file
    bool openCamera(int deviceIdx);               // live camera (avfoundation/v4l2)
    void close();
    bool isOpen()   const { return m_fmtCtx != nullptr; }
    bool isCamera() const { return m_isCamera; }

    // Returns detected camera names (index = device index for openCamera()).
    // macOS: avfoundation video devices. Linux: /dev/videoN devices.
    static std::vector<std::string> listCameras();

    // Returns the next decoded frame in RGB24.
    // Caller must call releaseFrame() when done.
    // Returns nullptr if no frame available (end of file / camera not ready yet).
    // Force output RGB frame to this resolution (upscales/downscales via swscale).
    // Pass 0,0 to use the source's native resolution (default).
    // Call each frame from main.cpp with the FBO size to keep camera at full screen.
    void setOutputSize(int w, int h) { m_outW = w; m_outH = h; }

    AVFrame* nextFrame();
    void     releaseFrame(AVFrame* frame);

    int width()  const { return m_width; }
    int height() const { return m_height; }
    const std::string& path() const { return m_path; }

private:
    AVFormatContext* m_fmtCtx    = nullptr;
    AVCodecContext*  m_codecCtx  = nullptr;
    SwsContext*      m_swsCtx    = nullptr;
    AVFrame*         m_frame     = nullptr;   // decoded (HW or SW)
    AVFrame*         m_frameSW   = nullptr;   // CPU copy when using HW decode
    AVFrame*         m_frameRGB  = nullptr;   // converted RGB24
    AVPacket*        m_pkt       = nullptr;
    AVBufferRef*     m_hwDevCtx  = nullptr;   // VideoToolbox device (macOS only)
    bool             m_useHW     = false;
    int              m_streamIdx = -1;
    int              m_width     = 0;
    int              m_height    = 0;
    AVPixelFormat    m_lastPixFmt= AV_PIX_FMT_NONE;
    int              m_srcW      = 0;   // source frame size (for swscale invalidation)
    int              m_srcH      = 0;
    bool             m_isCamera  = false;
    int              m_outW      = 0;   // 0 = use native source size
    int              m_outH      = 0;
    std::string      m_path;

    bool initCodec();
    void ensureSwsCtx(AVPixelFormat srcFmt, int w, int h);
};
