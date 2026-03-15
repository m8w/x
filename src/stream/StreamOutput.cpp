#include "StreamOutput.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <chrono>
#include <sys/stat.h>   // mkdir / stat for local recording dir creation
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
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
    // Record wall-clock origin at thread start — pts=0 corresponds to this instant.
    // Every packet is paced against this baseline so the RTMP socket never receives
    // data faster than realtime (the root cause of YouTube "faster than realtime" errors).
    auto sinkStart = std::chrono::steady_clock::now();

    while (s.running) {
        std::unique_lock<std::mutex> lock(s.mtx);
        s.cv.wait(lock, [&]{ return !s.queue.empty() || !s.running; });
        while (!s.queue.empty()) {
            AVPacket* pkt = s.queue.front();
            s.queue.pop();
            lock.unlock();

            // Pace to realtime: sleep until the packet's DTS wall-clock moment.
            // Use DTS (decode time) rather than PTS so that B-frame reordering
            // never causes negative sleeps.
            if (pkt->dts != AV_NOPTS_VALUE) {
                AVStream* st = nullptr;
                if (s.avStream    && pkt->stream_index == s.avStream->index)
                    st = s.avStream;
                else if (s.audioStream && pkt->stream_index == s.audioStream->index)
                    st = s.audioStream;
                if (st) {
                    double pktSec = pkt->dts * av_q2d(st->time_base);
                    auto target   = sinkStart +
                                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                        std::chrono::duration<double>(pktSec));
                    auto now = std::chrono::steady_clock::now();
                    if (now < target)
                        std::this_thread::sleep_until(target);
                }
            }

            int wret = av_interleaved_write_frame(s.fmtCtx, pkt);
            if (wret < 0) {
                char errbuf[128];
                av_strerror(wret, errbuf, sizeof(errbuf));
                fprintf(stderr, "[sink:%s] write error: %s\n",
                        s.name.c_str(), errbuf);
            }
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
    // Auto-detect format: "flv" for rtmp(s)://, mp4/mkv/etc. for local paths.
    const bool isLocalFile = s.url.find("://") == std::string::npos;
    if (isLocalFile) {
        // Create parent directory tree (e.g. /Volumes/Seagate/fractal stream/part 1)
        std::string dir = s.url;
        auto slash = dir.rfind('/');
        if (slash != std::string::npos) {
            dir = dir.substr(0, slash);
            // mkdir each component
            for (size_t i = 1; i <= dir.size(); ++i) {
                if (i == dir.size() || dir[i] == '/') {
                    std::string part = dir.substr(0, i);
                    mkdir(part.c_str(), 0755);  // ok if already exists
                }
            }
        }
    }
    if (avformat_alloc_output_context2(&s.fmtCtx, nullptr,
                                       isLocalFile ? nullptr : "flv",
                                       s.url.c_str()) < 0) {
        fprintf(stderr, "[sink:%s] cannot alloc output context\n", s.name.c_str());
        return false;
    }

    s.avStream = avformat_new_stream(s.fmtCtx, nullptr);
    avcodec_parameters_from_context(s.avStream->codecpar, m_codecCtx);
    s.avStream->time_base = m_codecCtx->time_base;

    // YouTube requires an audio track — add a silent AAC stream
    s.audioStream = avformat_new_stream(s.fmtCtx, nullptr);
    avcodec_parameters_from_context(s.audioStream->codecpar, m_audioCtx);
    s.audioStream->time_base = m_audioCtx->time_base;

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

// ── audio encoder (silent AAC — required by YouTube RTMP ingest) ─────────────

bool StreamOutput::openAudioEncoder() {
    const AVCodec* acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!acodec) {
        fprintf(stderr, "StreamOutput: AAC encoder not found\n");
        return false;
    }
    m_audioCtx = avcodec_alloc_context3(acodec);
    m_audioCtx->sample_rate = 44100;
    m_audioCtx->bit_rate    = 128000;
    m_audioCtx->time_base   = {1, 44100};
    m_audioCtx->flags      |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Use FLTP (planar float) — the native format for FFmpeg's AAC encoder
    m_audioCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;

    // Channel layout: stereo
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&m_audioCtx->ch_layout, &stereo);

    if (avcodec_open2(m_audioCtx, acodec, nullptr) < 0) {
        avcodec_free_context(&m_audioCtx);
        return false;
    }

    m_audioSamplesPerFrame = m_audioCtx->frame_size > 0 ? m_audioCtx->frame_size : 1024;

    m_audioFrame = av_frame_alloc();
    m_audioFrame->format      = m_audioCtx->sample_fmt;
    m_audioFrame->sample_rate = m_audioCtx->sample_rate;
    m_audioFrame->nb_samples  = m_audioSamplesPerFrame;
    av_channel_layout_copy(&m_audioFrame->ch_layout, &m_audioCtx->ch_layout);
    av_frame_get_buffer(m_audioFrame, 0);
    av_samples_set_silence(m_audioFrame->data, 0, m_audioFrame->nb_samples,
                           m_audioCtx->ch_layout.nb_channels,
                           (AVSampleFormat)m_audioFrame->format);
    m_audioPts = 0;
    return true;
}

