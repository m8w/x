// AudioCapture_mac.mm — macOS audio capture via AVAudioEngine + Accelerate vDSP FFT
// Ported from MilkDropMac/Engine/AudioEngine.swift

#import <AVFoundation/AVFoundation.h>
#import <Accelerate/Accelerate.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>

#include "IAudioCapture.h"

// ---------------------------------------------------------------------------

static constexpr int kFFTSize  = 1024;
static constexpr int kHopSize  = 512;
static constexpr int kHalfN    = kFFTSize / 2;
static constexpr int kRingSize = kFFTSize * 4;

class AudioCapture_mac : public IAudioCapture {
public:
    AudioCapture_mac();
    ~AudioCapture_mac() override;

    bool start() override;
    void stop()  override;
    bool isRunning() const override { return m_running.load(); }
    AudioData poll()               override;
    std::vector<std::string> listDevices() override;
    bool setDevice(const std::string& name) override;
    std::string currentDevice() const override { return m_deviceName; }

private:
    void setupFFT();
    void buildHannWindow();
    void processSamples(const float* samples, int count);
    AudioData computeAudioData();
    float computeRMS(const float* data, int count);

    // AVAudioEngine (ObjC objects — held as id to avoid importing ObjC headers
    // into translation units that include this header indirectly)
    AVAudioEngine*    m_engine   = nil;
    AVAudioConverter* m_converter = nil;

    // FFT
    FFTSetup m_fftSetup = nullptr;
    float    m_window[kFFTSize]{};
    float    m_ring[kRingSize]{};
    int      m_ringWrite = 0;

    // Split-complex buffers reused every frame
    float    m_real[kHalfN]{};
    float    m_imag[kHalfN]{};
    float    m_mag[kHalfN]{};

    // Smoothing state
    float m_bassSmooth   = 0.f;
    float m_midSmooth    = 0.f;
    float m_trebleSmooth = 0.f;
    float m_rmsSmooth    = 0.f;
    static constexpr float kSmooth = 0.7f;

    // Latest computed frame (written by audio callback, read by poll())
    std::mutex    m_frameMtx;
    AudioData     m_latest{};

    std::atomic<bool> m_running{false};
    std::string       m_deviceName;  // empty = system default
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AudioCapture_mac::AudioCapture_mac() {
    m_engine = [[AVAudioEngine alloc] init];
    setupFFT();
    buildHannWindow();
    m_latest = AudioData::silence();
}

AudioCapture_mac::~AudioCapture_mac() {
    stop();
    if (m_fftSetup) vDSP_destroy_fftsetup(m_fftSetup);
}

void AudioCapture_mac::setupFFT() {
    vDSP_Length log2n = (vDSP_Length)log2((double)kFFTSize);
    m_fftSetup = vDSP_create_fftsetup(log2n, FFTRadix(FFT_RADIX2));
}

void AudioCapture_mac::buildHannWindow() {
    for (int i = 0; i < kFFTSize; ++i) {
        float n = (float)i;
        float N = (float)kFFTSize;
        m_window[i] = 0.5f * (1.f - cosf(2.f * M_PI * n / (N - 1.f)));
    }
}

// ---------------------------------------------------------------------------
// Device enumeration
// ---------------------------------------------------------------------------

std::vector<std::string> AudioCapture_mac::listDevices() {
    std::vector<std::string> result;
    result.push_back("");   // index 0 = system default

    NSArray* deviceTypes;
    if (@available(macOS 14.0, *)) {
        deviceTypes = @[AVCaptureDeviceTypeMicrophone];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        deviceTypes = @[AVCaptureDeviceTypeExternalUnknown, AVCaptureDeviceTypeMicrophone];
#pragma clang diagnostic pop
    }
    AVCaptureDeviceDiscoverySession* session =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:deviceTypes
                                  mediaType:AVMediaTypeAudio
                                   position:AVCaptureDevicePositionUnspecified];

    for (AVCaptureDevice* dev in session.devices) {
        result.push_back(dev.localizedName.UTF8String);
    }
    return result;
}

bool AudioCapture_mac::setDevice(const std::string& name) {
    m_deviceName = name;
    return true;
}

// ---------------------------------------------------------------------------
// Start / Stop
// ---------------------------------------------------------------------------

bool AudioCapture_mac::start() {
    if (m_running.load()) return true;

    AVAudioInputNode* inputNode = m_engine.inputNode;
    [inputNode removeTapOnBus:0];

    AVAudioFormat* inputFormat = [inputNode inputFormatForBus:0];
    AVAudioFormat* targetFormat = [[AVAudioFormat alloc]
        initWithCommonFormat:AVAudioPCMFormatFloat32
                  sampleRate:44100
                    channels:1
                 interleaved:NO];

    m_converter = [[AVAudioConverter alloc] initFromFormat:inputFormat toFormat:targetFormat];

    // Use a raw pointer guarded by the m_running atomic — the tap is removed in stop()
    // before this object can be destroyed, so the pointer is always valid while the tap fires.
    AudioCapture_mac* rawSelf = this;
    [inputNode installTapOnBus:0
                    bufferSize:512
                        format:inputFormat
                         block:^(AVAudioPCMBuffer* buffer, AVAudioTime*) {
        AudioCapture_mac* self = rawSelf;
        if (!self->m_running.load()) return;

        AVAudioFrameCount frameCount = buffer.frameLength;
        AVAudioPCMBuffer* converted = [[AVAudioPCMBuffer alloc]
            initWithPCMFormat:targetFormat
                frameCapacity:frameCount];

        NSError* err = nil;
        [self->m_converter convertToBuffer:converted
                                     error:&err
                        withInputFromBlock:^AVAudioBuffer*(AVAudioPacketCount, AVAudioConverterInputStatus* status) {
            *status = AVAudioConverterInputStatus_HaveData;
            return buffer;
        }];

        if (err || !converted.floatChannelData) return;
        converted.frameLength = frameCount;

        const float* data = converted.floatChannelData[0];
        self->processSamples(data, (int)frameCount);
    }];

    [m_engine prepare];
    NSError* startErr = nil;
    if (![m_engine startAndReturnError:&startErr]) {
        NSLog(@"[AudioCapture_mac] Failed to start: %@", startErr.localizedDescription);
        return false;
    }

    m_running.store(true);
    return true;
}

