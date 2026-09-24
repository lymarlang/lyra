#ifndef INDEX_CLIENT_HH
#define INDEX_CLIENT_HH

#include "lyra_common.hh"
#include "crypto_helper.hh"
#include "fetch_system.hh"
#include "semver.hh"
#include "nol.hpp"
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

struct IndexVersion {
    std::string version;
    std::string url;
    std::string git;
    std::string tag;
    std::string hash;
    std::map<std::string, std::string> dependencies;
};

struct PackageOwner {
    std::string name;
    std::string email;

    bool matches(const PackageOwner& other) const {
        // Match by email (primary identifier, case-insensitive)
        if (!email.empty() && !other.email.empty()) {
            std::string a = email, b = other.email;
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);
            return a == b;
        }
        return false;
    }
};

struct IndexPackage {
    std::string name;
    std::string description;
    std::string repository;
    std::vector<PackageOwner> owners;
    std::map<std::string, IndexVersion> versions;
};

class IndexClient {
public:
    static bool sync_index(bool force = false) {
        fs::path index_dir = Lyra::get_index_dir();
        std::string index_url = Lyra::get_default_index_url();

        if (!fs::exists(index_dir / ".git")) {
            std::cout << "Cloning package index from " << index_url << "...\n";
            fs::create_directories(index_dir.parent_path());
            std::string cmd = "git clone --depth 1 \"" + index_url + "\" \"" + index_dir.string() + "\"" + LYRA_DEV_NULL;
            int ret = std::system(cmd.c_str());
            if (ret != 0) {
                // If clone fails (e.g. offline or empty remote), check if dir exists anyway
                if (!fs::exists(index_dir)) {
                    fs::create_directories(index_dir / "packages");
                }
                return false;
            }
            return true;
        } else if (force) {
            std::cout << "Updating package index...\n";
            std::string cmd = "git -C \"" + index_dir.string() + "\" pull --ff-only" + LYRA_DEV_NULL;
            std::system(cmd.c_str());
            return true;
        }
        return true;
    }

    static fs::path get_package_file_path(const std::string& name) {
        fs::path index_dir = Lyra::get_index_dir();
        // 1. Direct path: packages/<name>.nol
        fs::path p1 = index_dir / "packages" / (name + ".nol");
        if (fs::exists(p1)) return p1;

        // 2. Sharded path (for large registries): packages/<first_char>/<name>.nol
        if (!name.empty()) {
            std::string shard = name.substr(0, 1);
            fs::path p2 = index_dir / "packages" / shard / (name + ".nol");
            if (fs::exists(p2)) return p2;
        }

        return p1;
    }

