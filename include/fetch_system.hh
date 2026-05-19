#ifndef FETCH_SYSTEM_HH
#define FETCH_SYSTEM_HH

#include "config_parser.hh"
#include "lyra_common.hh"
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

class FetchSystem {
public:
    static std::string fetch_git(const std::string& name, const std::string& url, const std::string& tag) {
        fs::path cache_root = Lyra::get_cache_dir();
        fs::path pkg_dir = cache_root / (name + "-" + (tag.empty() ? "main" : tag));

        if (fs::exists(pkg_dir)) {
            return pkg_dir.string();
        }

        std::cout << "Fetching dependency '" << name << "' from " << url << "...\n";
        fs::create_directories(cache_root);

        // Security: Quote URL and path to prevent shell injection.
        std::string cmd = "git clone --depth 1 ";
        if (!tag.empty()) {
            cmd += "-b \"" + tag + "\" ";
        }
#ifdef _WIN32
        cmd += "\"" + url + "\" \"" + pkg_dir.string() + "\" > nul 2>&1";
#else
        cmd += "\"" + url + "\" \"" + pkg_dir.string() + "\" 2>/dev/null";
#endif

        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "error: failed to clone " << url << "\n";
            return "";
        }

        return pkg_dir.string();
    }
};

#endif