// ── avfoundation audio capture (BlackHole 2ch → decode → resample → AAC) ─────

bool StreamOutput::openAudioCapture(const std::string& device) {
    if (!openAudioEncoder()) return false;   // always need the AAC encoder
    if (device.empty()) return true;         // silence mode — done

    avdevice_register_all();

    const AVInputFormat* ifmt = av_find_input_format("avfoundation");
    if (!ifmt) {
        fprintf(stderr, "StreamOutput: avfoundation not available — using silence\n");
        return true;
    }

    // Colon prefix selects audio-only (no video index before the colon)
    std::string inputStr = ":" + device;
    AVDictionary* opts = nullptr;
    if (avformat_open_input(&m_captureFmtCtx, inputStr.c_str(),
                            const_cast<AVInputFormat*>(ifmt), &opts) < 0) {
        fprintf(stderr, "StreamOutput: cannot open '%s' — using silence\n",
                device.c_str());
        av_dict_free(&opts);
        return true;  // non-fatal
    }
    av_dict_free(&opts);

    avformat_find_stream_info(m_captureFmtCtx, nullptr);

    for (int i = 0; i < (int)m_captureFmtCtx->nb_streams; i++) {
        if (m_captureFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_captureStreamIdx = i;
            break;
        }
    }
    if (m_captureStreamIdx < 0) {
        fprintf(stderr, "StreamOutput: no audio stream in '%s' — using silence\n",
                device.c_str());
        avformat_close_input(&m_captureFmtCtx);
        return true;
    }

    auto* par = m_captureFmtCtx->streams[m_captureStreamIdx]->codecpar;

    // avfoundation can deliver raw PCM whose decoder may not exist in all builds
    const AVCodec* dec = avcodec_find_decoder(par->codec_id);
    if (!dec) {
        fprintf(stderr, "StreamOutput: no decoder for avfoundation audio (codec_id=%d) — using silence\n",
                par->codec_id);
        avformat_close_input(&m_captureFmtCtx);
        m_captureStreamIdx = -1;
        return true;
    }

    m_captureCodecCtx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(m_captureCodecCtx, par);

    // avfoundation sometimes leaves sample_rate / ch_layout uninitialised;
    // supply safe defaults so the resampler doesn't receive zeros.
    if (m_captureCodecCtx->sample_rate <= 0)
        m_captureCodecCtx->sample_rate = 44100;
    if (m_captureCodecCtx->ch_layout.nb_channels <= 0) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&m_captureCodecCtx->ch_layout, &stereo);
    }
    if (m_captureCodecCtx->sample_fmt == AV_SAMPLE_FMT_NONE)
        m_captureCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;

    if (avcodec_open2(m_captureCodecCtx, dec, nullptr) < 0) {
        fprintf(stderr, "StreamOutput: avcodec_open2 failed for capture device — using silence\n");
        avcodec_free_context(&m_captureCodecCtx);
        avformat_close_input(&m_captureFmtCtx);
        m_captureStreamIdx = -1;
        return true;
    }

    // Resampler: whatever avfoundation delivers → FLTP 44100 Hz stereo
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    int swr_ret = swr_alloc_set_opts2(&m_swrCtx,
                        &stereo,                          AV_SAMPLE_FMT_FLTP, 44100,
                        &m_captureCodecCtx->ch_layout,    m_captureCodecCtx->sample_fmt,
                        m_captureCodecCtx->sample_rate,
                        0, nullptr);
    if (swr_ret < 0 || swr_init(m_swrCtx) < 0) {
        fprintf(stderr, "StreamOutput: swr init failed — using silence\n");
        if (m_swrCtx) { swr_free(&m_swrCtx); m_swrCtx = nullptr; }
        avcodec_free_context(&m_captureCodecCtx);
        avformat_close_input(&m_captureFmtCtx);
        m_captureStreamIdx = -1;
        return true;
    }

    m_audioCaptureRunning = true;
    m_audioCaptureThread  = std::thread(&StreamOutput::audioCaptureLoop, this);
    fprintf(stderr, "StreamOutput: audio capture '%s' %d Hz %dch fmt=%s\n",
            device.c_str(), m_captureCodecCtx->sample_rate,
            m_captureCodecCtx->ch_layout.nb_channels,
            av_get_sample_fmt_name(m_captureCodecCtx->sample_fmt));
    return true;
}

