#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <memory>

// One RTMP destination (YouTube, Twitch, etc.)
struct DestSink {
    std::string  name;
    std::string  url;
    bool         enabled   = true;
    bool         connected = false;     // set by StreamOutput::start()

    // FFmpeg mux context for this destination (one per sink)
    AVFormatContext* fmtCtx   = nullptr;
    AVStream*        avStream = nullptr;

    // Background send thread + packet queue (so a slow sink can't stall others)
    std::thread             thread;
    std::mutex              mtx;
    std::condition_variable cv;
    std::queue<AVPacket*>   queue;
    std::atomic<bool>       running{false};
};

// Encodes rendered OpenGL frames once, then fans the bitstream out to every
// enabled DestSink over RTMP simultaneously on per-sink background threads.
class StreamOutput {
public:
    StreamOutput();
    ~StreamOutput();

    // ── Destination management (safe to call before start() or after stop()) ──
    void addDestination(const std::string& name, const std::string& url);
    void removeDestination(int idx);
    int  destCount() const { return (int)m_sinks.size(); }
    DestSink&       dest(int i)       { return *m_sinks[i]; }
    const DestSink& dest(int i) const { return *m_sinks[i]; }

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    bool start(int width, int height, int bitrate_kbps = 4000, int fps = 30);
    void stop();
    bool isStreaming() const { return m_streaming; }

    // data = packed RGB24 pixels from glReadPixels, row 0 = bottom-left
    void pushFrame(const uint8_t* rgbData, int width, int height);

    // Settings (apply before start())
    int bitrate_kbps = 4000;
    int fps          = 30;

private:
    std::vector<std::unique_ptr<DestSink>> m_sinks;

    // Shared encoder — encode once, clone packet per sink
    AVCodecContext* m_codecCtx  = nullptr;
    SwsContext*     m_swsCtx    = nullptr;
    AVFrame*        m_frame     = nullptr;
    AVPacket*       m_pkt       = nullptr;
    int64_t         m_pts       = 0;
    bool            m_streaming = false;
    int             m_width     = 0;
    int             m_height    = 0;

    bool openSink(DestSink& s);
    void closeSink(DestSink& s);
    void sinkThreadFunc(DestSink& s);
    void encodeAndDistribute(AVFrame* frame);
};