void AudioCapture_mac::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    [m_engine.inputNode removeTapOnBus:0];
    [m_engine stop];
}

// ---------------------------------------------------------------------------
// Audio processing
// ---------------------------------------------------------------------------

void AudioCapture_mac::processSamples(const float* samples, int count) {
    for (int i = 0; i < count; ++i) {
        m_ring[m_ringWrite % kRingSize] = samples[i];
        ++m_ringWrite;
    }

    if (m_ringWrite >= kHopSize) {
        AudioData data = computeAudioData();
        std::lock_guard<std::mutex> lk(m_frameMtx);
        m_latest = data;
    }
}

AudioData AudioCapture_mac::computeAudioData() {
    // Extract kFFTSize samples from ring buffer
    float pcm[kFFTSize];
    int start = (m_ringWrite - kFFTSize + kRingSize) % kRingSize;
    for (int i = 0; i < kFFTSize; ++i)
        pcm[i] = m_ring[(start + i) % kRingSize];

    // Waveform — first 512 samples
    AudioData out{};
    for (int i = 0; i < 512; ++i)
        out.waveform[i] = pcm[i];

    // Apply Hann window
    float windowed[kFFTSize];
    vDSP_vmul(pcm, 1, m_window, 1, windowed, 1, kFFTSize);

    // Forward FFT via split-complex
    DSPSplitComplex sc{ m_real, m_imag };
    windowed[0] = 0.f;  // zero DC to avoid reinterpret aliasing issues
    vDSP_ctoz((DSPComplex*)windowed, 2, &sc, 1, kHalfN);

    vDSP_Length log2n = (vDSP_Length)log2((double)kFFTSize);
    vDSP_fft_zrip(m_fftSetup, &sc, 1, log2n, FFTDirection(FFT_FORWARD));

    vDSP_zvabs(&sc, 1, m_mag, 1, kHalfN);

    // Scale
    float scale = 2.f / (float)kFFTSize;
    vDSP_vsmul(m_mag, 1, &scale, m_mag, 1, kHalfN);

    // Reduce to 256 bins (linear grouping — sufficient for visualisation)
    {
        int ratio = kHalfN / 256;
        for (int i = 0; i < 256; ++i) {
            float sum = 0.f;
            for (int j = 0; j < ratio; ++j)
                sum += m_mag[i * ratio + j];
            out.spectrum[i] = sum / (float)ratio;
        }
    }

    // Band RMS (bin Hz = sampleRate / fftSize = 44100/1024 ≈ 43 Hz)
    const double binHz = 44100.0 / (double)kFFTSize;
    int bassEnd   = (int)(200.0  / binHz);
    int midEnd    = (int)(2000.0 / binHz);
    bassEnd  = std::min(bassEnd,  kHalfN - 1);
    midEnd   = std::min(midEnd,   kHalfN - 1);

    float bassRaw   = computeRMS(m_mag + 1,        bassEnd - 1);
    float midRaw    = computeRMS(m_mag + bassEnd,   midEnd - bassEnd);
    float trebleRaw = computeRMS(m_mag + midEnd,    kHalfN - midEnd);
    float rmsRaw    = computeRMS(pcm, kFFTSize);

    m_bassSmooth   = m_bassSmooth   * kSmooth + bassRaw   * (1.f - kSmooth);
    m_midSmooth    = m_midSmooth    * kSmooth + midRaw    * (1.f - kSmooth);
    m_trebleSmooth = m_trebleSmooth * kSmooth + trebleRaw * (1.f - kSmooth);
    m_rmsSmooth    = m_rmsSmooth    * kSmooth + rmsRaw    * (1.f - kSmooth);

    out.bass   = m_bassSmooth;
    out.mid    = m_midSmooth;
    out.treble = m_trebleSmooth;
    out.rms    = m_rmsSmooth;

    float bassLevel = std::min(m_bassSmooth * 3.f, 1.f);
    out.bassLevel = bassLevel;
    out.bassAttn  = std::max(bassLevel - 0.4f, 0.f) / 0.6f;

    return out;
}

float AudioCapture_mac::computeRMS(const float* data, int count) {
    if (count <= 0) return 0.f;
    float sumSq = 0.f;
    vDSP_svesq(data, 1, &sumSq, (vDSP_Length)count);
    return sqrtf(sumSq / (float)count);
}

// ---------------------------------------------------------------------------
// poll()
// ---------------------------------------------------------------------------

AudioData AudioCapture_mac::poll() {
    std::lock_guard<std::mutex> lk(m_frameMtx);
    return m_latest;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<IAudioCapture> createAudioCapture() {
    return std::make_unique<AudioCapture_mac>();
}
