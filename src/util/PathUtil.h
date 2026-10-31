#pragma once

#include <filesystem>
#include <string>

// std::filesystem::path::string() uses the Windows ANSI code page. miniaudio
// and FTXUI both expect UTF-8, so always convert paths through u8string().
inline std::string pathToUtf8(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

inline std::string toLowerAscii(std::string value) {
    for (char& c : value) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return value;
}
