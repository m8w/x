#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>
#include <libavdevice/avdevice.h>
}
#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif
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
    bool isOpen()          const { return m_fmtCtx != nullptr || m_useCGImage; }
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
    // macOS  : uses CGWindowListCreateImage — captures even when the window is behind
    //          other windows (works on a single monitor setup).
    // Linux  : uses x11grab region; the window must not be fully occluded.
    //          For single-monitor use, run the visualizer on an Xvfb virtual display
    //          and capture that display via openScreenCapture(":99.0").
    struct WindowInfo {
        std::string title;
        std::string devStr;   // platform-specific; pass opaquely to openWindowCapture
        int x = 0, y = 0, w = 0, h = 0;
    };
    static std::vector<WindowInfo> listWindows();
    bool openWindowCapture(const WindowInfo& win, int fps = 30);

    // Linux helper: launch an Xvfb virtual display on the given display number.
    // Returns the display string (e.g. ":99") on success, "" on failure.
    // After calling this, run your visualizer with DISPLAY=:99 <app>,
    // then openScreenCapture(":99.0") to capture it.
    static std::string launchVirtualDisplay(int displayNum = 99,
                                            int w = 1920, int h = 1080);

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
    AVFrame*         m_frame     = nullptr;
    AVFrame*         m_frameSW   = nullptr;
    AVFrame*         m_frameRGB  = nullptr;
    AVPacket*        m_pkt       = nullptr;
    AVBufferRef*     m_hwDevCtx  = nullptr;
    bool             m_useHW     = false;
    int              m_streamIdx = -1;
    int              m_width     = 0;
    int              m_height    = 0;
    AVPixelFormat    m_lastPixFmt= AV_PIX_FMT_NONE;
    int              m_srcW      = 0;
    int              m_srcH      = 0;
    bool             m_isCamera  = false;
    bool             m_isScreen  = false;
    int              m_outW      = 0;
    int              m_outH      = 0;
    std::string      m_path;

    // macOS CoreGraphics window-capture mode (single-monitor capable)
    bool             m_useCGImage  = false;
#ifdef __APPLE__
    CGWindowID       m_cgWindowID  = kCGNullWindowID;
#endif
    SwsContext*      m_cgSwsCtx    = nullptr;  // BGRA→RGB24 for CGImage path

    bool initCodec();
    void ensureSwsCtx(AVPixelFormat srcFmt, int w, int h);
    AVFrame* nextFrameCGImage();   // macOS CoreGraphics path
};