void StreamOutput::audioCaptureLoop() {
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    AVAudioFifo* fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 2,
                                             m_audioSamplesPerFrame * 8);
    AVPacket* pkt = av_packet_alloc();
    AVFrame*  dec = av_frame_alloc();
    AVFrame*  res = av_frame_alloc();

    while (m_audioCaptureRunning) {
        int ret = av_read_frame(m_captureFmtCtx, pkt);
        if (ret == AVERROR(EAGAIN)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (ret < 0) break;

        if (pkt->stream_index != m_captureStreamIdx) {
            av_packet_unref(pkt);
            continue;
        }

        avcodec_send_packet(m_captureCodecCtx, pkt);
        av_packet_unref(pkt);

        while (avcodec_receive_frame(m_captureCodecCtx, dec) == 0) {
            if (!m_swrCtx || dec->nb_samples <= 0) { av_frame_unref(dec); continue; }

            // How many output samples will the resampler produce?
            int64_t delay = swr_get_delay(m_swrCtx, m_captureCodecCtx->sample_rate);
            int outSamples = (int)av_rescale_rnd(delay + dec->nb_samples,
                44100, m_captureCodecCtx->sample_rate, AV_ROUND_UP);
            if (outSamples <= 0) { av_frame_unref(dec); continue; }

            res->format      = AV_SAMPLE_FMT_FLTP;
            res->sample_rate = 44100;
            res->nb_samples  = outSamples;
            av_channel_layout_copy(&res->ch_layout, &stereo);
            if (av_frame_get_buffer(res, 0) < 0) { av_frame_unref(dec); continue; }

            int converted = swr_convert(m_swrCtx,
                                        res->data, outSamples,
                                        (const uint8_t**)dec->data, dec->nb_samples);
            if (converted > 0)
                av_audio_fifo_write(fifo, (void**)res->data, converted);

            av_frame_unref(res);
            av_frame_unref(dec);

            // Encode full AAC frames as they become available
            while (av_audio_fifo_size(fifo) >= m_audioSamplesPerFrame) {
                AVFrame* enc = av_frame_alloc();
                enc->format      = AV_SAMPLE_FMT_FLTP;
                enc->sample_rate = 44100;
                enc->nb_samples  = m_audioSamplesPerFrame;
                av_channel_layout_copy(&enc->ch_layout, &stereo);
                av_frame_get_buffer(enc, 0);
                av_audio_fifo_read(fifo, (void**)enc->data, m_audioSamplesPerFrame);
                enc->pts  = m_audioPts;
                m_audioPts += m_audioSamplesPerFrame;
                encodeAndDistributeAudio(enc);
                av_frame_free(&enc);
            }
        }
    }

    av_audio_fifo_free(fifo);
    av_frame_free(&res);
    av_frame_free(&dec);
    av_packet_free(&pkt);
}

