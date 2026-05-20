#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
}
#include <string>
#include <mutex>
#include <chrono>
#include <cstdint>

// Local file recorder: encodes rendered frames to a 4K or 8K MP4 file.
// Video: H.264 (4K) or HEVC (8K preferred), hardware encoder when available.
// Audio: AAC 128 kbps stereo, fed from StreamOutput's audio capture path.
//
// Thread safety: pushFrame() and pushAudio() may be called from different
// threads; both serialise on an internal mutex.
class RecordOutput {
public:
    enum class Resolution { k4K, k8K };

    // ── Settings (set before start()) ────────────────────────────────────────
    Resolution  resolution   = Resolution::k4K;
    int         bitrateKbps  = 35000;   // 35 Mbps — fits 12h in 256 GB
    int         fps          = 30;
    std::string outputPath;             // full .mp4 path

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    bool start();
    void stop();
    bool isRecording() const { return m_recording; }

    // ── Frame push (render thread) ────────────────────────────────────────────
    // rgbData: packed RGB24 from glReadPixels, bottom-up rows, size srcW*srcH*3
    void pushFrame(const uint8_t* rgbData, int srcW, int srcH);

    // ── Audio push (StreamOutput audio thread) ────────────────────────────────
    // FLTP stereo 44100 Hz — same layout as StreamOutput's captured audio
    void pushAudio(float** planes, int nbSamples);

    // ── YouTube size helpers ──────────────────────────────────────────────────
    // Returns max bitrate (kbps) keeping total file < 256 GB for targetSecs
    static int    safeBitrateKbps(double targetSecs);
    static double estimatedSizeGB(int bitrateKbps, double targetSecs);

    int recordWidth()  const { return (resolution == Resolution::k8K) ? 7680 : 3840; }
    int recordHeight() const { return (resolution == Resolution::k8K) ? 4320 : 2160; }

private:
    AVFormatContext* m_fmtCtx    = nullptr;
    AVCodecContext*  m_vidCtx    = nullptr;
    AVCodecContext*  m_audCtx    = nullptr;
    AVStream*        m_vidStream = nullptr;
    AVStream*        m_audStream = nullptr;
    SwsContext*      m_swsCtx    = nullptr;
    AVFrame*         m_vidFrame  = nullptr;
    AVFrame*         m_audFrm    = nullptr;
    AVPacket*        m_pkt       = nullptr;
    AVAudioFifo*     m_audFifo   = nullptr;
    int64_t          m_vidPts    = 0;
    int64_t          m_audPts    = 0;
    bool             m_recording = false;
    int              m_width     = 0;
    int              m_height    = 0;
    int              m_swsInW    = 0;
    int              m_swsInH    = 0;
    int              m_samplesPerFrame = 1024;

    std::mutex       m_mutex;
    std::chrono::steady_clock::time_point m_lastFrameTime{};

    bool tryOpenVideoEncoder(const char* name, int w, int h);
    bool openAudioEncoder();
    void encodeVideoFrame(AVFrame* frame);
    void encodeAudioFrame(AVFrame* frame);
};
