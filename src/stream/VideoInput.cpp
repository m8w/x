#include "VideoInput.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#ifdef __APPLE__
#include <libavutil/hwcontext_videotoolbox.h>
// CoreGraphics included via VideoInput.h
#endif
#ifdef _WIN32
#include <windows.h>
#elif !defined(__APPLE__)
#include <unistd.h>
#include <time.h>
#endif

VideoInput::VideoInput() {
    m_frame    = av_frame_alloc();
    m_frameSW  = av_frame_alloc();
    m_frameRGB = av_frame_alloc();
    m_pkt      = av_packet_alloc();
}

VideoInput::~VideoInput() {
    close();
    av_frame_free(&m_frame);
    av_frame_free(&m_frameSW);
    av_frame_free(&m_frameRGB);
    av_packet_free(&m_pkt);
}

// ── Camera enumeration ────────────────────────────────────────────────────────

std::vector<VideoInput::CameraInfo> VideoInput::listCameras() {
    std::vector<CameraInfo> result;
    avdevice_register_all();

#if defined(__APPLE__)
    const AVInputFormat* fmt = av_find_input_format("avfoundation");
    if (!fmt) return result;
    AVDeviceInfoList* devList = nullptr;
    if (avdevice_list_input_sources(fmt, nullptr, nullptr, &devList) >= 0 && devList) {
        int vidIdx = 0;
        for (int i = 0; i < devList->nb_devices; i++) {
            AVDeviceInfo* d = devList->devices[i];
            bool hasVideo = false;
            for (int j = 0; j < d->nb_media_types; j++) {
                if (d->media_types[j] == AVMEDIA_TYPE_VIDEO) { hasVideo = true; break; }
            }
            if (hasVideo) {
                CameraInfo ci;
                ci.name   = d->device_description ? d->device_description : d->device_name;
                ci.devStr = std::to_string(vidIdx++);   // avfoundation video-device index
                result.push_back(ci);
            }
        }
        avdevice_free_list_devices(&devList);
    }
    if (result.empty())
        fprintf(stderr, "VideoInput: no cameras found via avfoundation.\n"
                "  → Check System Settings > Privacy & Security > Camera\n"
                "    and grant access to your terminal app.\n");
#elif defined(__linux__)
    for (int i = 0; i < 8; i++) {
        std::string dev = "/dev/video" + std::to_string(i);
        if (access(dev.c_str(), F_OK) == 0)
            result.push_back({dev, dev});
    }
#elif defined(_WIN32)
    const AVInputFormat* fmt = av_find_input_format("dshow");
    if (fmt) {
        AVDeviceInfoList* devList = nullptr;
        if (avdevice_list_input_sources(fmt, "video=dummy", nullptr, &devList) >= 0 && devList) {
            for (int i = 0; i < devList->nb_devices; i++) {
                AVDeviceInfo* d = devList->devices[i];
                bool hasVideo = false;
                for (int j = 0; j < d->nb_media_types; j++)
                    if (d->media_types[j] == AVMEDIA_TYPE_VIDEO) { hasVideo = true; break; }
                if (!hasVideo) continue;
                std::string name = d->device_description ? d->device_description
                                                         : (d->device_name ? d->device_name : "");
                std::string devStr = "video=" + name;
                result.push_back({name, devStr});
            }
            avdevice_free_list_devices(&devList);
        }
    }
#endif
    return result;
}

// ── Camera open ───────────────────────────────────────────────────────────────

