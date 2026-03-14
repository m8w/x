#pragma once
#include <string>
#include <vector>

// Manages the on-disk storage layout for settings and presets.
//
//   ~/.fractal_stream/last.ini          — auto-saved on exit, loaded on start
//   ~/.fractal_stream/presets/<name>.ini — named user presets
//
class AppSettings {
public:
    static std::string dataDir();      // ~/.fractal_stream
    static std::string presetsDir();   // ~/.fractal_stream/presets
    static std::string lastPath();     // ~/.fractal_stream/last.ini
    static std::string presetPath(const std::string& name); // presetsDir/<name>.ini

    // Scan presetsDir and return sorted list of preset names (without .ini)
    static std::vector<std::string> listPresets();

    // Create dataDir and presetsDir if they don't exist yet
    static void ensureDirs();
};
