#include "FftChain.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void FftChain::fft(std::complex<float>* buf, int n, bool inverse) {
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }
    // Cooley-Tukey iterative butterfly
    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * (float)M_PI / len * (inverse ? 1.0f : -1.0f);
        std::complex<float> wlen(cosf(ang), sinf(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; j++) {
                auto u = buf[i + j];
                auto v = buf[i + j + len / 2] * w;
                buf[i + j]           = u + v;
                buf[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        float s = 1.0f / n;
        for (int i = 0; i < n; i++) buf[i] *= s;
    }
}

void FftChain::process(float** planes, int nbSamples, int channels, int /*sampleRate*/) {
    if (!enabled || nbSamples <= 0 || channels < 1) return;

    const int N    = FFT_SIZE;
    const int half = N / 2;

    // Band bin boundaries: bass / low-mid / high-mid / high
    // At 44100 Hz / 1024 bins: ~43 Hz/bin
    static const int kBandLim[5] = { 0, 16, 64, 256, half };

    std::vector<std::complex<float>> buf(N);
    float rawEnergy[4] = {};

    for (int ch = 0; ch < std::min(channels, 2); ch++) {
        float* samples = planes[ch];
        int copyLen    = std::min(nbSamples, N);

        // Load into FFT buffer (zero-pad to N)
        for (int i = 0; i < N; i++)
            buf[i] = (i < copyLen) ? std::complex<float>(samples[i], 0.0f)
                                   : std::complex<float>(0.0f, 0.0f);

        fft(buf.data(), N, false);

        // ── Spectral gate ──────────────────────────────────────────────────
        if (gate > 0.0f) {
            float thresh = gate * 0.05f;
            for (int i = 0; i < N; i++)
                if (std::abs(buf[i]) < thresh) buf[i] = 0.0f;
        }

        // ── Frequency shift (bin rotation) ────────────────────────────────
        if (fabsf(freqShift) > 0.001f) {
            int shift = (int)(freqShift * half);  // +/-half at extremes
            if (shift != 0) {
                std::vector<std::complex<float>> shifted(N, {0.0f, 0.0f});
                for (int i = 0; i < N; i++) {
                    int j = ((i + shift) % N + N) % N;
                    shifted[j] = buf[i];
                }
                buf = std::move(shifted);
            }
        }

        // ── Temporal smear (first channel drives the smear buffer) ────────
        if (smear > 0.0f) {
            for (int i = 1; i < half; i++) {
                float mag      = std::abs(buf[i]);
                float smearedM = mag * (1.0f - smear) + m_prevMag[i] * smear;
                float phase    = std::arg(buf[i]);
                buf[i]     = std::polar(smearedM, phase);
                buf[N - i] = std::conj(buf[i]);
                if (ch == 0) m_prevMag[i] = smearedM;
            }
        }

        // ── Phase scramble ────────────────────────────────────────────────
        if (phaseScram > 0.0f) {
            for (int i = 1; i < half; i++) {
                float mag       = std::abs(buf[i]);
                float origPhase = std::arg(buf[i]);
                // Deterministic but pseudo-random phase offset per bin
                float randPhase = fmodf((float)i * 2.399f + phaseScram * 7.3f,
                                        2.0f * (float)M_PI) * phaseScram;
                buf[i]     = std::polar(mag, origPhase + randPhase);
                buf[N - i] = std::conj(buf[i]);
            }
        }

        // ── Harmonic boost (amplify even-index bins) ──────────────────────
        if (harmonicBoost > 0.0f) {
            float gain = 1.0f + harmonicBoost * 2.0f;
            for (int i = 2; i < half; i += 2) {
                buf[i] *= gain;
                if (N - i > 0 && N - i < N) buf[N - i] = std::conj(buf[i]);
            }
        }

        // ── Band energy (accumulate across channels, average later) ───────
        for (int b = 0; b < 4; b++) {
            float sum = 0.0f;
            for (int i = kBandLim[b]; i < kBandLim[b + 1]; i++)
                sum += std::abs(buf[i]);
            int count = kBandLim[b + 1] - kBandLim[b];
            rawEnergy[b] += (count > 0) ? sum / count : 0.0f;
        }

        // ── AFT: scale bins per adaptive band gain ────────────────────────
        if (aftEnabled) {
            for (int b = 0; b < 4; b++) {
                for (int i = kBandLim[b]; i < kBandLim[b + 1]; i++) {
                    buf[i] *= m_aftGain[b];
                    int mir = N - i;
                    if (mir > 0 && mir < N) buf[mir] = std::conj(buf[i]);
                }
            }
        }

        // ── Inverse FFT back to time domain ──────────────────────────────
        fft(buf.data(), N, true);

        for (int i = 0; i < copyLen; i++) {
            float s = buf[i].real();
            samples[i] = std::max(-1.0f, std::min(1.0f, s));
        }
    }

    // ── Update band energy with exponential smoothing ─────────────────────────
    {
        int numCh = std::min(channels, 2);
        const float kNorm  = 20.0f;   // scale so moderate signal ≈ 0.5
        const float alpha  = 0.25f;   // smoothing
        for (int b = 0; b < 4; b++) {
            float e = (rawEnergy[b] / numCh) * kNorm;
            e = std::min(e, 1.0f);
            bandEnergy[b] = bandEnergy[b] * (1.0f - alpha) + e * alpha;
        }
    }

    // ── AFT: update adaptive gains to equalise band levels ────────────────────
    if (aftEnabled) {
        float totalE = 0.0f;
        for (int b = 0; b < 4; b++) totalE += bandEnergy[b];
        if (totalE > 0.005f) {
            float target = totalE / 4.0f;
            float smooth = aftRate;
            for (int b = 0; b < 4; b++) {
                float desired = (bandEnergy[b] > 0.005f) ? target / bandEnergy[b] : 1.0f;
                desired = std::max(0.1f, std::min(desired, 8.0f));
                m_aftGain[b] = m_aftGain[b] * smooth + desired * (1.0f - smooth);
            }
        }
    }
}
