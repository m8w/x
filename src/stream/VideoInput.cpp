#include "VideoInput.h"
#include <cstdio>

VideoInput::VideoInput() {
    m_frame    = av_frame_alloc();
    m_frameRGB = av_frame_alloc();
    m_pkt      = av_packet_alloc();
}

VideoInput::~VideoInput() {
    close();
    av_frame_free(&m_frame);
    av_frame_free(&m_frameRGB);
    av_packet_free(&m_pkt);
}

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
    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        fprintf(stderr, "VideoInput: avcodec_open2 failed\n");
        return false;
    }
    m_width  = m_codecCtx->width;
    m_height = m_codecCtx->height;
    allocRGBFrame();
    return true;
}

void VideoInput::allocRGBFrame() {
    m_swsCtx = sws_getContext(
        m_width, m_height, m_codecCtx->pix_fmt,
        m_width, m_height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    m_frameRGB->format = AV_PIX_FMT_RGB24;
    m_frameRGB->width  = m_width;
    m_frameRGB->height = m_height;
    av_frame_get_buffer(m_frameRGB, 0);
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
            sws_scale(m_swsCtx,
                      m_frame->data, m_frame->linesize, 0, m_height,
                      m_frameRGB->data, m_frameRGB->linesize);
            return m_frameRGB;
        }
    }
    // Loop: seek back to start
    av_seek_frame(m_fmtCtx, m_streamIdx, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(m_codecCtx);
    return nullptr;
}

void VideoInput::releaseFrame(AVFrame* /*frame*/) {
    // m_frameRGB is reused; nothing to free per-call.
}

void VideoInput::close() {
    if (m_swsCtx)   { sws_freeContext(m_swsCtx);        m_swsCtx = nullptr; }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); }
    if (m_fmtCtx)   { avformat_close_input(&m_fmtCtx);  }
    m_streamIdx = -1;
}
