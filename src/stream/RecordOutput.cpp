#include "RecordOutput.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <sys/stat.h>
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

// ── YouTube size helpers ───────────────────────────────────────────────────────

int RecordOutput::safeBitrateKbps(double targetSecs) {
    if (targetSecs <= 0.0) return 50000;
    // 256 GB limit; reserve 2 GB for mux overhead and audio
    static constexpr double kMaxBytes = (256.0 - 2.0) * 1024.0 * 1024.0 * 1024.0;
    int kbps = (int)((kMaxBytes * 8.0) / targetSecs / 1000.0);
    return std::max(1000, std::min(kbps, 80000));
}

double RecordOutput::estimatedSizeGB(int bitrateKbps, double targetSecs) {
    return (bitrateKbps * 1000.0 / 8.0 * targetSecs) / (1024.0 * 1024.0 * 1024.0);
}

// ── Encoder helpers ────────────────────────────────────────────────────────────

bool RecordOutput::tryOpenVideoEncoder(const char* name, int w, int h) {
    const AVCodec* codec = avcodec_find_encoder_by_name(name);
    if (!codec) return false;

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    ctx->width        = w;
    ctx->height       = h;
    ctx->bit_rate     = (int64_t)bitrateKbps * 1000;
    ctx->time_base    = {1, fps};
    ctx->framerate    = {fps, 1};
    ctx->gop_size     = fps * 2;
    ctx->max_b_frames = 2;      // B-frames ok for file recording (no latency req)
    ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
    ctx->flags       |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (strcmp(name, "libx264") == 0) {
        av_opt_set(ctx->priv_data, "preset",  "medium", 0);
        av_opt_set(ctx->priv_data, "profile", "high",   0);
        ctx->level = (w >= 7000) ? 60 : 51;
    } else if (strcmp(name, "libx265") == 0) {
        av_opt_set(ctx->priv_data, "preset",  "medium", 0);
    } else if (strcmp(name, "h264_nvenc") == 0) {
        av_opt_set(ctx->priv_data, "preset",  "p5",     0);
        av_opt_set(ctx->priv_data, "profile", "high",   0);
        ctx->level = (w >= 7000) ? 60 : 51;
    } else if (strcmp(name, "hevc_nvenc") == 0) {
        av_opt_set(ctx->priv_data, "preset",  "p5",     0);
    } else if (strcmp(name, "h264_videotoolbox") == 0) {
        av_opt_set(ctx->priv_data, "profile", "high",   0);
        ctx->level = (w >= 7000) ? 60 : 51;
    } else if (strcmp(name, "hevc_videotoolbox") == 0) {
        // Use default settings for HEVC VT
    }

    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        avcodec_free_context(&ctx);
        return false;
    }
    m_vidCtx = ctx;
    return true;
}

bool RecordOutput::openAudioEncoder() {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        fprintf(stderr, "RecordOutput: AAC encoder not found\n");
        return false;
    }
    m_audCtx = avcodec_alloc_context3(codec);
    m_audCtx->sample_rate = 44100;
    m_audCtx->bit_rate    = 192000;   // 192 kbps for recording
    m_audCtx->time_base   = {1, 44100};
    m_audCtx->sample_fmt  = AV_SAMPLE_FMT_FLTP;
    m_audCtx->flags      |= AV_CODEC_FLAG_GLOBAL_HEADER;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&m_audCtx->ch_layout, &stereo);

    if (avcodec_open2(m_audCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_audCtx);
        return false;
    }
    m_samplesPerFrame = (m_audCtx->frame_size > 0) ? m_audCtx->frame_size : 1024;
    return true;
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

