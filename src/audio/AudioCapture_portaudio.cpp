// AudioCapture_portaudio.cpp — Linux / Windows audio capture stub via PortAudio
// TODO Phase 1 (Linux/Windows port): implement using PortAudio + kiss_fft
//
// To complete this file:
//   1. find_package(PortAudio) in CMakeLists.txt  (or use RtAudio which is
//      already present as an RtMidi dependency)
//   2. Add third_party/kissfft as a submodule (or use <fftw3.h>)
//   3. Implement AudioCapture_portaudio mirroring AudioCapture_mac:
//      - Pa_OpenDefaultStream with paFloat32 mono 44100 Hz
//      - ring-buffer tap identical to the macOS version
//      - kiss_fftr in place of vDSP_fft_zrip
//      - same band-RMS and smoothing logic

#include "IAudioCapture.h"
#include <cstdio>

class AudioCapture_portaudio : public IAudioCapture {
public:
    bool start() override {
        fprintf(stderr, "[AudioCapture] PortAudio backend not yet implemented.\n");
        return false;
    }
    void stop()  override {}
    bool isRunning() const override { return false; }
    AudioData poll() override { return AudioData::silence(); }
    std::vector<std::string> listDevices() override { return {"(not available)"}; }
    bool setDevice(const std::string&) override { return false; }
    std::string currentDevice() const override { return ""; }
};

std::unique_ptr<IAudioCapture> createAudioCapture() {
    return std::make_unique<AudioCapture_portaudio>();
}
