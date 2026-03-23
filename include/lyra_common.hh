#ifndef LYRA_COMMON_HH
#define LYRA_COMMON_HH

#include <string>
#include <filesystem>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace Lyra {

inline bool is_safe_string(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_') return false;
    }
    return true;
}

inline fs::path get_cache_dir() {
    char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");
    }
    if (home) {
        return fs::path(home) / ".lyra" / "cache";
    }
    return fs::current_path() / ".lyra_cache";
}

inline unsigned int get_process_id() {
#ifdef _WIN32
    return static_cast<unsigned int>(GetCurrentProcessId());
#else
    return static_cast<unsigned int>(getpid());
#endif
}

} // namespace Lyra

#endif
