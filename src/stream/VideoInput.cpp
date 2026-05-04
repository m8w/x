#include "VideoInput.h"
#include <cstdio>
#ifdef __APPLE__
#include <libavutil/hwcontext_videotoolbox.h>
#endif
#ifndef __APPLE__
#include <unistd.h>
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

std::vector<std::string> VideoInput::listCameras() {
    std::vector<std::string> result;
    avdevice_register_all();

#if defined(__APPLE__)
    const AVInputFormat* fmt = av_find_input_format("avfoundation");
    if (!fmt) return result;
    AVDeviceInfoList* devList = nullptr;
    if (avdevice_list_input_sources(fmt, nullptr, nullptr, &devList) >= 0 && devList) {
        for (int i = 0; i < devList->nb_devices; i++) {
            AVDeviceInfo* d = devList->devices[i];
            bool hasVideo = false;
            for (int j = 0; j < d->nb_media_types; j++) {
                if (d->media_types[j] == AVMEDIA_TYPE_VIDEO) { hasVideo = true; break; }
            }
            if (hasVideo) {
                const char* desc = d->device_description ? d->device_description : d->device_name;
                result.push_back(desc);
            }
        }
        avdevice_free_list_devices(&devList);
    }
#elif defined(__linux__)
    for (int i = 0; i < 8; i++) {
        std::string dev = "/dev/video" + std::to_string(i);
        if (access(dev.c_str(), F_OK) == 0)
            result.push_back(dev);
    }
#endif
    return result;
}

// ── Camera open ───────────────────────────────────────────────────────────────

bool VideoInput::openCamera(int idx) {
    close();
    m_isCamera = true;
    avdevice_register_all();

    const AVInputFormat* fmt = nullptr;
    std::string devStr;

#if defined(__APPLE__)
    fmt = av_find_input_format("avfoundation");
    devStr = std::to_string(idx);
#elif defined(__linux__)
    fmt = av_find_input_format("v4l2");
    devStr = "/dev/video" + std::to_string(idx);
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

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "framerate", "30", 0);
#ifdef __APPLE__
    // uyvy422 is the native format for most Mac webcams — avoids colour conversion
    av_dict_set(&opts, "pixel_format", "uyvy422", 0);
#endif

    m_path = "cam:" + devStr;
    int ret = avformat_open_input(&m_fmtCtx, devStr.c_str(), fmt, &opts);
    av_dict_free(&opts);

    if (ret != 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "VideoInput: cannot open camera %d: %s\n", idx, errbuf);
        m_isCamera = false;
        return false;
    }

    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "VideoInput: camera has no stream info\n");
        return false;
    }
    fprintf(stderr, "VideoInput: camera %d opened\n", idx);
    return initCodec();
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
void VideoInput::ensureSwsCtx(AVPixelFormat srcFmt, int w, int h) {
    if (m_swsCtx && srcFmt == m_lastPixFmt && w == m_width && h == m_height)
        return;

    if (m_swsCtx) sws_freeContext(m_swsCtx);

    // If frame dimensions changed, reallocate output frame too
    if (w != m_width || h != m_height) {
        av_frame_unref(m_frameRGB);
        m_width  = w;
        m_height = h;
        m_frameRGB->format = AV_PIX_FMT_RGB24;
        m_frameRGB->width  = w;
        m_frameRGB->height = h;
        av_frame_get_buffer(m_frameRGB, 0);
    }

    m_swsCtx = sws_getContext(
        w, h, srcFmt,
        w, h, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    m_lastPixFmt = srcFmt;
}

AVFrame* VideoInput::nextFrame() {
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
    if (m_codecCtx)  { avcodec_free_context(&m_codecCtx); }
    if (m_fmtCtx)    { avformat_close_input(&m_fmtCtx);   }
    if (m_hwDevCtx)  { av_buffer_unref(&m_hwDevCtx);      m_hwDevCtx = nullptr; }
    m_useHW      = false;
    m_isCamera   = false;
    m_streamIdx  = -1;
    m_lastPixFmt = AV_PIX_FMT_NONE;
    av_frame_unref(m_frameSW);
}
