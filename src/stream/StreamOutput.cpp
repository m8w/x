#include "StreamOutput.h"
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
}

// ── helpers ───────────────────────────────────────────────────────────────────

StreamOutput::StreamOutput() {
    m_pkt = av_packet_alloc();
}

StreamOutput::~StreamOutput() {
    stop();
    av_packet_free(&m_pkt);
}

void StreamOutput::addDestination(const std::string& name, const std::string& url) {
    auto s = std::make_unique<DestSink>();
    s->name = name;
    s->url  = url;
    m_sinks.push_back(std::move(s));
}

void StreamOutput::removeDestination(int idx) {
    if (idx < 0 || idx >= (int)m_sinks.size()) return;
    if (m_streaming) closeSink(*m_sinks[idx]);
    m_sinks.erase(m_sinks.begin() + idx);
}

// ── per-sink background thread ────────────────────────────────────────────────

void StreamOutput::sinkThreadFunc(DestSink& s) {
    while (s.running) {
        std::unique_lock<std::mutex> lock(s.mtx);
        s.cv.wait(lock, [&]{ return !s.queue.empty() || !s.running; });
        while (!s.queue.empty()) {
            AVPacket* pkt = s.queue.front();
            s.queue.pop();
            lock.unlock();
            av_interleaved_write_frame(s.fmtCtx, pkt);
            av_packet_free(&pkt);
            lock.lock();
        }
    }
    while (!s.queue.empty()) {
        AVPacket* pkt = s.queue.front(); s.queue.pop();
        av_interleaved_write_frame(s.fmtCtx, pkt);
        av_packet_free(&pkt);
    }
}

bool StreamOutput::openSink(DestSink& s) {
    s.connected = false;
    if (avformat_alloc_output_context2(&s.fmtCtx, nullptr, "flv",
                                       s.url.c_str()) < 0) {
        fprintf(stderr, "[sink:%s] cannot alloc output context\n", s.name.c_str());
        return false;
    }

    s.avStream = avformat_new_stream(s.fmtCtx, nullptr);
    avcodec_parameters_from_context(s.avStream->codecpar, m_codecCtx);
    s.avStream->time_base = m_codecCtx->time_base;

    if (!(s.fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "rtmp_live", "live", 0);
        av_dict_set(&opts, "tls_verify", "0", 0);
        int ret = avio_open2(&s.fmtCtx->pb, s.url.c_str(), AVIO_FLAG_WRITE,
                             nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            fprintf(stderr, "[sink:%s] avio_open2 failed: %s — %s\n",
                    s.name.c_str(), s.url.c_str(), errbuf);
            avformat_free_context(s.fmtCtx);
            s.fmtCtx = nullptr;
            return false;
        }
    }
    if (avformat_write_header(s.fmtCtx, nullptr) < 0) {
        fprintf(stderr, "[sink:%s] write_header failed\n", s.name.c_str());
        avio_closep(&s.fmtCtx->pb);
        avformat_free_context(s.fmtCtx);
        s.fmtCtx = nullptr;
        return false;
    }

    s.running = true;
    s.thread  = std::thread(&StreamOutput::sinkThreadFunc, this, std::ref(s));
    s.connected = true;
    fprintf(stderr, "[sink:%s] connected → %s\n", s.name.c_str(), s.url.c_str());
    return true;
}

