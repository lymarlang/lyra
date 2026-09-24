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
    static std::string normalize_git_url(const std::string& url) {
        if (url.find("://") == std::string::npos && url.find('@') == std::string::npos) {
            // Assume GitHub short form: "owner/repo"
            return "https://github.com/" + url + ".git";
        }
        return url;
    }

    static std::string fetch_git(const std::string& name, const std::string& raw_url, const std::string& tag) {
        std::string url = normalize_git_url(raw_url);
        fs::path cache_root = Lyra::get_cache_dir();
        std::string ref_name = tag.empty() ? "main" : tag;
        // Clean ref_name for directory name
        std::string safe_ref = ref_name;
        for (char& c : safe_ref) {
            if (c == '/' || c == '\\' || c == ':') c = '-';
        }
        fs::path pkg_dir = cache_root / (name + "-" + safe_ref);

        if (fs::exists(pkg_dir) && fs::exists(pkg_dir / "lymar.nol")) {
            return pkg_dir.string();
        }

        std::cout << "Fetching dependency '" << name << "' from " << url << " (" << ref_name << ")...\n";
        fs::create_directories(cache_root);

        // Try fast clone with --depth 1 -b <tag>
        std::string cmd = "git clone --depth 1 ";
        if (!tag.empty()) {
            cmd += "-b \"" + tag + "\" ";
        }
        cmd += "\"" + url + "\" \"" + pkg_dir.string() + "\"" + LYRA_DEV_NULL;

        int result = std::system(cmd.c_str());
        if (result != 0) {
            // If -b failed (e.g. tag is a commit SHA), try fetching commit directly
            if (fs::exists(pkg_dir)) fs::remove_all(pkg_dir);
            fs::create_directories(pkg_dir);

            std::string init_cmd = "git init \"" + pkg_dir.string() + "\"" + LYRA_DEV_NULL;
            std::string remote_cmd = "git -C \"" + pkg_dir.string() + "\" remote add origin \"" + url + "\"" + LYRA_DEV_NULL;
            std::string fetch_cmd = "git -C \"" + pkg_dir.string() + "\" fetch --depth 1 origin \"" + (tag.empty() ? "HEAD" : tag) + "\"" + LYRA_DEV_NULL;
            std::string checkout_cmd = "git -C \"" + pkg_dir.string() + "\" checkout -q FETCH_HEAD" + LYRA_DEV_NULL;

            if (std::system(init_cmd.c_str()) != 0 ||
                std::system(remote_cmd.c_str()) != 0 ||
                std::system(fetch_cmd.c_str()) != 0 ||
                std::system(checkout_cmd.c_str()) != 0) {
                std::cerr << "error: failed to clone " << url << " at ref '" << ref_name << "'\n";
                fs::remove_all(pkg_dir);
                return "";
            }
        }

        return pkg_dir.string();
    }
};

#endif