bool VideoInput::openCameraByName(const std::string& devStr, int fps) {
    close();
    m_isCamera = true;
    avdevice_register_all();

    const AVInputFormat* fmt = nullptr;

#if defined(__APPLE__)
    fmt = av_find_input_format("avfoundation");
#elif defined(_WIN32)
    fmt = av_find_input_format("dshow");
#elif defined(__linux__)
    fmt = av_find_input_format("v4l2");
#else
    fprintf(stderr, "VideoInput: camera capture not supported on this platform\n");
    m_isCamera = false;
    return false;
#endif

    if (!fmt) {
        fprintf(stderr, "VideoInput: camera capture format not available\n");
        m_isCamera = false;
        return false;
    }

    m_path = "cam:" + devStr;

    // Try open without specifying pixel format — let the device choose its native format.
    // avfoundation on newer macOS / Continuity Camera uses nv12/420v, not uyvy422.
    AVDictionary* opts = nullptr;
    char fpsBuf[16];
    snprintf(fpsBuf, sizeof(fpsBuf), "%d", fps);
    av_dict_set(&opts, "framerate", fpsBuf, 0);
    int ret = avformat_open_input(&m_fmtCtx, devStr.c_str(), fmt, &opts);
    av_dict_free(&opts);

    if (ret != 0) {
        // Fallback: try uyvy422 (native for older FaceTime HD cameras)
        opts = nullptr;
        av_dict_set(&opts, "framerate",    fpsBuf,   0);
        av_dict_set(&opts, "pixel_format", "uyvy422", 0);
        ret = avformat_open_input(&m_fmtCtx, devStr.c_str(), fmt, &opts);
        av_dict_free(&opts);
    }

    if (ret != 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "VideoInput: cannot open camera '%s': %s\n", devStr.c_str(), errbuf);
#ifdef __APPLE__
        fprintf(stderr, "  → If this is a permission error, go to\n"
                "    System Settings > Privacy & Security > Camera\n"
                "    and grant access to your terminal app (Terminal / iTerm2).\n");
#endif
        m_isCamera = false;
        return false;
    }

    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "VideoInput: camera has no stream info\n");
        return false;
    }
    fprintf(stderr, "VideoInput: camera '%s' opened\n", devStr.c_str());
    return initCodec();
}

bool VideoInput::openCamera(int idx) {
    auto cameras = listCameras();
    if (idx < 0 || idx >= (int)cameras.size()) {
        fprintf(stderr, "VideoInput: camera index %d out of range (%d cameras)\n",
                idx, (int)cameras.size());
        return false;
    }
    return openCameraByName(cameras[idx].devStr);
}

// ── File open ─────────────────────────────────────────────────────────────────

bool VideoInput::open(const std::string& path) {
    close();
    m_path = path;

    if (avformat_open_input(&m_fmtCtx, path.c_str(), nullptr, nullptr) != 0) {
        fprintf(stderr, "VideoInput: cannot open '%s'\n", path.c_str());
        return false;
    }
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "VideoInput: no stream info\n");
        return false;
    }
    return initCodec();
}

bool VideoInput::initCodec() {
    m_streamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_streamIdx < 0) {
        fprintf(stderr, "VideoInput: no video stream\n");
        return false;
    }
    AVStream* stream = m_fmtCtx->streams[m_streamIdx];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "VideoInput: no decoder\n");
        return false;
    }
    m_codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtx, stream->codecpar);

#ifdef __APPLE__
    // Try VideoToolbox hardware accelerated decode.
    // Supports H.264, HEVC, ProRes, VP9, AV1 on Apple Silicon.
    // Falls back to software decode silently on failure.
    m_useHW = false;
    if (av_hwdevice_ctx_create(&m_hwDevCtx,
                               AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
                               nullptr, nullptr, 0) == 0) {
        m_codecCtx->hw_device_ctx = av_buffer_ref(m_hwDevCtx);
        m_useHW = true;
        fprintf(stderr, "VideoInput: VideoToolbox HW decode enabled\n");
    } else {
        fprintf(stderr, "VideoInput: VideoToolbox unavailable, using SW decode\n");
    }
#endif

    // Multi-threaded SW decode (used when HW is off or as fallback)
    if (!m_useHW) {
        m_codecCtx->thread_count = 0;  // auto
        m_codecCtx->thread_type  = FF_THREAD_FRAME;
    }
    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        fprintf(stderr, "VideoInput: avcodec_open2 failed\n");
        return false;
    }
    m_width  = m_codecCtx->width;
    m_height = m_codecCtx->height;

    // Pre-allocate the RGB output frame (size known from header)
    m_frameRGB->format = AV_PIX_FMT_RGB24;
    m_frameRGB->width  = m_width;
    m_frameRGB->height = m_height;
    av_frame_get_buffer(m_frameRGB, 0);

    // swscale is created lazily on first frame so we use the real pixel format
    return true;
}