void StreamOutput::closeSink(DestSink& s) {
    if (!s.connected) return;
    s.running = false;
    s.cv.notify_all();
    if (s.thread.joinable()) s.thread.join();

    av_interleaved_write_frame(s.fmtCtx, nullptr);
    av_write_trailer(s.fmtCtx);
    if (!(s.fmtCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&s.fmtCtx->pb);
    avformat_free_context(s.fmtCtx);
    s.fmtCtx    = nullptr;
    s.avStream  = nullptr;
    s.connected = false;
}

// ── encoder selection ─────────────────────────────────────────────────────────
//
// Priority order — none of these encoders carry a GPL license:
//   h264_nvenc        NVIDIA GPU     Linux + Windows
//   h264_amf          AMD GPU        Windows
//   h264_vaapi        Intel/AMD GPU  Linux (needs hwdevice context)
//   h264_videotoolbox Apple GPU/ANE  macOS
//   libx264           CPU software   GPL — commercial builds should not ship this
//
// VAAPI requires an extra upload step (CPU NV12 → VAAPI hw frame).
// All other encoders accept CPU-side YUV420P directly.

bool StreamOutput::tryOpenEncoder(const char* name, bool vaapi, int w, int h) {
    const AVCodec* codec = avcodec_find_encoder_by_name(name);
    if (!codec) return false;

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    ctx->codec_id     = AV_CODEC_ID_H264;
    ctx->width        = w;
    ctx->height       = h;
    ctx->bit_rate     = bitrate_kbps * 1000;
    ctx->time_base    = {1, fps};
    ctx->framerate    = {fps, 1};
    ctx->gop_size     = fps * 2;
    ctx->max_b_frames = 0;
    ctx->flags       |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (vaapi) {
        if (av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_VAAPI,
                                   nullptr, nullptr, 0) < 0) {
            avcodec_free_context(&ctx);
            return false;
        }
        AVBufferRef* framesRef = av_hwframe_ctx_alloc(m_hwDeviceCtx);
        auto* fc = (AVHWFramesContext*)framesRef->data;
        fc->format    = AV_PIX_FMT_VAAPI;
        fc->sw_format = AV_PIX_FMT_NV12;
        fc->width     = w;
        fc->height    = h;
        fc->initial_pool_size = 20;
        if (av_hwframe_ctx_init(framesRef) < 0) {
            av_buffer_unref(&framesRef);
            av_buffer_unref(&m_hwDeviceCtx);
            avcodec_free_context(&ctx);
            return false;
        }
        ctx->pix_fmt       = AV_PIX_FMT_VAAPI;
        ctx->hw_frames_ctx = av_buffer_ref(framesRef);
        av_buffer_unref(&framesRef);
        m_vaapi = true;
    } else {
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        if (strcmp(name, "h264_nvenc") == 0) {
            av_opt_set(ctx->priv_data, "preset", "p4",  0);
            av_opt_set(ctx->priv_data, "tune",   "ll",  0);
            av_opt_set(ctx->priv_data, "rc",     "cbr", 0);
        } else if (strcmp(name, "h264_amf") == 0) {
            av_opt_set(ctx->priv_data, "usage", "ultralowlatency", 0);
            av_opt_set(ctx->priv_data, "rc",    "cbr",             0);
        } else if (strcmp(name, "libx264") == 0) {
            av_opt_set(ctx->priv_data, "preset", "veryfast",    0);
            av_opt_set(ctx->priv_data, "tune",   "zerolatency", 0);
        }
        // h264_videotoolbox: default options are fine for streaming
    }

    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        if (m_hwDeviceCtx) { av_buffer_unref(&m_hwDeviceCtx); m_vaapi = false; }
        avcodec_free_context(&ctx);
        return false;
    }

    m_codecCtx = ctx;
    return true;
}

// ── lifecycle ─────────────────────────────────────────────────────────────────

bool StreamOutput::start(int width, int height, int bitrate_kbps_, int fps_) {
    stop();
    bitrate_kbps = bitrate_kbps_;
    fps          = fps_;
    m_width      = width;
    m_height     = height;

    // Try hardware encoders before falling back to libx264.
    static const struct { const char* name; bool vaapi; } kEncoders[] = {
        { "h264_nvenc",        false },   // NVIDIA  (Linux + Windows)
        { "h264_amf",          false },   // AMD     (Windows)
        { "h264_vaapi",        true  },   // VAAPI   (Linux Intel/AMD)
        { "h264_videotoolbox", false },   // Apple   (macOS)
        { "libx264",           false },   // GPL CPU fallback — avoid in commercial builds
    };

    bool opened = false;
    for (auto& enc : kEncoders) {
        if (tryOpenEncoder(enc.name, enc.vaapi, width, height)) {
            fprintf(stderr, "StreamOutput: encoder = %s\n", enc.name);
            if (strcmp(enc.name, "libx264") == 0)
                fprintf(stderr, "StreamOutput: WARNING — libx264 is GPL-licensed. "
                        "A commercial build must use a hardware encoder.\n");
            opened = true;
            break;
        }
    }
    if (!opened) {
        fprintf(stderr, "StreamOutput: no H.264 encoder found\n");
        return false;
    }

    // Open each enabled destination sink
    int connected = 0;
    for (auto& sp : m_sinks)
        if (sp->enabled && openSink(*sp)) connected++;

    if (connected == 0) {
        fprintf(stderr, "StreamOutput: no destinations connected — add at least one\n");
        avcodec_free_context(&m_codecCtx);
        if (m_hwDeviceCtx) { av_buffer_unref(&m_hwDeviceCtx); m_vaapi = false; }
        return false;
    }

    // Software-side frame: NV12 for VAAPI upload path, YUV420P otherwise
    AVPixelFormat swFmt = m_vaapi ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
    m_frame = av_frame_alloc();
    m_frame->format = swFmt;
    m_frame->width  = width;
    m_frame->height = height;
    av_frame_get_buffer(m_frame, 0);

    // Hardware-side frame for VAAPI upload
    if (m_vaapi) {
        m_hwFrame = av_frame_alloc();
        av_hwframe_get_buffer(m_codecCtx->hw_frames_ctx, m_hwFrame, 0);
    }

    m_swsCtx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                               width, height, swFmt,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
    m_pts       = 0;
    m_streaming = true;
    return true;
}

