// MilkDropPreset.cpp — load + parse implementation

#include "MilkDropPreset.h"
#include "PresetParser.h"
#include <fstream>
#include <sstream>

bool MilkDropPreset::load() {
    if (!rawData.empty()) return true;
    if (path.empty()) return false;
    std::ifstream f(path);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    rawData = ss.str();
    return !rawData.empty();
}

void MilkDropPreset::parseParameters() {
    if (paramsParsed) return;
    load();
    if (rawData.empty()) return;
    params = PresetParser::parse(rawData);
    paramsParsed = true;
}