// Build / rebuild the swscale context from the actual decoded frame's format.
// .mov files (ProRes, HEVC, H264 with full-range flag) often report a different
// pixel format in the codec header than what the decoder actually outputs.
void VideoInput::ensureSwsCtx(AVPixelFormat srcFmt, int srcW, int srcH) {
    // Target output size: use override (e.g. FBO size for camera) or native source size
    int dstW = (m_outW > 0) ? m_outW : srcW;
    int dstH = (m_outH > 0) ? m_outH : srcH;

    if (m_swsCtx && srcFmt == m_lastPixFmt && srcW == m_srcW && srcH == m_srcH
                 && dstW == m_width && dstH == m_height)
        return;

    if (m_swsCtx) sws_freeContext(m_swsCtx);

    // Reallocate output frame if destination size changed
    if (dstW != m_width || dstH != m_height) {
        av_frame_unref(m_frameRGB);
        m_width  = dstW;
        m_height = dstH;
        m_frameRGB->format = AV_PIX_FMT_RGB24;
        m_frameRGB->width  = dstW;
        m_frameRGB->height = dstH;
        av_frame_get_buffer(m_frameRGB, 0);
    }

    m_swsCtx = sws_getContext(
        srcW, srcH, srcFmt,
        dstW, dstH, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    m_lastPixFmt = srcFmt;
    m_srcW = srcW;
    m_srcH = srcH;
}

AVFrame* VideoInput::nextFrame() {
    if (m_useCGImage) return nextFrameCGImage();

    while (av_read_frame(m_fmtCtx, m_pkt) >= 0) {
        if (m_pkt->stream_index != m_streamIdx) {
            av_packet_unref(m_pkt);
            continue;
        }
        if (avcodec_send_packet(m_codecCtx, m_pkt) < 0) {
            av_packet_unref(m_pkt);
            continue;
        }
        av_packet_unref(m_pkt);

        if (avcodec_receive_frame(m_codecCtx, m_frame) == 0) {
            // When VideoToolbox is active the frame lives in GPU memory
            // (AV_PIX_FMT_VIDEOTOOLBOX).  Transfer it to a CPU frame first.
            AVFrame* srcFrame = m_frame;
#ifdef __APPLE__
            if (m_useHW && m_frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
                av_frame_unref(m_frameSW);
                if (av_hwframe_transfer_data(m_frameSW, m_frame, 0) < 0) {
                    fprintf(stderr, "VideoInput: HW→CPU frame transfer failed\n");
                    continue;
                }
                m_frameSW->width  = m_frame->width;
                m_frameSW->height = m_frame->height;
                srcFrame = m_frameSW;
            }
#endif
            // Use the actual decoded frame's pixel format — not the header's
            ensureSwsCtx((AVPixelFormat)srcFrame->format,
                         srcFrame->width, srcFrame->height);
            if (!m_swsCtx) continue;

            av_frame_make_writable(m_frameRGB);
            sws_scale(m_swsCtx,
                      srcFrame->data, srcFrame->linesize, 0, srcFrame->height,
                      m_frameRGB->data, m_frameRGB->linesize);
            return m_frameRGB;
        }
    }
    if (m_isCamera) {
        // Live camera: no frame ready yet — caller retries next render frame
        return nullptr;
    }
    // File: loop back to start
    av_seek_frame(m_fmtCtx, m_streamIdx, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(m_codecCtx);
    return nullptr;
}

void VideoInput::releaseFrame(AVFrame* /*frame*/) {
    // m_frameRGB is reused each call — nothing to free.
}

void VideoInput::close() {
    if (m_swsCtx)    { sws_freeContext(m_swsCtx);         m_swsCtx   = nullptr; }
    if (m_cgSwsCtx)  { sws_freeContext(m_cgSwsCtx);       m_cgSwsCtx = nullptr; }
    if (m_codecCtx)  { avcodec_free_context(&m_codecCtx); }
    if (m_fmtCtx)    { avformat_close_input(&m_fmtCtx);   }
    if (m_hwDevCtx)  { av_buffer_unref(&m_hwDevCtx);      m_hwDevCtx = nullptr; }
    m_useHW      = false;
    m_isCamera   = false;
    m_isScreen   = false;
    m_useCGImage = false;
#ifdef __APPLE__
    m_cgWindowID = kCGNullWindowID;
#endif
    m_streamIdx  = -1;
    m_lastPixFmt = AV_PIX_FMT_NONE;
    m_srcW = 0; m_srcH = 0;
    m_outW = 0; m_outH = 0;
    av_frame_unref(m_frameSW);
}

// ── Screen enumeration ────────────────────────────────────────────────────────

std::vector<VideoInput::ScreenInfo> VideoInput::listScreens() {
    std::vector<ScreenInfo> result;
    avdevice_register_all();

#if defined(__APPLE__)
    const AVInputFormat* fmt = av_find_input_format("avfoundation");
    if (!fmt) return result;
    AVDeviceInfoList* devList = nullptr;
    if (avdevice_list_input_sources(fmt, nullptr, nullptr, &devList) >= 0 && devList) {
        int vidIdx = 0;
        for (int i = 0; i < devList->nb_devices; i++) {
            AVDeviceInfo* d = devList->devices[i];
            bool hasVideo = false;
            for (int j = 0; j < d->nb_media_types; j++)
                if (d->media_types[j] == AVMEDIA_TYPE_VIDEO) { hasVideo = true; break; }
            if (!hasVideo) continue;
            std::string name = d->device_description ? d->device_description
                                                     : (d->device_name ? d->device_name : "");
            if (name.find("screen") != std::string::npos ||
                name.find("Screen") != std::string::npos ||
                name.find("display") != std::string::npos) {
                result.push_back({name, std::to_string(vidIdx)});
            }
            vidIdx++;
        }
        avdevice_free_list_devices(&devList);
    }
    if (result.empty())
        fprintf(stderr, "VideoInput: no screen devices found via avfoundation.\n"
                "  → System Settings > Privacy & Security > Screen Recording\n"
                "    and grant access to your terminal app.\n");

#elif defined(__linux__)
    const char* disp = getenv("DISPLAY");
    if (!disp) disp = ":0";

    // Use xrandr to enumerate monitors with their offsets/sizes
    FILE* f = popen("xrandr --listmonitors 2>/dev/null", "r");
    if (f) {
        char line[512];
        bool gotAny = false;
        while (fgets(line, sizeof(line), f)) {
            // Line format: " 0: +*eDP-1 1920/309x1080/173+0+0  eDP-1"
            int idx; char flags[8]; int pw, phys_w, ph, phys_h, ox, oy; char mname[64];
            // Parse the "WxH+OX+OY" geometry block
            if (sscanf(line, " %d: %7s %d/%*dx%d/%*d+%d+%d %63s",
                       &idx, flags, &pw, &ph, &ox, &oy, mname) >= 7) {
                ScreenInfo si;
                si.name   = std::string(mname) + "  (" + std::to_string(pw) + "x"
                          + std::to_string(ph) + "  offset " + std::to_string(ox) + ","
                          + std::to_string(oy) + ")";
                // devStr: "display+ox,oy WxH"  (space separates display from size)
                si.devStr = std::string(disp) + "+" + std::to_string(ox) + ","
                          + std::to_string(oy) + " " + std::to_string(pw) + "x"
                          + std::to_string(ph);
                result.push_back(si);
                gotAny = true;
            }
        }
        pclose(f);
        if (!gotAny)
            result.push_back({"Full screen (" + std::string(disp) + ")",
                               std::string(disp)});
    } else {
        result.push_back({"Full screen (" + std::string(disp) + ")",
                           std::string(disp)});
    }
#elif defined(_WIN32)
    // Enumerate monitors via gdigrab device list
    result.push_back({"Full desktop", "desktop"});
#endif
    return result;
}

// ── Screen open ───────────────────────────────────────────────────────────────

bool VideoInput::openScreenCapture(const std::string& devStr, int fps) {
    close();
    m_isCamera = true;   // live source: no looping
    m_isScreen = true;
    avdevice_register_all();

    const AVInputFormat* fmt = nullptr;
    std::string inputStr;
    AVDictionary* opts = nullptr;
    char fpsBuf[16];
    snprintf(fpsBuf, sizeof(fpsBuf), "%d", fps);

#if defined(__APPLE__)
    fmt = av_find_input_format("avfoundation");
    if (!fmt) { m_isCamera = m_isScreen = false; return false; }
    inputStr = devStr + ":none";  // video_idx:no_audio
    av_dict_set(&opts, "framerate",       fpsBuf, 0);
    av_dict_set(&opts, "capture_cursor",  "1",    0);
    av_dict_set(&opts, "pixel_format",    "bgr0",  0);

#elif defined(__linux__)
    fmt = av_find_input_format("x11grab");
    if (!fmt) {
        fprintf(stderr, "VideoInput: x11grab not available (compile FFmpeg with --enable-x11grab)\n");
        m_isCamera = m_isScreen = false;
        return false;
    }
    // devStr: ":0.0"  or  ":0.0+ox,oy WxH"
    auto space = devStr.find(' ');
    if (space != std::string::npos) {
        inputStr = devStr.substr(0, space);
        av_dict_set(&opts, "video_size", devStr.substr(space + 1).c_str(), 0);
    } else {
        inputStr = devStr;
    }
    av_dict_set(&opts, "framerate",   fpsBuf, 0);
    av_dict_set(&opts, "draw_mouse",  "1",    0);
    av_dict_set(&opts, "probesize",   "32",   0);
#elif defined(_WIN32)
    fmt = av_find_input_format("gdigrab");
    if (!fmt) {
        fprintf(stderr, "VideoInput: gdigrab not available\n");
        m_isCamera = m_isScreen = false;
        return false;
    }
    // devStr: "desktop" or a window title prefix
    inputStr = devStr;
    av_dict_set(&opts, "framerate",  fpsBuf, 0);
    av_dict_set(&opts, "draw_mouse", "1",    0);
#else
    fprintf(stderr, "VideoInput: screen capture not supported on this platform\n");
    m_isCamera = m_isScreen = false;
    return false;
#endif

    m_path = "screen:" + devStr;
    int ret = avformat_open_input(&m_fmtCtx, inputStr.c_str(), fmt, &opts);
    av_dict_free(&opts);

    if (ret != 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "VideoInput: cannot open screen '%s': %s\n", devStr.c_str(), errbuf);
#ifdef __APPLE__
        fprintf(stderr, "  → System Settings > Privacy & Security > Screen Recording\n"
                "    and grant access to your terminal app.\n");
#endif
        m_isCamera = m_isScreen = false;
        return false;
    }
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "VideoInput: screen capture stream info failed\n");
        return false;
    }
    fprintf(stderr, "VideoInput: screen capture '%s' opened\n", devStr.c_str());
    return initCodec();
}