void StreamOutput::stop() {
    if (!m_streaming) return;

    avcodec_send_frame(m_codecCtx, nullptr);
    while (avcodec_receive_packet(m_codecCtx, m_pkt) == 0) {
        for (auto& sp : m_sinks) {
            if (!sp->connected) continue;
            AVPacket* clone = av_packet_clone(m_pkt);
            av_packet_rescale_ts(clone, m_codecCtx->time_base, sp->avStream->time_base);
            clone->stream_index = sp->avStream->index;
            std::lock_guard<std::mutex> lk(sp->mtx);
            sp->queue.push(clone);
            sp->cv.notify_one();
        }
        av_packet_unref(m_pkt);
    }

    for (auto& sp : m_sinks) closeSink(*sp);

    if (m_swsCtx)       { sws_freeContext(m_swsCtx);      m_swsCtx   = nullptr; }
    if (m_hwFrame)      { av_frame_free(&m_hwFrame);                              }
    if (m_frame)        { av_frame_free(&m_frame);                                }
    if (m_hwDeviceCtx)  { av_buffer_unref(&m_hwDeviceCtx);                        }
    if (m_codecCtx)     { avcodec_free_context(&m_codecCtx);                      }
    m_vaapi     = false;
    m_streaming = false;
}

// ── per-frame path ─────────────────────────────────────────────────────────────

void StreamOutput::pushFrame(const uint8_t* rgbData, int width, int height) {
    if (!m_streaming) return;

    // glReadPixels returns bottom-up rows — flip vertically
    const int stride = width * 3;
    std::vector<uint8_t> flipped(stride * height);
    for (int row = 0; row < height; row++)
        memcpy(flipped.data() + row * stride,
               rgbData + (height - 1 - row) * stride, stride);

    const uint8_t* src[1]    = { flipped.data() };
    int            srcStr[1] = { stride };
    sws_scale(m_swsCtx, src, srcStr, 0, height, m_frame->data, m_frame->linesize);

    if (m_vaapi) {
        // Upload CPU NV12 frame to GPU VAAPI frame, then encode
        av_hwframe_transfer_data(m_hwFrame, m_frame, 0);
        m_hwFrame->pts = m_pts++;
        encodeAndDistribute(m_hwFrame);
    } else {
        m_frame->pts = m_pts++;
        encodeAndDistribute(m_frame);
    }
}

void StreamOutput::encodeAndDistribute(AVFrame* frame) {
    if (avcodec_send_frame(m_codecCtx, frame) < 0) return;
    while (avcodec_receive_packet(m_codecCtx, m_pkt) == 0) {
        for (auto& sp : m_sinks) {
            if (!sp->connected) continue;
            AVPacket* clone = av_packet_clone(m_pkt);
            av_packet_rescale_ts(clone, m_codecCtx->time_base, sp->avStream->time_base);
            clone->stream_index = sp->avStream->index;
            std::lock_guard<std::mutex> lk(sp->mtx);
            sp->queue.push(clone);
            sp->cv.notify_one();
        }
        av_packet_unref(m_pkt);
    }
}
