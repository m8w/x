#pragma once
#include <string>
#include <vector>
#include <memory>

// ---------------------------------------------------------------------------
// AudioData — one frame of processed audio, polled each render tick
// ---------------------------------------------------------------------------
struct AudioData {
    float waveform[512];   // raw PCM samples (mono, normalised -1..1)
    float spectrum[256];   // FFT magnitude bins (0–22 kHz, log-reduced)
    float bass;            // 20–200 Hz RMS (smoothed)
    float mid;             // 200–2000 Hz RMS (smoothed)
    float treble;          // 2–20 kHz RMS (smoothed)
    float rms;             // overall RMS (smoothed)
    float bassLevel;       // normalised beat band 0–1
    float bassAttn;        // attenuated bass for hardcut detection (0–1)

    static AudioData silence() {
        AudioData d{};
        return d;
    }
};

// ---------------------------------------------------------------------------
// IAudioCapture — platform-agnostic audio capture interface
//
// Implementations:
//   AudioCapture_mac.mm   — macOS: AVAudioEngine + Accelerate vDSP FFT
//   AudioCapture_portaudio.cpp — Linux / Windows: PortAudio + kiss_fft
//
// Usage in render loop:
//   auto audio = capture->poll();   // non-blocking, returns latest frame
// ---------------------------------------------------------------------------
class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    // Start capturing from the currently selected device.
    // Returns false if the device could not be opened.
    virtual bool start() = 0;

    // Stop capturing and release the device.
    virtual void stop() = 0;

    // True once start() has succeeded and stop() has not been called.
    virtual bool isRunning() const = 0;

    // Non-blocking. Returns the most recently computed AudioData frame.
    // If no frame has been produced yet, returns AudioData::silence().
    virtual AudioData poll() = 0;

    // Returns a list of available input device names.
    virtual std::vector<std::string> listDevices() = 0;

    // Select a device by name (empty string = system default).
    // Takes effect on the next call to start().
    virtual bool setDevice(const std::string& name) = 0;

    // Name of the currently selected device (empty = system default).
    virtual std::string currentDevice() const = 0;
};

// ---------------------------------------------------------------------------
// Factory — implemented in the platform-specific .mm / .cpp file
// ---------------------------------------------------------------------------
std::unique_ptr<IAudioCapture> createAudioCapture();
