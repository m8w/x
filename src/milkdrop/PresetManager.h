#pragma once
#include "MilkDropPreset.h"
#include <string>
#include <vector>
#include <set>
#include <functional>

// ---------------------------------------------------------------------------
// PresetManager — scan directories, navigate, and persist MilkDrop presets
// Ported from MilkDropMac/Presets/PresetManager.swift
//
// Preset directories (mirrors AppSettings layout):
//   ~/.fractal_stream/milkdrop/          — user .milk files
//   <exe_dir>/presets/milkdrop/          — bundled .milk files (read-only)
//
// Favorites are stored in:
//   ~/.fractal_stream/milkdrop_favorites.txt  — one preset path per line
// ---------------------------------------------------------------------------

enum class SortOrder { Name, DateModified, Rating, Random };

class PresetManager {
public:
    PresetManager();

    // ── Loading ────────────────────────────────────────────────────────────

    // Scan preset directories and populate the preset list.
    // Call once at startup; safe to call again to reload.
    void loadAll();

    // Add an extra search directory (call before loadAll or re-call loadAll).
    void addSearchDir(const std::string& dir);

    // ── Navigation ────────────────────────────────────────────────────────

    void nextPreset   (TransitionType t = TransitionType::Smooth);
    void prevPreset   (TransitionType t = TransitionType::Smooth);
    void randomPreset (TransitionType t = TransitionType::Smooth);
    void selectByIndex(int idx, TransitionType t = TransitionType::Smooth);
    void selectByPath (const std::string& path, TransitionType t = TransitionType::Smooth);

    // ── Current state ─────────────────────────────────────────────────────

    // Returns nullptr if no preset is loaded yet.
    MilkDropPreset*       current();
    const MilkDropPreset* current() const;

    // Pending transition type (set by the last navigation call, consumed by renderer).
    TransitionType        pendingTransition() const { return m_pendingTransition; }
    void                  clearPendingTransition()  { m_pendingTransition = TransitionType::Smooth; }

    int   currentIndex() const { return m_currentIdx; }
    int   totalCount()   const { return (int)m_presets.size(); }
    bool  isLoading()    const { return m_loading; }

    // ── List access ───────────────────────────────────────────────────────

    const std::vector<MilkDropPreset>& presets() const { return m_presets; }

    // Apply search/filter/sort and return a filtered view (indices into m_presets).
    std::vector<int> filteredIndices() const;

    // ── Filters ───────────────────────────────────────────────────────────

    std::string searchText;
    bool        filterFavorites = false;
    int         filterMinRating = 0;
    SortOrder   sortOrder       = SortOrder::Name;

    // ── Favorites ─────────────────────────────────────────────────────────

    void toggleFavorite(int presetIndex);
    bool isFavorite(int presetIndex) const;

    // ── Save / import ─────────────────────────────────────────────────────

    // Write a preset's rawData to the user presets directory.
    bool savePreset(MilkDropPreset& preset);

    // Import .milk / .milk2 files or directories into the user presets dir.
    void importFiles(const std::vector<std::string>& paths);

    // ── Callback ──────────────────────────────────────────────────────────
    // Called after currentPreset changes (e.g. to trigger renderer transition).
    std::function<void(MilkDropPreset&, TransitionType)> onPresetChanged;

private:
    void scanDirectory(const std::string& dir);
    void sortPresets();
    void setCurrentIndex(int idx, TransitionType t);
    void loadFavorites();
    void saveFavorites() const;
    std::string userPresetsDir() const;
    std::string favoritesPath()  const;

    std::vector<MilkDropPreset> m_presets;
    std::vector<std::string>    m_searchDirs;
    std::set<std::string>       m_favorites;   // set of preset paths

    int            m_currentIdx       = -1;
    TransitionType m_pendingTransition = TransitionType::Smooth;
    bool           m_loading          = false;

    // History (last 50)
    std::vector<int> m_history;
};
