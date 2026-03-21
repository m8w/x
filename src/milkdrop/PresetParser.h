#pragma once
#include "MilkDropPreset.h"
#include <string>

// ---------------------------------------------------------------------------
// PresetParser — parse / serialize .milk preset text
// Ported from MilkDropMac PresetParser (enum in MilkDropPreset.swift)
// ---------------------------------------------------------------------------
namespace PresetParser {

// Parse raw .milk text into PresetParameters.
PresetParameters parse(const std::string& text);

// Serialize PresetParameters back to .milk text.
std::string serialize(const PresetParameters& params, const std::string& name);

} // namespace PresetParser
