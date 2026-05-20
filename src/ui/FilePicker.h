#pragma once
#include <string>

// Opens a native OS file-picker dialog filtered to common video formats.
// Returns the chosen file path, or "" if the user cancelled.
std::string pickVideoFile();

// Opens a native OS save-file dialog for choosing a recording destination.
// suggestedName: pre-filled filename (e.g. "fractal_20250520_120000.mp4")
// Returns the chosen path, or "" if the user cancelled.
std::string pickSaveFile(const std::string& suggestedName);