// ── Window enumeration ────────────────────────────────────────────────────────

std::vector<VideoInput::WindowInfo> VideoInput::listWindows() {
    std::vector<WindowInfo> result;

#if defined(__APPLE__)
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!list) return result;

    for (CFIndex i = 0; i < CFArrayGetCount(list); i++) {
        auto* dict = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);

        // Window must be on-screen layer (layer 0 = normal windows)
        int layer = 0;
        if (auto* n = (CFNumberRef)CFDictionaryGetValue(dict, kCGWindowLayer))
            CFNumberGetValue(n, kCFNumberIntType, &layer);
        if (layer != 0) continue;

        // Get window title (may be NULL for some windows)
        std::string title;
        if (auto* s = (CFStringRef)CFDictionaryGetValue(dict, kCGWindowName)) {
            char buf[256] = {};
            if (CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
                title = buf;
        }
        // Fall back to owning application name
        if (title.empty()) {
            if (auto* s = (CFStringRef)CFDictionaryGetValue(dict, kCGWindowOwnerName)) {
                char buf[256] = {};
                if (CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
                    title = std::string("[") + buf + "]";
            }
        }
        if (title.empty()) continue;

        // Bounds
        CGRect bounds = CGRectZero;
        if (auto* bd = (CFDictionaryRef)CFDictionaryGetValue(dict, kCGWindowBounds))
            CGRectMakeWithDictionaryRepresentation(bd, &bounds);
        if (bounds.size.width < 50 || bounds.size.height < 50) continue;

        // Window ID as devStr (for openWindowCapture to determine screen index)
        int winID = 0;
        if (auto* n = (CFNumberRef)CFDictionaryGetValue(dict, kCGWindowNumber))
            CFNumberGetValue(n, kCFNumberIntType, &winID);

        WindowInfo wi;
        wi.title  = title;
        wi.x = (int)bounds.origin.x;
        wi.y = (int)bounds.origin.y;
        wi.w = (int)bounds.size.width;
        wi.h = (int)bounds.size.height;
        wi.devStr = std::to_string(winID);  // opaque: window ID
        result.push_back(wi);
    }
    CFRelease(list);

#elif defined(__linux__)
    // wmctrl -lG: 0xWINID  desktop  x  y  w  h  hostname  title
    FILE* f = popen("wmctrl -lG 2>/dev/null", "r");
    if (!f) {
        fprintf(stderr, "VideoInput: wmctrl not found — install with: sudo apt install wmctrl\n");
        return result;
    }
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        unsigned long winId = 0;
        int desktop = 0, x = 0, y = 0, w = 0, h = 0;
        char host[64] = {};
        char title[512] = {};
        int n = sscanf(line, "0x%lx %d %d %d %d %d %63s %511[^\n]",
                       &winId, &desktop, &x, &y, &w, &h, host, title);
        if (n < 7 || w < 50 || h < 50) continue;
        WindowInfo wi;
        wi.title  = (n >= 8 && title[0]) ? title : "(unnamed)";
        wi.x = x; wi.y = y; wi.w = w; wi.h = h;
        wi.devStr = std::to_string(winId);
        result.push_back(wi);
    }
    pclose(f);
