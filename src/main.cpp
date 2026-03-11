#include "gl_includes.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "renderer/Renderer.h"
#include "renderer/VideoTexture.h"
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include "stream/VideoInput.h"
#include "stream/StreamOutput.h"
#include "ui/EquationEditor.h"

#include <cstdio>
#include <string>
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
    EquationEditor ui(engine, blend, videoIn, streamOut);

    renderer.init();

    // Default video path from argv
    if (argc > 1) videoIn.open(argv[1]);

    double t0 = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Decode next video frame if ready
        if (videoIn.isOpen()) {
            AVFrame* frame = videoIn.nextFrame();
            if (frame) {
                videoTex.upload(frame);
                videoIn.releaseFrame(frame);
            }
        }

        // Render fractal + video blend
        int fw, fh;
        glfwGetFramebufferSize(window, &fw, &fh);
        float t = (float)(glfwGetTime() - t0);

        renderer.render(fw, fh, t, engine, blend, videoTex);

        // Encode frame for RTMP if streaming
        if (streamOut.isStreaming()) {
            streamOut.pushFrame(renderer.fboPixels(fw, fh), fw, fh);
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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