bool RecordOutput::start() {
    stop();
    if (outputPath.empty()) {
        fprintf(stderr, "RecordOutput: no output path set\n");
        return false;
    }

    m_width  = recordWidth();
    m_height = recordHeight();

    // Create parent directories
    {
        std::string dir = outputPath;
        auto slash = dir.rfind('/');
        if (slash != std::string::npos) {
            dir = dir.substr(0, slash);
            for (size_t i = 1; i <= dir.size(); ++i) {
                if (i == dir.size() || dir[i] == '/') {
                    mkdir(dir.substr(0, i).c_str(), 0755);
                }
            }
        }
    }

    if (avformat_alloc_output_context2(&m_fmtCtx, nullptr, nullptr,
                                        outputPath.c_str()) < 0) {
        fprintf(stderr, "RecordOutput: cannot alloc output context for %s\n",
                outputPath.c_str());
        return false;
    }

    // Open video encoder — prefer GPU, fall back to CPU
    bool opened = false;
    if (resolution == Resolution::k8K) {
        static const char* k8KEnc[] = {
            "hevc_nvenc", "hevc_videotoolbox",
            "h264_nvenc", "libx265", "libx264", nullptr
        };
        for (int i = 0; k8KEnc[i]; i++) {
            if (tryOpenVideoEncoder(k8KEnc[i], m_width, m_height)) {
                fprintf(stderr, "RecordOutput: 8K encoder = %s\n", k8KEnc[i]);
                opened = true;
                break;
            }
        }
    } else {
        static const char* k4KEnc[] = {
            "h264_nvenc", "h264_videotoolbox", "libx264", nullptr
        };
        for (int i = 0; k4KEnc[i]; i++) {
            if (tryOpenVideoEncoder(k4KEnc[i], m_width, m_height)) {
                fprintf(stderr, "RecordOutput: 4K encoder = %s\n", k4KEnc[i]);
                opened = true;
                break;
            }
        }
    }
    if (!opened) {
        fprintf(stderr, "RecordOutput: no video encoder found for %dx%d\n",
                m_width, m_height);
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
        return false;
    }

    m_vidStream = avformat_new_stream(m_fmtCtx, nullptr);
    avcodec_parameters_from_context(m_vidStream->codecpar, m_vidCtx);
    m_vidStream->time_base = m_vidCtx->time_base;

    if (!openAudioEncoder()) {
        avcodec_free_context(&m_vidCtx);
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
        return false;
    }
    m_audStream = avformat_new_stream(m_fmtCtx, nullptr);
    avcodec_parameters_from_context(m_audStream->codecpar, m_audCtx);
    m_audStream->time_base = m_audCtx->time_base;

    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_fmtCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "RecordOutput: cannot open file %s\n", outputPath.c_str());
            avcodec_free_context(&m_audCtx);
            avcodec_free_context(&m_vidCtx);
            avformat_free_context(m_fmtCtx);
            m_fmtCtx = nullptr;
            return false;
        }
    }

    if (avformat_write_header(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "RecordOutput: write_header failed\n");
        avio_closep(&m_fmtCtx->pb);
        avcodec_free_context(&m_audCtx);
        avcodec_free_context(&m_vidCtx);
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
        return false;
    }

    // Video frame buffer (YUV420P at record resolution)
    m_vidFrame = av_frame_alloc();
    m_vidFrame->format = AV_PIX_FMT_YUV420P;
    m_vidFrame->width  = m_width;
    m_vidFrame->height = m_height;
    av_frame_get_buffer(m_vidFrame, 0);

    // Audio frame + FIFO
    m_samplesPerFrame = (m_audCtx->frame_size > 0) ? m_audCtx->frame_size : 1024;
    m_audFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 2, m_samplesPerFrame * 8);
    m_audFrm  = av_frame_alloc();
    m_audFrm->format      = AV_SAMPLE_FMT_FLTP;
    m_audFrm->sample_rate = 44100;
    m_audFrm->nb_samples  = m_samplesPerFrame;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&m_audFrm->ch_layout, &stereo);
    av_frame_get_buffer(m_audFrm, 0);

    m_pkt    = av_packet_alloc();
    m_vidPts = 0;
    m_audPts = 0;
    m_swsCtx = nullptr;
    m_swsInW = m_swsInH = 0;

    m_lastFrameTime = std::chrono::steady_clock::now() -
                      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<double>(1.0 / fps));

    m_recording = true;
    fprintf(stderr, "RecordOutput: recording %dx%d @ %d kbps fps=%d → %s\n",
            m_width, m_height, bitrateKbps, fps, outputPath.c_str());
    return true;
}

void RecordOutput::stop() {
    if (!m_recording) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_recording) return;
    m_recording = false;

    // Flush video
    if (m_vidCtx) {
        avcodec_send_frame(m_vidCtx, nullptr);
        while (avcodec_receive_packet(m_vidCtx, m_pkt) == 0) {
            av_packet_rescale_ts(m_pkt, m_vidCtx->time_base, m_vidStream->time_base);
            m_pkt->stream_index = m_vidStream->index;
            av_interleaved_write_frame(m_fmtCtx, m_pkt);
            av_packet_unref(m_pkt);
        }
    }

    // Flush audio
    if (m_audCtx) {
        avcodec_send_frame(m_audCtx, nullptr);
        while (avcodec_receive_packet(m_audCtx, m_pkt) == 0) {
            av_packet_rescale_ts(m_pkt, m_audCtx->time_base, m_audStream->time_base);
            m_pkt->stream_index = m_audStream->index;
            av_interleaved_write_frame(m_fmtCtx, m_pkt);
            av_packet_unref(m_pkt);
        }
    }

    if (m_fmtCtx) av_write_trailer(m_fmtCtx);
    if (m_fmtCtx && !(m_fmtCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&m_fmtCtx->pb);

    if (m_swsCtx)   { sws_freeContext(m_swsCtx);        m_swsCtx  = nullptr; }
    if (m_vidFrame) { av_frame_free(&m_vidFrame);        m_vidFrame = nullptr; }
    if (m_audFrm)   { av_frame_free(&m_audFrm);          m_audFrm   = nullptr; }
    if (m_audFifo)  { av_audio_fifo_free(m_audFifo);     m_audFifo  = nullptr; }
    if (m_pkt)      { av_packet_free(&m_pkt);             m_pkt      = nullptr; }
    if (m_audCtx)   { avcodec_free_context(&m_audCtx);   m_audCtx   = nullptr; }
    if (m_vidCtx)   { avcodec_free_context(&m_vidCtx);   m_vidCtx   = nullptr; }
    if (m_fmtCtx)   { avformat_free_context(m_fmtCtx);   m_fmtCtx   = nullptr; }

    m_swsInW = m_swsInH = 0;
    m_vidPts = m_audPts = 0;
    fprintf(stderr, "RecordOutput: stopped → %s\n", outputPath.c_str());
}

