#ifndef __APPLE__
#include "FilePicker.h"
#include <cstdio>
#include <array>
#include <string>

static std::string zenityRun(const std::string& cmd) {
    std::array<char, 1024> buf{};
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return "";
    std::string path;
    while (fgets(buf.data(), buf.size(), f))
        path += buf.data();
    pclose(f);
    if (!path.empty() && path.back() == '\n') path.pop_back();
    return path;
}

// On Linux use zenity (GTK file chooser) if available, else return "".
std::string pickVideoFile() {
    return zenityRun(
        "zenity --file-selection --title='Choose a video file' "
        "--file-filter='Video files (mp4 mkv mov avi) | *.mp4 *.mkv *.mov *.avi' 2>/dev/null");
}

std::string pickSaveFile(const std::string& suggestedName) {
    std::string cmd =
        "zenity --file-selection --save --confirm-overwrite "
        "--title='Save recording as' "
        "--file-filter='MP4 video | *.mp4' "
        "--filename='" + suggestedName + "' 2>/dev/null";
    std::string path = zenityRun(cmd);
    // Ensure the path ends with .mp4
    if (!path.empty() && (path.size() < 4 ||
        path.substr(path.size() - 4) != ".mp4"))
        path += ".mp4";
    return path;
}
#endif
