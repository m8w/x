#include "StreamOutput.h"
#include <cstdio>
#include <cstring>

StreamOutput::StreamOutput() {
    m_pkt = av_packet_alloc();
}

StreamOutput::~StreamOutput() {
    stop();
    av_packet_free(&m_pkt);
}

bool StreamOutput::start(const std::string& rtmpUrl, int width, int height,
                          int bitrate_kbps_, int fps_) {
    stop();
    bitrate_kbps = bitrate_kbps_;
    fps          = fps_;

    // Output format context for RTMP/FLV
    if (avformat_alloc_output_context2(&m_fmtCtx, nullptr, "flv",
                                       rtmpUrl.c_str()) < 0) {
        fprintf(stderr, "StreamOutput: cannot alloc output context\n");
        return false;
    }

    // H.264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "StreamOutput: H.264 encoder not found\n");
        return false;
    }
    m_stream   = avformat_new_stream(m_fmtCtx, nullptr);
    m_codecCtx = avcodec_alloc_context3(codec);

    m_codecCtx->codec_id   = AV_CODEC_ID_H264;
    m_codecCtx->width      = width;
    m_codecCtx->height     = height;
    m_codecCtx->bit_rate   = bitrate_kbps * 1000;
    m_codecCtx->time_base  = {1, fps};
    m_codecCtx->framerate  = {fps, 1};
    m_codecCtx->gop_size   = fps * 2;
    m_codecCtx->max_b_frames = 0;
    m_codecCtx->pix_fmt    = AV_PIX_FMT_YUV420P;

    // Low-latency x264 preset for streaming
    av_opt_set(m_codecCtx->priv_data, "preset", "veryfast", 0);
    av_opt_set(m_codecCtx->priv_data, "tune",   "zerolatency", 0);

    if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        fprintf(stderr, "StreamOutput: avcodec_open2 failed\n");
        return false;
    }
    avcodec_parameters_from_context(m_stream->codecpar, m_codecCtx);
    m_stream->time_base = m_codecCtx->time_base;

    // Open RTMP connection
    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_fmtCtx->pb, rtmpUrl.c_str(), AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "StreamOutput: avio_open failed for %s\n", rtmpUrl.c_str());
            return false;
        }
    }
    if (avformat_write_header(m_fmtCtx, nullptr) < 0) {
        fprintf(stderr, "StreamOutput: avformat_write_header failed\n");
        return false;
    }

    // Allocate YUV420 frame
    m_frame = av_frame_alloc();
    m_frame->format = AV_PIX_FMT_YUV420P;
    m_frame->width  = width;
    m_frame->height = height;
    av_frame_get_buffer(m_frame, 0);

    // sws: RGB24 (flipped) → YUV420P
    m_swsCtx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                               width, height, AV_PIX_FMT_YUV420P,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
    m_pts      = 0;
    m_streaming = true;
    return true;
}

void StreamOutput::pushFrame(const uint8_t* rgbData, int width, int height) {
    if (!m_streaming) return;

    // glReadPixels returns bottom-up; flip vertically
    const int stride = width * 3;
    std::vector<uint8_t> flipped(stride * height);
    for (int row = 0; row < height; row++) {
        memcpy(flipped.data() + row * stride,
               rgbData + (height - 1 - row) * stride, stride);
    }

    const uint8_t* src[1]  = { flipped.data() };
    int            srcStride[1] = { stride };
    sws_scale(m_swsCtx, src, srcStride, 0, height,
              m_frame->data, m_frame->linesize);

    m_frame->pts = m_pts++;
    encodeAndSend(m_frame);
}

bool StreamOutput::encodeAndSend(AVFrame* frame) {
    if (avcodec_send_frame(m_codecCtx, frame) < 0) return false;
    while (avcodec_receive_packet(m_codecCtx, m_pkt) == 0) {
        av_packet_rescale_ts(m_pkt, m_codecCtx->time_base, m_stream->time_base);
        m_pkt->stream_index = m_stream->index;
        av_interleaved_write_frame(m_fmtCtx, m_pkt);
        av_packet_unref(m_pkt);
    }
    return true;
}

void StreamOutput::stop() {
    if (!m_streaming) return;
    // Flush encoder
    avcodec_send_frame(m_codecCtx, nullptr);
    while (avcodec_receive_packet(m_codecCtx, m_pkt) == 0) {
        av_packet_rescale_ts(m_pkt, m_codecCtx->time_base, m_stream->time_base);
        m_pkt->stream_index = m_stream->index;
        av_interleaved_write_frame(m_fmtCtx, m_pkt);
        av_packet_unref(m_pkt);
    }
    av_write_trailer(m_fmtCtx);

    if (m_swsCtx)   { sws_freeContext(m_swsCtx);          m_swsCtx = nullptr; }
    if (m_frame)    { av_frame_free(&m_frame); }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); }
    if (m_fmtCtx) {
        if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&m_fmtCtx->pb);
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    m_streaming = false;
}
