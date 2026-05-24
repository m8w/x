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

// Decodes local video files, live cameras, or screen/window captures.
// Returns RGB24 AVFrames ready for upload to a GL texture.
class VideoInput {
public:
    VideoInput();
    ~VideoInput();

    bool open(const std::string& path);           // local file
    bool openCamera(int deviceIdx);               // live camera (avfoundation/v4l2)
    bool openCameraByName(const std::string& devStr, int fps = 30);
    void close();
    bool isOpen()          const { return m_fmtCtx != nullptr; }
    bool isCamera()        const { return m_isCamera; }
    bool isScreenCapture() const { return m_isScreen; }

    struct CameraInfo { std::string name; std::string devStr; };
    static std::vector<CameraInfo> listCameras();

    // Screen / monitor capture.
    // devStr on macOS  : avfoundation video-device index string (e.g. "1")
    // devStr on Linux  : ":0.0" for whole display, or ":0.0+ox,oy WxH" for a monitor
    struct ScreenInfo { std::string name; std::string devStr; };
    static std::vector<ScreenInfo> listScreens();
    bool openScreenCapture(const std::string& devStr, int fps = 30);

    // Window capture.  Fill a WindowInfo from listWindows() then pass to openWindowCapture().
    // Linux  : enumerates via wmctrl; capture uses x11grab with window geometry.
    // macOS  : enumerates via CGWindowList; capture uses avfoundation region.
    struct WindowInfo {
        std::string title;
        std::string devStr;   // platform-specific; pass opaquely to openWindowCapture
        int x = 0, y = 0, w = 0, h = 0;
    };
    static std::vector<WindowInfo> listWindows();
    bool openWindowCapture(const WindowInfo& win, int fps = 30);

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
    bool             m_isScreen  = false;
    int              m_outW      = 0;   // 0 = use native source size
    int              m_outH      = 0;
    std::string      m_path;

    bool initCodec();
    void ensureSwsCtx(AVPixelFormat srcFmt, int w, int h);
};