void StreamOutput::closeAudioCapture() {
    if (m_audioCaptureRunning) {
        m_audioCaptureRunning = false;
        if (m_audioCaptureThread.joinable())
            m_audioCaptureThread.join();
    }
    if (m_swrCtx)          { swr_free(&m_swrCtx);                     m_swrCtx          = nullptr; }
    if (m_captureCodecCtx) { avcodec_free_context(&m_captureCodecCtx); m_captureCodecCtx = nullptr; }
    if (m_captureFmtCtx)   { avformat_close_input(&m_captureFmtCtx);   m_captureFmtCtx   = nullptr; }
    m_captureStreamIdx = -1;
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
    ctx->gop_size     = fps * 2;   // 2-second keyframe interval
    ctx->keyint_min   = fps * 2;   // prevent shorter intervals from scene-cut detection
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
            av_opt_set(ctx->priv_data, "preset",       "p4",  0);
            av_opt_set(ctx->priv_data, "tune",         "ll",  0);
            av_opt_set(ctx->priv_data, "rc",           "cbr", 0);
            av_opt_set(ctx->priv_data, "profile",      "main", 0);
            av_opt_set(ctx->priv_data, "no-scenecut",  "1",   0);  // lock keyframe interval
            ctx->level = 41;
        } else if (strcmp(name, "h264_amf") == 0) {
            av_opt_set(ctx->priv_data, "usage",        "ultralowlatency", 0);
            av_opt_set(ctx->priv_data, "rc",           "cbr",             0);
            av_opt_set(ctx->priv_data, "profile",      "main",            0);
            ctx->level = 41;
        } else if (strcmp(name, "libx264") == 0) {
            av_opt_set(ctx->priv_data, "preset",       "veryfast",    0);
            av_opt_set(ctx->priv_data, "tune",         "zerolatency", 0);
            av_opt_set(ctx->priv_data, "profile",      "main",        0);
            av_opt_set(ctx->priv_data, "sc_threshold", "0",           0);  // disable scene cuts
            ctx->level = 41;
        } else if (strcmp(name, "h264_videotoolbox") == 0) {
            av_opt_set(ctx->priv_data, "profile",      "main", 0);
            av_opt_set(ctx->priv_data, "realtime",     "1",    0);
            ctx->level = 41;
        }
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

    // Open audio (BlackHole capture if available, silence fallback).
    // YouTube won't publish without an audio track.
    if (!openAudioCapture(audioDevice)) {
        fprintf(stderr, "StreamOutput: failed to open audio encoder\n");
        avcodec_free_context(&m_codecCtx);
        if (m_hwDeviceCtx) { av_buffer_unref(&m_hwDeviceCtx); m_vaapi = false; }
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
    m_pts           = 0;
    m_lastFrameTime = std::chrono::steady_clock::now() -
                      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<double>(1.0 / fps));
    m_streaming = true;
    return true;
}