// ── Per-frame paths ────────────────────────────────────────────────────────────

void RecordOutput::pushFrame(const uint8_t* rgbData, int srcW, int srcH) {
    if (!m_recording || !rgbData) return;

    // Rate-limit to configured fps (same pattern as StreamOutput)
    auto now = std::chrono::steady_clock::now();
    using Sec = std::chrono::duration<double>;
    double elapsed = Sec(now - m_lastFrameTime).count();
    double period  = 1.0 / fps;
    if (elapsed < period) return;
    m_lastFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(period));
    if (Sec(now - m_lastFrameTime).count() > period)
        m_lastFrameTime = now;

    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_recording || !m_vidCtx) return;

    // Flip rows (glReadPixels returns bottom-up)
    const int stride = srcW * 3;
    std::vector<uint8_t> flipped(stride * srcH);
    for (int row = 0; row < srcH; row++)
        memcpy(flipped.data() + row * stride,
               rgbData + (srcH - 1 - row) * stride, stride);

    if (srcW != m_swsInW || srcH != m_swsInH) {
        if (m_swsCtx) sws_freeContext(m_swsCtx);
        m_swsCtx = sws_getContext(srcW, srcH, AV_PIX_FMT_RGB24,
                                   m_width, m_height, AV_PIX_FMT_YUV420P,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_swsInW = srcW;
        m_swsInH = srcH;
        fprintf(stderr, "RecordOutput: sws %dx%d → %dx%d\n",
                srcW, srcH, m_width, m_height);
    }

    const uint8_t* src[1]    = { flipped.data() };
    int            srcStr[1] = { stride };
    sws_scale(m_swsCtx, src, srcStr, 0, srcH, m_vidFrame->data, m_vidFrame->linesize);
    m_vidFrame->pts = m_vidPts++;
    encodeVideoFrame(m_vidFrame);

    // Drive audio to match video timing; generate silence if FIFO is empty
    int64_t targetSamples = (int64_t)m_vidPts * 44100 / fps;
    while (m_audPts < targetSamples) {
        if (m_audFifo && av_audio_fifo_size(m_audFifo) >= m_samplesPerFrame) {
            av_audio_fifo_read(m_audFifo, (void**)m_audFrm->data, m_samplesPerFrame);
        } else {
            av_samples_set_silence(m_audFrm->data, 0, m_samplesPerFrame, 2,
                                   AV_SAMPLE_FMT_FLTP);
        }
        m_audFrm->pts = m_audPts;
        m_audPts += m_samplesPerFrame;
        encodeAudioFrame(m_audFrm);
    }
}

void RecordOutput::pushAudio(float** planes, int nbSamples) {
    if (!m_recording || !planes || nbSamples <= 0) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_recording || !m_audFifo) return;
    av_audio_fifo_write(m_audFifo, (void**)planes, nbSamples);
}

void RecordOutput::encodeVideoFrame(AVFrame* frame) {
    if (avcodec_send_frame(m_vidCtx, frame) < 0) return;
    while (avcodec_receive_packet(m_vidCtx, m_pkt) == 0) {
        av_packet_rescale_ts(m_pkt, m_vidCtx->time_base, m_vidStream->time_base);
        m_pkt->stream_index = m_vidStream->index;
        av_interleaved_write_frame(m_fmtCtx, m_pkt);
        av_packet_unref(m_pkt);
    }
}

void RecordOutput::encodeAudioFrame(AVFrame* frame) {
    if (avcodec_send_frame(m_audCtx, frame) < 0) return;
    while (avcodec_receive_packet(m_audCtx, m_pkt) == 0) {
        av_packet_rescale_ts(m_pkt, m_audCtx->time_base, m_audStream->time_base);
        m_pkt->stream_index = m_audStream->index;
        av_interleaved_write_frame(m_fmtCtx, m_pkt);
        av_packet_unref(m_pkt);
    }
}