#elif defined(_WIN32)
    struct EnumData { std::vector<WindowInfo>* out; };
    EnumData ed{ &result };
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* ed = reinterpret_cast<EnumData*>(lp);
        if (!IsWindowVisible(hwnd)) return TRUE;
        char title[512] = {};
        GetWindowTextA(hwnd, title, sizeof(title));
        if (!title[0]) return TRUE;
        RECT r{};
        GetWindowRect(hwnd, &r);
        int w = r.right - r.left, h = r.bottom - r.top;
        if (w < 50 || h < 50) return TRUE;
        WindowInfo wi;
        wi.title  = title;
        wi.x = r.left; wi.y = r.top; wi.w = w; wi.h = h;
        wi.devStr = "title=" + std::string(title);
        ed->out->push_back(wi);
        return TRUE;
    }, (LPARAM)&ed);
#endif
    return result;
}

// ── Window capture open ───────────────────────────────────────────────────────

bool VideoInput::openWindowCapture(const WindowInfo& win, int fps) {
    close();
    m_isCamera = true;
    m_isScreen = true;

#if defined(__APPLE__)
#if MAC_OS_X_VERSION_MAX_ALLOWED < 150000
    // macOS 14-: CGImage path reads window compositor buffer directly (works occluded).
    CGWindowID wid = (CGWindowID)std::stoul(win.devStr);
    m_cgWindowID = wid;
    m_useCGImage = true;
    m_width      = (win.w > 0) ? (win.w & ~1) : 2;
    m_height     = (win.h > 0) ? (win.h & ~1) : 2;
    m_path       = "window:" + win.title;
    av_frame_unref(m_frameRGB);
    m_frameRGB->format = AV_PIX_FMT_RGB24;
    m_frameRGB->width  = m_width;
    m_frameRGB->height = m_height;
    av_frame_get_buffer(m_frameRGB, 0);
    fprintf(stderr, "VideoInput: CGImage window capture '%s' (wid %u)  %dx%d\n",
            win.title.c_str(), wid, m_width, m_height);
    return true;
#else
    // macOS 15+: CGWindowListCreateImage and CGDisplayCreateImage are removed.
    // Fall back to avfoundation screen capture of the main display.
    fprintf(stderr, "VideoInput: per-window capture unavailable on macOS 15+ "
            "(ScreenCaptureKit entitlement required). Falling back to screen capture.\n");
    return openScreenCapture("0", fps);
#endif

#elif defined(__linux__)
    avdevice_register_all();
    const AVInputFormat* fmt = av_find_input_format("x11grab");
    if (!fmt) {
        fprintf(stderr, "VideoInput: x11grab not available\n");
        m_isCamera = m_isScreen = false;
        return false;
    }
    const char* disp = getenv("DISPLAY");
    if (!disp) disp = ":0";

    int x = std::max(0, win.x);
    int y = std::max(0, win.y);
    int w = win.w & ~1;
    int h = win.h & ~1;
    if (w < 2) w = 2;
    if (h < 2) h = 2;

    char inputStr[128];
    snprintf(inputStr, sizeof(inputStr), "%s+%d,%d", disp, x, y);
    char sizeBuf[32], fpsBuf[16];
    snprintf(sizeBuf, sizeof(sizeBuf), "%dx%d", w, h);
    snprintf(fpsBuf,  sizeof(fpsBuf),  "%d",    fps);

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "video_size",  sizeBuf, 0);
    av_dict_set(&opts, "framerate",   fpsBuf,  0);
    av_dict_set(&opts, "draw_mouse",  "1",     0);
    av_dict_set(&opts, "probesize",   "32",    0);

    m_path = "window:" + win.title;
    int ret = avformat_open_input(&m_fmtCtx, inputStr, fmt, &opts);
    av_dict_free(&opts);

    if (ret != 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "VideoInput: cannot open window region '%s': %s\n",
                win.title.c_str(), errbuf);
        m_isCamera = m_isScreen = false;
        return false;
    }
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "VideoInput: window region stream info failed\n");
        return false;
    }
    fprintf(stderr, "VideoInput: x11grab window capture '%s'  %dx%d @%d,%d\n",
            win.title.c_str(), w, h, x, y);
    return initCodec();