void StreamOutput::stop() {
    if (!m_streaming) return;

    // Stop capture thread before draining the shared AAC encoder
    closeAudioCapture();

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

    // Drain and free audio encoder
    if (m_audioCtx) {
        avcodec_send_frame(m_audioCtx, nullptr);
        while (avcodec_receive_packet(m_audioCtx, m_pkt) == 0) {
            for (auto& sp : m_sinks) {
                if (!sp->connected || !sp->audioStream) continue;
                AVPacket* clone = av_packet_clone(m_pkt);
                av_packet_rescale_ts(clone, m_audioCtx->time_base,
                                     sp->audioStream->time_base);
                clone->stream_index = sp->audioStream->index;
                std::lock_guard<std::mutex> lk(sp->mtx);
                sp->queue.push(clone);
                sp->cv.notify_one();
            }
            av_packet_unref(m_pkt);
        }
    }

    for (auto& sp : m_sinks) closeSink(*sp);

    if (m_swsCtx)       { sws_freeContext(m_swsCtx);        m_swsCtx   = nullptr; }
    if (m_hwFrame)      { av_frame_free(&m_hwFrame);                                }
    if (m_frame)        { av_frame_free(&m_frame);                                  }
    if (m_audioFrame)   { av_frame_free(&m_audioFrame);      m_audioFrame = nullptr; }
    if (m_audioCtx)     { avcodec_free_context(&m_audioCtx); m_audioCtx   = nullptr; }
    if (m_hwDeviceCtx)  { av_buffer_unref(&m_hwDeviceCtx);                           }
    if (m_codecCtx)     { avcodec_free_context(&m_codecCtx);                         }
    m_vaapi     = false;
    m_streaming = false;
    m_swsInW    = 0;
    m_swsInH    = 0;
    m_audioPts  = 0;
}

// ── per-frame path ─────────────────────────────────────────────────────────────

void StreamOutput::pushFrame(const uint8_t* rgbData, int width, int height) {
    if (!m_streaming) return;

    // Rate-limit to the configured stream fps using the wall clock.
    // Without this the render loop (which runs uncapped) sends frames far
    // faster than realtime and YouTube/Twitch reject the stream.
    auto now = std::chrono::steady_clock::now();
    using Sec = std::chrono::duration<double>;
    double elapsed = Sec(now - m_lastFrameTime).count();
    double framePeriod = 1.0 / fps;
    if (elapsed < framePeriod) return;
    // Advance by one period (keeps cadence stable; clamps runaway drift to 2×)
    m_lastFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(framePeriod));
    if (Sec(now - m_lastFrameTime).count() > framePeriod)
        m_lastFrameTime = now;

    // Recreate SwsContext if the window framebuffer size differs from stream
    // resolution — the window can be any size/DPI but we always encode at
    // m_width × m_height.  sws_scale will rescale accordingly.
    if (width != m_swsInW || height != m_swsInH) {
        if (m_swsCtx) sws_freeContext(m_swsCtx);
        AVPixelFormat swFmt = m_vaapi ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
        m_swsCtx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                                   m_width, m_height, swFmt,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_swsInW = width;
        m_swsInH = height;
        fprintf(stderr, "StreamOutput: sws %dx%d→%dx%d\n",
                width, height, m_width, m_height);
    }

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

    // Push silent audio only when there is no live capture thread.
    // When BlackHole is running, audioCaptureLoop() drives audio independently.
    if (!m_audioCaptureRunning && m_audioCtx && m_audioFrame) {
        int64_t targetSamples = m_pts * (int64_t)m_audioCtx->sample_rate / fps;
        while (m_audioPts < targetSamples) {
            m_audioFrame->pts = m_audioPts;
            m_audioPts += m_audioSamplesPerFrame;
            encodeAndDistributeAudio(m_audioFrame);
        }
    }
}

void StreamOutput::encodeAndDistribute(AVFrame* frame) {
    int ret = avcodec_send_frame(m_codecCtx, frame);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "StreamOutput: avcodec_send_frame failed: %s\n", errbuf);
        return;
    }
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

void StreamOutput::encodeAndDistributeAudio(AVFrame* frame) {
    if (avcodec_send_frame(m_audioCtx, frame) < 0) return;
    AVPacket* apkt = av_packet_alloc();
    while (avcodec_receive_packet(m_audioCtx, apkt) == 0) {
        for (auto& sp : m_sinks) {
            if (!sp->connected || !sp->audioStream) continue;
            AVPacket* clone = av_packet_clone(apkt);
            av_packet_rescale_ts(clone, m_audioCtx->time_base,
                                 sp->audioStream->time_base);
            clone->stream_index = sp->audioStream->index;
            std::lock_guard<std::mutex> lk(sp->mtx);
            sp->queue.push(clone);
            sp->cv.notify_one();
        }
        av_packet_unref(apkt);
    }
    av_packet_free(&apkt);
}
