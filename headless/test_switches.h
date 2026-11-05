// EglantinePS5 - Settings > Diagnostics > Testing switches: the emulator's switches (EDEN_NX2_...)
// kept in config/env.txt, one NAME=VALUE per line ('#' starts a comment). main.cpp sets them as the
// app starts, before the core reads them; the app folder's env.txt (development) is read after it
// and wins. A change takes effect from the next start, which is why the menu closes the app.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "storage_paths.h"

namespace Eden::TestSwitches {

inline std::string Path() {
    return ConfigFile("env.txt");
}

// A NAME=VALUE line's name and value; false for a comment, a blank or a malformed line.
inline bool Parse(std::string line, std::string* name, std::string* value) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
    const auto start = line.find_first_not_of(" \t");
    if (start == std::string::npos || line[start] == '#') return false;
    const auto equals = line.find('=', start);
    if (equals == std::string::npos || equals == start) return false;
    *name = line.substr(start, equals - start);
    while (!name->empty() && (name->back() == ' ' || name->back() == '\t')) name->pop_back();
    *value = line.substr(equals + 1);
    return !name->empty();
}

inline std::vector<std::pair<std::string, std::string>> Read(const std::string& path = Path()) {
    std::vector<std::pair<std::string, std::string>> switches;
    std::ifstream file(path);
    std::string name, value;
    for (std::string line; std::getline(file, line);)
        if (Parse(line, &name, &value)) switches.emplace_back(name, value);
    return switches;
}

// Sets the switches `names` to `values` (a name without a value is left out of the file) and keeps
// every other line as it was. Written beside the file and renamed over it: never half written.
inline bool Write(const std::vector<std::string>& names, const std::vector<std::pair<std::string, std::string>>& values,
                  const std::string& path = Path()) {
    std::vector<std::string> kept;
    {
        std::ifstream file(path);
        std::string name, value;
        for (std::string line; std::getline(file, line);)
            if (!Parse(line, &name, &value) || std::find(names.begin(), names.end(), name) == names.end())
                kept.push_back(line);
    }
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
    const std::string temporary = path + ".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        if (!file) return false;
        for (const std::string& line : kept) file << line << '\n';
        for (const auto& [name, value] : values)
            if (!value.empty()) file << name << '=' << value << '\n';
        if (!file.flush()) return false;
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

} // namespace Eden::TestSwitches
