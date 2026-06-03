#include "AppSettings.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static std::string homeDir() {
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    return appdata ? appdata : "C:\\Temp";
#else
    const char* h = getenv("HOME");
    return h ? h : "/tmp";
#endif
}

std::string AppSettings::dataDir() {
#ifdef _WIN32
    return homeDir() + "\\fractal_stream";
#else
    return homeDir() + "/.fractal_stream";
#endif
}

std::string AppSettings::presetsDir() {
#ifdef _WIN32
    return dataDir() + "\\presets";
#else
    return dataDir() + "/presets";
#endif
}

std::string AppSettings::lastPath() {
#ifdef _WIN32
    return dataDir() + "\\last.ini";
#else
    return dataDir() + "/last.ini";
#endif
}

std::string AppSettings::presetPath(const std::string& name) {
#ifdef _WIN32
    return presetsDir() + "\\" + name + ".ini";
#else
    return presetsDir() + "/" + name + ".ini";
#endif
}

void AppSettings::ensureDirs() {
    fs::create_directories(dataDir());
    fs::create_directories(presetsDir());
}

std::vector<std::string> AppSettings::listPresets() {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(presetsDir(), ec)) {
        if (entry.path().extension() == ".ini")
            out.push_back(entry.path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}