    static bool get_package(const std::string& name, IndexPackage& out_pkg) {
        fs::path pkg_file = get_package_file_path(name);
        if (!fs::exists(pkg_file)) {
            // Try updating index once
            sync_index(true);
            pkg_file = get_package_file_path(name);
            if (!fs::exists(pkg_file)) return false;
        }

        std::ifstream file(pkg_file);
        if (!file.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        try {
            NOL::Document doc = NOL::parse(content);
            out_pkg.name = name;

            if (const auto* pkg = doc.get("package")) {
                if (pkg->isObject()) {
                    const auto& obj = pkg->asObject();
                    if (obj.count("name") && obj.at("name").isString()) out_pkg.name = obj.at("name").asString();
                    if (obj.count("description") && obj.at("description").isString()) out_pkg.description = obj.at("description").asString();
                    if (obj.count("repository") && obj.at("repository").isString()) out_pkg.repository = obj.at("repository").asString();
                }
            }

            // Parse owners list
            if (const auto* owners_val = doc.get("owners")) {
                if (owners_val->isArray()) {
                    for (const auto& ov : owners_val->asArray()) {
                        if (ov.isObject()) {
                            PackageOwner po;
                            const auto& oo = ov.asObject();
                            if (oo.count("name") && oo.at("name").isString()) po.name = oo.at("name").asString();
                            if (oo.count("email") && oo.at("email").isString()) po.email = oo.at("email").asString();
                            out_pkg.owners.push_back(po);
                        }
                    }
                }
            }

            if (const auto* vers = doc.get("versions")) {
                if (vers->isObject()) {
                    for (const auto& [ver_str, val] : vers->asObject()) {
                        IndexVersion iv;
                        iv.version = ver_str;
                        if (val.isObject()) {
                            const auto& obj = val.asObject();
                            if (obj.count("url") && obj.at("url").isString()) iv.url = obj.at("url").asString();
                            if (obj.count("git") && obj.at("git").isString()) iv.git = obj.at("git").asString();
                            if (obj.count("tag") && obj.at("tag").isString()) iv.tag = obj.at("tag").asString();
                            if (obj.count("hash") && obj.at("hash").isString()) iv.hash = obj.at("hash").asString();
                            if (obj.count("dependencies") && obj.at("dependencies").isObject()) {
                                for (const auto& [dep_name, dep_val] : obj.at("dependencies").asObject()) {
                                    if (dep_val.isString()) iv.dependencies[dep_name] = dep_val.asString();
                                }
                            }
                        }
                        out_pkg.versions[ver_str] = iv;
                    }
                }
            }

            return true;
        } catch (...) {
            return false;
        }
    }

    static std::string resolve_best_version(const IndexPackage& pkg, const std::string& requirement) {
        std::vector<Version> matches;
        for (const auto& [ver_str, _] : pkg.versions) {
            if (Semver::satisfies(ver_str, requirement)) {
                matches.push_back(Version::parse(ver_str));
            }
        }

        if (matches.empty()) return "";

        std::sort(matches.begin(), matches.end());
        return matches.back().to_string();
    }

    static std::string fetch(const std::string& name, const std::string& version_req) {
        sync_index(false);

        IndexPackage pkg;
        if (!get_package(name, pkg)) {
            std::cerr << "error: package '" << name << "' not found in index (" << Lyra::get_default_index_url() << ")\n";
            return "";
        }

        std::string selected_version = resolve_best_version(pkg, version_req);
        if (selected_version.empty()) {
            std::cerr << "error: no version of '" << name << "' satisfies constraint '" << version_req << "'\n";
            return "";
        }

        const IndexVersion& iv = pkg.versions[selected_version];
        fs::path cache_root = Lyra::get_cache_dir();
        fs::path pkg_dir = cache_root / (name + "-" + selected_version);

        if (fs::exists(pkg_dir) && fs::exists(pkg_dir / "lymar.nol")) {
            return pkg_dir.string();
        }

        // 1. If git URL is specified
        if (!iv.git.empty()) {
            std::string tag = iv.tag.empty() ? ("v" + selected_version) : iv.tag;
            return FetchSystem::fetch_git(name, iv.git, tag);
        }

        // 2. If download URL (e.g. GitHub release / archive tarball) is specified
        if (!iv.url.empty()) {
            fs::create_directories(cache_root);
            fs::path tar_path = cache_root / (name + "-" + selected_version + ".tar.gz");

            std::cout << "Downloading '" << name << "@" << selected_version << "' from " << iv.url << "...\n";
            std::string cmd = "curl -L -f -s -o \"" + tar_path.string() + "\" \"" + iv.url + "\"";
            int ret = std::system(cmd.c_str());
            if (ret != 0 || !fs::exists(tar_path) || fs::file_size(tar_path) == 0) {
                std::cerr << "error: failed to download package from " << iv.url << "\n";
                return "";
            }

            // Verify integrity if hash provided
            if (!iv.hash.empty()) {
                std::string expected_hash = iv.hash;
                if (expected_hash.rfind("sha256:", 0) == 0) {
                    expected_hash = expected_hash.substr(7);
                }
                std::string actual_hash = CryptoHelper::sha256_file(tar_path.string());
                if (actual_hash != expected_hash) {
                    std::cerr << "error: integrity check failed for '" << name << "@" << selected_version << "'!\n";
                    std::cerr << "  Expected: " << expected_hash << "\n";
                    std::cerr << "  Actual:   " << actual_hash << "\n";
                    fs::remove(tar_path);
                    return "";
                }
            }

            fs::create_directories(pkg_dir);
            std::string extract_cmd = "tar -xzf \"" + tar_path.string() + "\" -C \"" + pkg_dir.string() + "\"" + LYRA_DEV_NULL;
            std::system(extract_cmd.c_str());

            // Handle GitHub archives which extract to an inner directory (e.g. repo-1.0.0/)
            if (!fs::exists(pkg_dir / "lymar.nol")) {
                for (const auto& entry : fs::directory_iterator(pkg_dir)) {
                    if (entry.is_directory() && fs::exists(entry.path() / "lymar.nol")) {
                        // Move inner contents up to pkg_dir
                        for (const auto& sub : fs::directory_iterator(entry.path())) {
                            fs::rename(sub.path(), pkg_dir / sub.path().filename());
                        }
                        fs::remove_all(entry.path());
                        break;
                    }
                }
            }

            return pkg_dir.string();
        }

        std::cerr << "error: package '" << name << "@" << selected_version << "' has no git or url defined in index\n";
        return "";
    }

    static std::vector<IndexPackage> search(const std::string& query) {
        sync_index(false);
        std::vector<IndexPackage> results;
        fs::path index_dir = Lyra::get_index_dir() / "packages";
        if (!fs::exists(index_dir)) return results;

        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

        for (const auto& entry : fs::recursive_directory_iterator(index_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".nol") {
                std::string name = entry.path().stem().string();
                IndexPackage pkg;
                if (get_package(name, pkg)) {
                    std::string lower_name = pkg.name;
                    std::string lower_desc = pkg.description;
                    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                    std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);

                    if (lower_name.find(lower_query) != std::string::npos ||
                        lower_desc.find(lower_query) != std::string::npos) {
                        results.push_back(pkg);
                    }
                }
            }
        }
        return results;
    }
};

#endif
