#ifndef BUILD_CACHE_HH
#define BUILD_CACHE_HH

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "crypto_helper.hh"
#include "lyra_common.hh"

namespace fs = std::filesystem;

class BuildCache {
public:
    static fs::path get_build_cache_dir() {
        fs::path p = Lyra::get_cache_dir() / "builds";
        fs::create_directories(p);
        return p;
    }

    // Key calculation: hash(source_code + compiler_version + flags + dependencies)
    static std::string compute_cache_key(const std::string& project_dir,
                                         const std::string& entry_point,
                                         const std::string& compiler_version,
                                         const std::vector<std::string>& flags,
                                         const std::vector<std::string>& dep_hashes) {
        std::stringstream ss;
        // 1. Source code hash (entry point & src directory)
        fs::path proj(project_dir);
        fs::path entry = proj / entry_point;
        if (fs::exists(entry)) {
            ss << "entry:" << CryptoHelper::sha256_file(entry.string()) << "\n";
        }
        fs::path src_dir = proj / "src";
        if (fs::exists(src_dir) && fs::is_directory(src_dir)) {
            ss << "src:" << CryptoHelper::sha256_dir(src_dir.string()) << "\n";
        }

        // 2. Manifest hash
        fs::path manifest = proj / "lymar.nol";
        if (fs::exists(manifest)) {
            ss << "manifest:" << CryptoHelper::sha256_file(manifest.string()) << "\n";
        }

        // 3. Compiler version / path
        ss << "compiler:" << compiler_version << "\n";

        // 4. Flags
        for (const auto& f : flags) {
            ss << "flag:" << f << "\n";
        }

        // 5. Dependencies hashes
        for (const auto& dh : dep_hashes) {
            ss << "dep:" << dh << "\n";
        }

        return CryptoHelper::sha256(ss.str());
    }

    // Try restoring output artifact from cache
    static bool query(const std::string& key, const std::string& output_path) {
        fs::path cached = get_build_cache_dir() / key;
        if (fs::exists(cached) && fs::is_regular_file(cached)) {
            try {
                fs::copy_file(cached, output_path, fs::copy_options::overwrite_existing);
                // Preserve execution permissions on POSIX
#ifndef _WIN32
                fs::permissions(output_path,
                                fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec |
                                fs::perms::group_read | fs::perms::group_exec |
                                fs::perms::others_read | fs::perms::others_exec,
                                fs::perm_options::add);
#endif
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    // Store output artifact to cache
    static bool store(const std::string& key, const std::string& output_path) {
        if (!fs::exists(output_path) || !fs::is_regular_file(output_path)) {
            return false;
        }
        fs::path cached = get_build_cache_dir() / key;
        try {
            fs::copy_file(output_path, cached, fs::copy_options::overwrite_existing);
            return true;
        } catch (...) {
            return false;
        }
    }
};

#endif
