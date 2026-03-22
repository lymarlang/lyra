#ifndef FETCH_SYSTEM_HH
#define FETCH_SYSTEM_HH

#include "config_parser.hh"
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

class FetchSystem {
public:
    static std::string fetch_git(const std::string& name, const std::string& url, const std::string& tag) {
        fs::path cache_root = get_cache_dir();
        fs::path pkg_dir = cache_root / (name + "-" + (tag.empty() ? "main" : tag));

        if (fs::exists(pkg_dir)) {
            // std::cout << "Using cached dependency '" << name << "'\n";
            return pkg_dir.string();
        }

        std::cout << "Fetching dependency '" << name << "' from " << url << "...\n";
        fs::create_directories(cache_root);

        std::string cmd = "git clone --depth 1 ";
        if (!tag.empty()) {
            cmd += "-b " + tag + " ";
        }
        cmd += url + " " + pkg_dir.string() + " 2>/dev/null";

        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "error: failed to clone " << url << "\n";
            return "";
        }

        return pkg_dir.string();
    }

private:
    static fs::path get_cache_dir() {
        char* home = std::getenv("HOME");
        if (home) {
            return fs::path(home) / ".lyra" / "cache";
        }
        return fs::current_path() / ".lyra_cache";
    }
};

#endif
