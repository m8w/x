#pragma once
#include <vector>
#include <complex>
#include <array>

// FFT / AFT (Adaptive Frequency Transform) spectral processing chain.
//
// Audio path: process() applies spectral effects to FLTP stereo audio
// in-place before encoding to the stream.
//
// Visual path: bandEnergy[4] is updated after each process() call and
// forwarded to the Renderer to drive frequency-band visual distortion
// in the spectral post-process shader pass.
class FftChain {
public:
    // ── Master toggles ────────────────────────────────────────────────────────
    bool enabled  = false;  // master on/off for all effects
    bool onStream = true;   // gate audio + visual distortion to broadcast
    bool onRecord = true;   // gate to recording (future)

    // ── Spectral audio effects (0 = off) ─────────────────────────────────────
    float gate          = 0.0f;  // spectral gate threshold; zeroes quiet bins
    float freqShift     = 0.0f;  // bin rotation in +/- 1.0 units (~half-spectrum)
    float smear         = 0.0f;  // temporal smear: blend previous magnitude
    float phaseScram    = 0.0f;  // randomise bin phases
    float harmonicBoost = 0.0f;  // boost even-harmonic bins

    // ── AFT adaptive mode ─────────────────────────────────────────────────────
    bool  aftEnabled = false;   // auto-equalise band levels
    float aftRate    = 0.5f;    // smoothing: 0=instant  1=frozen

    // ── Per-band visual coupling strength ────────────────────────────────────
    // Index: 0=bass  1=low-mid  2=high-mid  3=high
    float visualGain[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // ── Live output (read after process()) ───────────────────────────────────
    float bandEnergy[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // normalised 0..1

    // Process nbSamples of FLTP stereo audio in-place.
    // planes[0] = left channel, planes[1] = right channel.
    void process(float** planes, int nbSamples, int channels, int sampleRate);

private:
    static constexpr int FFT_SIZE = 1024;

    // Cooley-Tukey in-place radix-2 DIT FFT.
    // inverse=false: forward (no scaling)
    // inverse=true:  inverse (scales by 1/n)
    static void fft(std::complex<float>* buf, int n, bool inverse);

    std::array<float, FFT_SIZE / 2> m_prevMag{};  // for temporal smear
    float m_aftGain[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};
