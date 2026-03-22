#include "gl_includes.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "renderer/Renderer.h"
#include "renderer/VideoTexture.h"
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include "fractal/GlitchEngine.h"
#include "fractal/ColorSynth.h"
#include "stream/VideoInput.h"
#include "stream/StreamOutput.h"
#include "ui/EquationEditor.h"
#include "AppSettings.h"
#include "remote/RemoteControl.h"
#include "midi/MidiInput.h"
#include "midi/MidiMapper.h"
#include "midi/MidiGenerator.h"
#include "midi/MidiOutput.h"
#include "audio/IAudioCapture.h"
#include "audio/BeatDetector.h"
#include "milkdrop/PresetManager.h"
#include "milkdrop/MilkDropGLRenderer.h"

#include <cstdio>
#include <string>
#include <ifaddrs.h>
#include <arpa/inet.h>
extern "C" {
#include <libavutil/log.h>
}

static void glfw_error_cb(int err, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}

int main(int argc, char** argv) {
    // Suppress FFmpeg verbose logs — only show real errors
    av_log_set_level(AV_LOG_ERROR);

    glfwSetErrorCallback(glfw_error_cb);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);   // macOS max = 4.1
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fractal Stream", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

#ifndef __APPLE__
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "GLEW init failed\n");
        return 1;
    }
#endif

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Core objects
    FractalEngine  engine;
    BlendController blend;
    VideoTexture   videoTex;
    Renderer       renderer;
    VideoInput     videoIn;
    StreamOutput   streamOut;
    MidiInput      midiIn;
    MidiOutput     midiOut;
    MidiMapper     midiMapper;
    MidiGenerator  midiGen;
    GlitchEngine   glitchEng;
    ColorSynth     colorSynth;
    EquationEditor ui(engine, blend, glitchEng, colorSynth,
                      videoIn, streamOut, midiIn, midiOut, midiMapper, midiGen);

    // MilkDrop subsystems
    auto           audioCapture = createAudioCapture();
    BeatDetector   beatDet;
    PresetManager  presetMgr;
    MilkDropGLRenderer mdRenderer;

    // Phone remote control — open http://<your-ip>:7777 in a browser
    RemoteControl remote(engine, blend);
    if (remote.start(7777)) {
        // Print all non-loopback IPv4 addresses so user knows what to type
        fprintf(stderr, "\n=== Phone remote ===\n");
        struct ifaddrs* ifap = nullptr;
        if (getifaddrs(&ifap) == 0) {
            for (auto* ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &((sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
                if (std::string(ip) == "127.0.0.1") continue;
                fprintf(stderr, "  http://%s:7777\n", ip);
            }
            freeifaddrs(ifap);
        }
        fprintf(stderr, "====================\n\n");
    }

    renderer.init();

    // MilkDrop init (must happen after GL context is current)
    mdRenderer.init(std::string(SHADERS_DIR));
    presetMgr.loadAll();
    presetMgr.onPresetChanged = [&](MilkDropPreset& p, TransitionType t) {
        if (!mdRenderer.hasPreset() || t == TransitionType::Instant)
            mdRenderer.loadPreset(p);
        else
            mdRenderer.beginTransition(p, (int)t, 2.5f);
    };
    audioCapture->start();
    ui.setMilkDrop(&presetMgr, &mdRenderer, audioCapture.get(), &beatDet);

    // Restore last session (before argv override so explicit path wins)
    ui.loadSettings(AppSettings::lastPath());

    // Default video path from argv
    if (argc > 1) videoIn.open(argv[1]);

    double t0   = glfwGetTime();
    double tPrev = t0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now     = glfwGetTime();
        float  t       = (float)(now - t0);
        float  dt      = (float)(now - tPrev);
        tPrev = now;

        // Poll real MIDI input; optional MIDI-Thru to output port
        std::vector<ColorSynth::Msg> synthMsgs;
        if (midiIn.isOpen()) {
            auto msgs = midiIn.poll();
            for (auto& msg : msgs) {
                midiMapper.apply(msg, engine, blend, colorSynth);
                midiMapper.feedLearn(msg);
                if (midiGen.midiThru && midiOut.isOpen())
                    midiOut.send(msg);  // MIDI Thru
                // Forward to color synth for velocity reaction
                synthMsgs.push_back({msg.status, msg.data1, msg.data2});
            }
        }

        // Tick MIDI generator — drive fractal params AND send real MIDI to VST
        {
            double elapsed = now - t0;

            // Tick glitch engine first (modifies engine/blend, returns ghost notes)
            auto glitchMsgs = glitchEng.tick(elapsed, engine, blend);
            for (auto& msg : glitchMsgs) {
                midiMapper.apply(msg, engine, blend, colorSynth);
                midiOut.send(msg);
                synthMsgs.push_back({msg.status, msg.data1, msg.data2});
            }

            auto genMsgs = midiGen.tick(elapsed);
            for (auto& msg : genMsgs) {
                // Apply MIDI glitch modifiers (velocity spike, pitch scramble)
                auto gMsg = glitchEng.applyMidiGlitch(msg);
                midiMapper.apply(gMsg, engine, blend, colorSynth);  // fractal params
                midiOut.send(gMsg);                                  // real MIDI to DAW/VST
                synthMsgs.push_back({gMsg.status, gMsg.data1, gMsg.data2});
            }
        }

        // Tick color synthesizer — update oscillators + react to MIDI
        colorSynth.tick(t, dt, synthMsgs);

        // Decode next video frame if ready
        if (videoIn.isOpen()) {
            AVFrame* frame = videoIn.nextFrame();
            if (frame) {
                videoTex.upload(frame);
                videoIn.releaseFrame(frame);
            }
        }

        // Poll audio + detect beats
        AudioData audio = audioCapture->poll();
        beatDet.process(audio);

        // Render fractal + video blend
        int fw, fh;
        glfwGetFramebufferSize(window, &fw, &fh);

        renderer.render(fw, fh, t, engine, blend, videoTex, colorSynth);

        // Render MilkDrop frame (if ready) then blit to window
        if (mdRenderer.isReady()) {
            mdRenderer.resize(fw, fh);
            // Only composite the fractal into MilkDrop when the overlay is explicitly on
            GLuint fracTex = ui.mdFractalOverlay() ? renderer.fboTexture() : 0;
            mdRenderer.render(t, dt, audio, fracTex, ui.mdFractalBlend());

            // Blit MilkDrop output over the fractal (full window)
            if (mdRenderer.hasPreset())
                mdRenderer.blitToScreen(fw, fh);

            // Hardcut-triggered preset advance
            if (beatDet.hardcutFired && presetMgr.totalCount() > 0)
                presetMgr.randomPreset(TransitionType::Hardcut);
        }

        // Encode frame for RTMP if streaming.
        // Stream source follows the UI toggle: MilkDrop output when toggled on
        // AND a preset is active, otherwise the fractal FBO.
        if (streamOut.isStreaming()) {
            if (mdRenderer.isReady() && mdRenderer.hasPreset() && ui.streamMilkDrop()) {
                streamOut.pushFrame(mdRenderer.readPixels(fw, fh), fw, fh);
            } else {
                streamOut.pushFrame(renderer.fboPixels(fw, fh), fw, fh);
            }
        }

        // ImGui overlay
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ui.draw();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    streamOut.stop();

    // Auto-save session so next launch resumes where we left off
    ui.saveSettings(AppSettings::lastPath());

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