#elif defined(_WIN32)
    avdevice_register_all();
    const AVInputFormat* fmt = av_find_input_format("gdigrab");
    if (!fmt) {
        fprintf(stderr, "VideoInput: gdigrab not available\n");
        m_isCamera = m_isScreen = false;
        return false;
    }

    // gdigrab uses "title=WindowTitle" to capture a specific window
    std::string inputStr = win.devStr;  // "title=..."
    char fpsBuf[16];
    snprintf(fpsBuf, sizeof(fpsBuf), "%d", fps);

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "framerate",  fpsBuf, 0);
    av_dict_set(&opts, "draw_mouse", "1",    0);

    m_path = "window:" + win.title;
    int ret = avformat_open_input(&m_fmtCtx, inputStr.c_str(), fmt, &opts);
    av_dict_free(&opts);

    if (ret != 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "VideoInput: cannot open gdigrab window '%s': %s\n",
                win.title.c_str(), errbuf);
        m_isCamera = m_isScreen = false;
        return false;
    }
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "VideoInput: gdigrab window stream info failed\n");
        return false;
    }
    fprintf(stderr, "VideoInput: gdigrab window capture '%s'\n", win.title.c_str());
    return initCodec();
#else
    fprintf(stderr, "VideoInput: window capture not supported on this platform\n");
    m_isCamera = m_isScreen = false;
    return false;
