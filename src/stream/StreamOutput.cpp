#include "StreamOutput.h"
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
#include <libavutil/opt.h>
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

// ── encoder ───────────────────────────────────────────────────────────────────
//
// NVIDIA NVENC — hardware H.264 encoder available on any NVIDIA GPU (Kepler+).
// Runs entirely on the Encode Engine, leaving CUDA cores and the CPU free.
// No GPL dependency. Pixel format: NV12 (NVENC's preferred chroma layout).
//
// Streaming-optimised options:
//   preset = p4          balanced speed/quality (p1=fastest … p7=best)
//   tune   = ll          low-latency mode — encoder doesn't buffer B-frames
//   rc     = cbr         constant bitrate — keeps RTMP buffer stable

bool StreamOutput::tryOpenEncoder(const char* /*unused*/, bool /*unused*/,
                                  int width, int height) {
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        fprintf(stderr, "StreamOutput: h264_nvenc not found — "
                "is an NVIDIA GPU present with a recent driver?\n");
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    m_codecCtx->codec_id     = AV_CODEC_ID_H264;
    m_codecCtx->width        = width;
    m_codecCtx->height       = height;
    m_codecCtx->bit_rate     = bitrate_kbps * 1000;
    m_codecCtx->time_base    = {1, fps};
    m_codecCtx->framerate    = {fps, 1};
    m_codecCtx->gop_size     = fps * 2;
    m_codecCtx->max_b_frames = 0;
    m_codecCtx->pix_fmt      = AV_PIX_FMT_NV12;   // NVENC native chroma
    m_codecCtx->flags       |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set(m_codecCtx->priv_data, "preset", "p4",  0);
    av_opt_set(m_codecCtx->priv_data, "tune",   "ll",  0);
    av_opt_set(m_codecCtx->priv_data, "rc",     "cbr", 0);

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        fprintf(stderr, "StreamOutput: avcodec_open2 failed for h264_nvenc\n");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    fprintf(stderr, "StreamOutput: encoder = h264_nvenc (NVIDIA NVENC)\n");
    return true;
}

// ── lifecycle ─────────────────────────────────────────────────────────────────

bool StreamOutput::start(int width, int height, int bitrate_kbps_, int fps_) {
    stop();
    bitrate_kbps = bitrate_kbps_;
    fps          = fps_;
    m_width      = width;
    m_height     = height;

    if (!tryOpenEncoder(nullptr, false, width, height))
        return false;

    int connected = 0;
    for (auto& sp : m_sinks)
        if (sp->enabled && openSink(*sp)) connected++;

    if (connected == 0) {
        fprintf(stderr, "StreamOutput: no destinations connected — add at least one\n");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    // NV12 — NVENC reads this directly without an internal conversion step
    m_frame = av_frame_alloc();
    m_frame->format = AV_PIX_FMT_NV12;
    m_frame->width  = width;
    m_frame->height = height;
    av_frame_get_buffer(m_frame, 0);

    m_swsCtx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                               width, height, AV_PIX_FMT_NV12,
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

    if (m_swsCtx)   { sws_freeContext(m_swsCtx); m_swsCtx  = nullptr; }
    if (m_frame)    { av_frame_free(&m_frame);                          }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx);                }
    m_streaming = false;
}

// ── per-frame path ─────────────────────────────────────────────────────────────

void StreamOutput::pushFrame(const uint8_t* rgbData, int width, int height) {
    if (!m_streaming) return;

    const int stride = width * 3;
    std::vector<uint8_t> flipped(stride * height);
    for (int row = 0; row < height; row++)
        memcpy(flipped.data() + row * stride,
               rgbData + (height - 1 - row) * stride, stride);

    const uint8_t* src[1]    = { flipped.data() };
    int            srcStr[1] = { stride };
    sws_scale(m_swsCtx, src, srcStr, 0, height, m_frame->data, m_frame->linesize);
    m_frame->pts = m_pts++;
    encodeAndDistribute(m_frame);
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
