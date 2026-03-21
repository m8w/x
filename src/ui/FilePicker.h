#pragma once
#include <string>

// Opens a native OS file-picker dialog filtered to common video formats.
// Returns the chosen file path, or "" if the user cancelled.
std::string pickVideoFile();
