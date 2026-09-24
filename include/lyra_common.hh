#ifndef LYRA_COMMON_HH
#define LYRA_COMMON_HH

#include <string>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace Lyra {

#ifdef _WIN32
#define LYRA_DEV_NULL " > nul 2>&1"
#define LYRA_EXE_SUFFIX ".exe"
#else
#define LYRA_DEV_NULL " > /dev/null 2>&1"
#define LYRA_EXE_SUFFIX ""
#endif

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

inline fs::path get_index_dir() {
    char* env = std::getenv("LYRA_INDEX_DIR");
    if (env && *env) return fs::path(env);
    char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    if (home) {
        return fs::path(home) / ".lyra" / "index";
    }
    return fs::current_path() / ".lyra_index";
}

inline std::string get_default_index_url() {
    char* url = std::getenv("LYRA_INDEX_URL");
    return (url && *url) ? std::string(url) : "https://github.com/lymarlang/index.git";
}

inline std::string find_compiler_path() {
    // 1. Check directory where lyra binary itself is located
#ifdef _WIN32
    char exe_buf[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_buf, MAX_PATH)) {
        fs::path self_dir = fs::path(exe_buf).parent_path();
        if (fs::exists(self_dir / "limitly.exe")) {
            return (self_dir / "limitly.exe").string();
        }
    }
#endif

    // 2. Check REPOROOT environment variable
    char* root_dir = getenv("REPOROOT");
    if (root_dir) {
        fs::path p = fs::path(root_dir) / "bin" / ("limitly" LYRA_EXE_SUFFIX);
        if (fs::exists(p)) {
            return p.string();
        }
    }

    // 3. Search upwards from current directory for bin/limitly
    try {
        fs::path cur = fs::current_path();
        while (true) {
            fs::path candidate = cur / "bin" / ("limitly" LYRA_EXE_SUFFIX);
            if (fs::exists(candidate)) {
                return candidate.string();
            }
            if (!cur.has_parent_path() || cur == cur.parent_path()) break;
            cur = cur.parent_path();
        }
    } catch (...) {}

#ifdef _WIN32
    return "limitly.exe";
#else
    return "limitly";
#endif
}

inline unsigned int get_process_id() {
#ifdef _WIN32
    return static_cast<unsigned int>(GetCurrentProcessId());
#else
    return static_cast<unsigned int>(getpid());
#endif
}

inline std::pair<std::string, std::string> get_git_identity() {
    std::string name, email;
    auto read_git_config = [](const std::string& key) -> std::string {
        std::string tmp = ".lyra_git_id.tmp";
        std::string cmd = "git config --get " + key + " > \"" + tmp + "\" 2>nul";
#ifndef _WIN32
        cmd = "git config --get " + key + " > \"" + tmp + "\" 2>/dev/null";
#endif
        std::system(cmd.c_str());
        std::ifstream f(tmp);
        std::string val;
        if (f.is_open()) {
            std::getline(f, val);
            f.close();
        }
        fs::remove(tmp);
        while (!val.empty() && (val.back() == '\n' || val.back() == '\r' || val.back() == ' ')) {
            val.pop_back();
        }
        return val;
    };
    name = read_git_config("user.name");
    email = read_git_config("user.email");
    return {name, email};
}

} // namespace Lyra

#endif
