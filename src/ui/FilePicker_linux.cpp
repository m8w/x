#ifndef __APPLE__
#include "FilePicker.h"
#include <cstdio>
#include <array>
#include <string>

// On Linux use zenity (GTK file chooser) if available, else return "".
std::string pickVideoFile() {
    std::array<char, 1024> buf{};
    FILE* f = popen(
        "zenity --file-selection --title='Choose a video file' "
        "--file-filter='Video files (mp4 mkv mov avi) | *.mp4 *.mkv *.mov *.avi' 2>/dev/null",
        "r");
    if (!f) return "";
    std::string path;
    while (fgets(buf.data(), buf.size(), f))
        path += buf.data();
    pclose(f);
    if (!path.empty() && path.back() == '\n') path.pop_back();
    return path;
}
#endif