#endif
}

// ── macOS CoreGraphics / ScreenCaptureKit frame pump ─────────────────────────
// On macOS < 15: uses CGDisplayCreateImage (fast, no entitlement needed).
// On macOS 15+:  those APIs were removed; nextFrameCGImage returns nullptr and
//               openWindowCapture gracefully falls back to avfoundation screen
//               capture so the app still builds and runs.

AVFrame* VideoInput::nextFrameCGImage() {
#ifdef __APPLE__
    if (m_cgWindowID == kCGNullWindowID) return nullptr;

#if MAC_OS_X_VERSION_MAX_ALLOWED < 150000
    // ── macOS 14 and earlier: CGDisplayCreateImage still available ────────────
    CGRect winBounds = CGRectZero;
    CFArrayRef winList = CGWindowListCopyWindowInfo(
        kCGWindowListOptionIncludingWindow, m_cgWindowID);
    if (winList) {
        if (CFArrayGetCount(winList) > 0) {
            auto* d = (CFDictionaryRef)CFArrayGetValueAtIndex(winList, 0);
            if (auto* bd = (CFDictionaryRef)CFDictionaryGetValue(d, kCGWindowBounds))
                CGRectMakeWithDictionaryRepresentation(bd, &winBounds);
        }
        CFRelease(winList);
    }
    CGDirectDisplayID display = CGMainDisplayID();
    CGImageRef fullImg = CGDisplayCreateImage(display);
    if (!fullImg) return nullptr;
    size_t dispH = CGImageGetHeight(fullImg);
    CGRect cropRect = CGRectMake(winBounds.origin.x,
                                 (CGFloat)dispH - winBounds.origin.y - winBounds.size.height,
                                 winBounds.size.width, winBounds.size.height);
    CGImageRef img = (winBounds.size.width > 0 && winBounds.size.height > 0)
        ? CGImageCreateWithImageInRect(fullImg, cropRect)
        : fullImg;
    CGImageRelease(fullImg);
    if (!img) return nullptr;
#else
    // ── macOS 15+: CGDisplayCreateImage removed — return nullptr ─────────────
    // Window capture is not available without ScreenCaptureKit entitlement.
    // openWindowCapture() falls back to avfoundation screen capture on macOS 15,
    // so this path is normally unreachable.
    return nullptr;
#endif // MAC_OS_X_VERSION_MAX_ALLOWED

    size_t imgW     = CGImageGetWidth(img);
    size_t imgH     = CGImageGetHeight(img);
    size_t rowBytes = CGImageGetBytesPerRow(img);

    CGDataProviderRef dp  = CGImageGetDataProvider(img);
    CFDataRef rawData     = CGDataProviderCopyData(dp);
    if (!rawData) { CGImageRelease(img); return nullptr; }

    const uint8_t* pixels = CFDataGetBytePtr(rawData);

    int dstW = (m_outW > 0) ? m_outW : (int)(imgW & ~1);
    int dstH = (m_outH > 0) ? m_outH : (int)(imgH & ~1);
    if (dstW < 2) dstW = 2;
    if (dstH < 2) dstH = 2;

    if (dstW != m_width || dstH != m_height || !m_cgSwsCtx) {
        if (m_cgSwsCtx) { sws_freeContext(m_cgSwsCtx); m_cgSwsCtx = nullptr; }
        av_frame_unref(m_frameRGB);
        m_width  = dstW;
        m_height = dstH;
        m_frameRGB->format = AV_PIX_FMT_RGB24;
        m_frameRGB->width  = dstW;
        m_frameRGB->height = dstH;
        av_frame_get_buffer(m_frameRGB, 0);
    }
    if (!m_cgSwsCtx) {
        m_cgSwsCtx = sws_getContext(
            (int)imgW, (int)imgH, AV_PIX_FMT_BGRA,
            dstW, dstH, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
    }
    if (!m_cgSwsCtx) { CFRelease(rawData); CGImageRelease(img); return nullptr; }

    av_frame_make_writable(m_frameRGB);
    const uint8_t* srcData[4]   = { pixels, nullptr, nullptr, nullptr };
    int            srcStride[4] = { (int)rowBytes, 0, 0, 0 };
    sws_scale(m_cgSwsCtx, srcData, srcStride, 0, (int)imgH,
              m_frameRGB->data, m_frameRGB->linesize);

    CFRelease(rawData);
    CGImageRelease(img);
    return m_frameRGB;
#else
    return nullptr;
#endif
}

// ── Virtual display (Linux Xvfb) ─────────────────────────────────────────────

std::string VideoInput::launchVirtualDisplay(int displayNum, int w, int h) {
#ifdef __linux__
    // Check Xvfb is available
    if (system("which Xvfb >/dev/null 2>&1") != 0) {
        fprintf(stderr, "VideoInput: Xvfb not found — install with: sudo apt install xvfb\n");
        return "";
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "Xvfb :%d -screen 0 %dx%dx24 +iglx &>/dev/null &",
             displayNum, w, h);
    if (system(cmd) != 0) {
        fprintf(stderr, "VideoInput: failed to launch Xvfb :%d\n", displayNum);
        return "";
    }
    // Give Xvfb a moment to start
    struct timespec ts = {0, 300000000L};  // 300 ms
    nanosleep(&ts, nullptr);

    char dispStr[16];
    snprintf(dispStr, sizeof(dispStr), ":%d", displayNum);
    fprintf(stderr, "VideoInput: Xvfb started on %s  (%dx%d)\n", dispStr, w, h);
    return dispStr;
#else
    (void)displayNum; (void)w; (void)h;
    return "";
#endif
}
