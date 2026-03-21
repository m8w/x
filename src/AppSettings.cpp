#include "AppSettings.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

static std::string homeDir() {
    const char* h = getenv("HOME");
    return h ? h : "/tmp";
}

std::string AppSettings::dataDir() {
    return homeDir() + "/.fractal_stream";
}

std::string AppSettings::presetsDir() {
    return dataDir() + "/presets";
}

std::string AppSettings::lastPath() {
    return dataDir() + "/last.ini";
}

std::string AppSettings::presetPath(const std::string& name) {
    return presetsDir() + "/" + name + ".ini";
}

void AppSettings::ensureDirs() {
    mkdir(dataDir().c_str(),    0755);
    mkdir(presetsDir().c_str(), 0755);
}

std::vector<std::string> AppSettings::listPresets() {
    std::vector<std::string> out;
    DIR* d = opendir(presetsDir().c_str());
    if (!d) return out;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n.size() > 4 && n.substr(n.size() - 4) == ".ini")
            out.push_back(n.substr(0, n.size() - 4));
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}
