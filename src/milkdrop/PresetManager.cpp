// PresetManager.cpp — ported from MilkDropMac/Presets/PresetManager.swift

#include "PresetManager.h"
#include "PresetParser.h"
#include "../AppSettings.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <ctime>
#include <cstring>
#include <cstdlib>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Directory helpers
// ---------------------------------------------------------------------------

std::string PresetManager::userPresetsDir() const {
    return AppSettings::dataDir() + "/milkdrop";
}

std::string PresetManager::favoritesPath() const {
    return AppSettings::dataDir() + "/milkdrop_favorites.txt";
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PresetManager::PresetManager() {
    // Ensure user presets dir exists
    std::error_code ec;
    fs::create_directories(userPresetsDir(), ec);
    loadFavorites();
}

void PresetManager::addSearchDir(const std::string& dir) {
    m_searchDirs.push_back(dir);
}

// ---------------------------------------------------------------------------
// Load / scan
// ---------------------------------------------------------------------------

void PresetManager::loadAll() {
    m_loading = true;
    m_presets.clear();
    m_currentIdx = -1;

    // 1. User presets dir
    scanDirectory(userPresetsDir());

    // 2. Any extra dirs registered via addSearchDir()
    for (auto& d : m_searchDirs)
        scanDirectory(d);

    sortPresets();

    // Apply favorites
    for (auto& p : m_presets)
        p.isFavorite = m_favorites.count(p.path) > 0;

    m_loading = false;

    // Pick a random starting preset if any are available
    if (!m_presets.empty()) {
        std::mt19937 rng(std::random_device{}());
        m_currentIdx = (int)(rng() % m_presets.size());
    }
}

void PresetManager::scanDirectory(const std::string& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;

    for (auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        // normalise extension to lowercase for comparison
        std::string extLow = ext;
        std::transform(extLow.begin(), extLow.end(), extLow.begin(),
                        [](unsigned char c){ return (char)std::tolower(c); });
        if (extLow != ".milk" && extLow != ".milk2") continue;

        MilkDropPreset p;
        p.path           = entry.path().string();
        p.name           = entry.path().stem().string();
        p.isDoublePreset = (extLow == ".milk2");
        // mtime
        auto wt = entry.last_write_time(ec);
        if (!ec) {
            // Convert file_time_type to seconds-since-epoch via system_clock
            auto sc = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                wt - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            p.mtime = (long long)std::chrono::system_clock::to_time_t(sc);
        }
        m_presets.push_back(std::move(p));
    }
}

// ---------------------------------------------------------------------------
// Sorting
// ---------------------------------------------------------------------------

void PresetManager::sortPresets() {
    switch (sortOrder) {
        case SortOrder::Name:
            std::sort(m_presets.begin(), m_presets.end(),
                [](const MilkDropPreset& a, const MilkDropPreset& b){ return a.name < b.name; });
            break;
        case SortOrder::DateModified:
            std::sort(m_presets.begin(), m_presets.end(),
                [](const MilkDropPreset& a, const MilkDropPreset& b){ return a.mtime > b.mtime; });
            break;
        case SortOrder::Rating:
            std::sort(m_presets.begin(), m_presets.end(),
                [](const MilkDropPreset& a, const MilkDropPreset& b){ return a.rating > b.rating; });
            break;
        case SortOrder::Random: {
            std::mt19937 rng(std::random_device{}());
            std::shuffle(m_presets.begin(), m_presets.end(), rng);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Filtered view
// ---------------------------------------------------------------------------

std::vector<int> PresetManager::filteredIndices() const {
    std::string q = searchText;
    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    std::vector<int> result;
    for (int i = 0; i < (int)m_presets.size(); ++i) {
        const auto& p = m_presets[i];
        if (filterFavorites && !p.isFavorite) continue;
        if (filterMinRating > 0 && p.rating < filterMinRating) continue;
        if (!q.empty()) {
            std::string nameLow = p.name;
            std::transform(nameLow.begin(), nameLow.end(), nameLow.begin(),
                           [](unsigned char c){ return (char)std::tolower(c); });
            if (nameLow.find(q) == std::string::npos) continue;
        }
        result.push_back(i);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void PresetManager::setCurrentIndex(int idx, TransitionType t) {
    if (m_currentIdx >= 0 && m_currentIdx < (int)m_presets.size()) {
        m_history.push_back(m_currentIdx);
        if ((int)m_history.size() > 50) m_history.erase(m_history.begin());
    }
    m_currentIdx = idx;
    m_pendingTransition = t;
    if (idx >= 0 && idx < (int)m_presets.size()) {
        auto& preset = m_presets[idx];
        preset.load();   // lazy-load raw text from disk
        if (onPresetChanged) onPresetChanged(preset, t);
    }
}

void PresetManager::nextPreset(TransitionType t) {
    auto filtered = filteredIndices();
    if (filtered.empty()) return;
    // Find current position in filtered list
    auto it = std::find(filtered.begin(), filtered.end(), m_currentIdx);
    int pos = (it != filtered.end()) ? (int)(it - filtered.begin()) : 0;
    pos = (pos + 1) % (int)filtered.size();
    setCurrentIndex(filtered[pos], t);
}

void PresetManager::prevPreset(TransitionType t) {
    auto filtered = filteredIndices();
    if (filtered.empty()) return;
    auto it = std::find(filtered.begin(), filtered.end(), m_currentIdx);
    int pos = (it != filtered.end()) ? (int)(it - filtered.begin()) : 0;
    pos = (pos - 1 + (int)filtered.size()) % (int)filtered.size();
    setCurrentIndex(filtered[pos], t);
}

void PresetManager::randomPreset(TransitionType t) {
    auto filtered = filteredIndices();
    if (filtered.empty()) return;
    std::mt19937 rng(std::random_device{}());
    int pos = (int)(rng() % filtered.size());
    setCurrentIndex(filtered[pos], t);
}

void PresetManager::selectByIndex(int idx, TransitionType t) {
    if (idx < 0 || idx >= (int)m_presets.size()) return;
    setCurrentIndex(idx, t);
}

void PresetManager::selectByPath(const std::string& path, TransitionType t) {
    for (int i = 0; i < (int)m_presets.size(); ++i) {
        if (m_presets[i].path == path) { setCurrentIndex(i, t); return; }
    }
}

// ---------------------------------------------------------------------------
// Current accessor
// ---------------------------------------------------------------------------

MilkDropPreset* PresetManager::current() {
    if (m_currentIdx < 0 || m_currentIdx >= (int)m_presets.size()) return nullptr;
    return &m_presets[m_currentIdx];
}

const MilkDropPreset* PresetManager::current() const {
    if (m_currentIdx < 0 || m_currentIdx >= (int)m_presets.size()) return nullptr;
    return &m_presets[m_currentIdx];
}

// ---------------------------------------------------------------------------
// Favorites
// ---------------------------------------------------------------------------

void PresetManager::toggleFavorite(int idx) {
    if (idx < 0 || idx >= (int)m_presets.size()) return;
    auto& p = m_presets[idx];
    if (m_favorites.count(p.path)) {
        m_favorites.erase(p.path);
        p.isFavorite = false;
    } else {
        m_favorites.insert(p.path);
        p.isFavorite = true;
    }
    saveFavorites();
}

bool PresetManager::isFavorite(int idx) const {
    if (idx < 0 || idx >= (int)m_presets.size()) return false;
    return m_presets[idx].isFavorite;
}

void PresetManager::loadFavorites() {
    m_favorites.clear();
    std::ifstream f(favoritesPath());
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) m_favorites.insert(line);
    }
}

void PresetManager::saveFavorites() const {
    std::ofstream f(favoritesPath());
    for (const auto& path : m_favorites)
        f << path << "\n";
}

// ---------------------------------------------------------------------------
// Save / import
// ---------------------------------------------------------------------------

bool PresetManager::savePreset(MilkDropPreset& preset) {
    std::string filename = preset.name;
    // Sanitise: replace '/' with '-'
    std::replace(filename.begin(), filename.end(), '/', '-');
    filename += "." + preset.fileExtension();
    std::string dest = userPresetsDir() + "/" + filename;

    std::ofstream f(dest);
    if (!f) return false;
    f << preset.rawData;
    preset.path = dest;

    // Add to list if not already present
    bool found = false;
    for (auto& p : m_presets) {
        if (p.path == dest) { p = preset; found = true; break; }
    }
    if (!found) m_presets.push_back(preset);
    return true;
}

void PresetManager::importFiles(const std::vector<std::string>& paths) {
    std::string destDir = userPresetsDir();
    std::error_code ec;

    for (const auto& src : paths) {
        fs::path sp(src);
        if (fs::is_directory(sp, ec)) {
            for (auto& entry : fs::recursive_directory_iterator(sp, ec)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c){ return (char)std::tolower(c); });
                if (ext != ".milk" && ext != ".milk2") continue;
                auto dst = fs::path(destDir) / entry.path().filename();
                fs::copy_file(entry.path(), dst,
                              fs::copy_options::skip_existing, ec);
            }
        } else {
            auto ext = sp.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c){ return (char)std::tolower(c); });
            if (ext != ".milk" && ext != ".milk2") continue;
            auto dst = fs::path(destDir) / sp.filename();
            fs::copy_file(sp, dst, fs::copy_options::skip_existing, ec);
        }
    }

    // Reload to pick up imported files
    loadAll();
}
