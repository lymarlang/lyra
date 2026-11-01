#ifndef CONFIG_PARSER_HH
#define CONFIG_PARSER_HH

#include "nol.hpp"
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>

struct Dependency {
    std::string name;
    std::string version;
    std::string path;
    std::string git;
    std::string tag;
    std::vector<std::string> features;
};

struct PackageMetadata {
    std::string name;
    std::string version;
    std::string description;
    std::string repository;
    std::string license;
    std::string author;
    std::map<std::string, Dependency> dependencies;
    std::map<std::string, std::vector<std::string>> features;
    std::vector<std::string> enabled_features;
    // [native] - foreign .c/.cpp sources and link options compiled by the
    // native toolchain and linked into built artifacts (roadmap item 2a).
    // Values accept either arrays or single-line strings.
    std::vector<std::string> native_sources;
    std::vector<std::string> native_include_dirs;
    std::vector<std::string> native_cflags;
    std::vector<std::string> native_link_libs;
    // [native] header binding generation (roadmap item 2b).
    std::vector<std::string> bind_headers;
    std::vector<std::string> bind_lib_paths;
    // [patch] / [replace] dependency overrides
    std::map<std::string, Dependency> patches;
};

class ConfigParser {
public:
    static std::string trim(const std::string& str, const std::string& chars = " \t\r\n\"") {
        size_t first = str.find_first_not_of(chars);
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(chars);
        return str.substr(first, (last - first + 1));
    }

    // Accept either an array of strings or a whitespace-separated string for
    // list-valued manifest keys.
    static std::vector<std::string> string_list(const NOL::Value& val) {
        std::vector<std::string> out;
        if (val.isArray()) {
            for (const auto& item : val.asArray()) {
                if (item.isString() && !item.asString().empty()) out.push_back(item.asString());
            }
        } else if (val.isString()) {
            std::stringstream stream(val.asString());
            std::string token;
            while (stream >> token) out.push_back(token);
        }
        return out;
    }

    static PackageMetadata parse(const std::string& filename) {
        PackageMetadata meta;
        std::ifstream file(filename);
        if (!file.is_open()) return meta;

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        try {
            NOL::Document doc = NOL::parse(content);

            // Parse [package]
            if (const auto* pkg = doc.get("package")) {
                if (pkg->isObject()) {
                    const auto& obj = pkg->asObject();
                    if (obj.count("name") && obj.at("name").isString()) meta.name = obj.at("name").asString();
                    if (obj.count("version") && obj.at("version").isString()) meta.version = obj.at("version").asString();
                    if (obj.count("description") && obj.at("description").isString()) meta.description = obj.at("description").asString();
                    if (obj.count("repository") && obj.at("repository").isString()) meta.repository = obj.at("repository").asString();
                    if (obj.count("license") && obj.at("license").isString()) meta.license = obj.at("license").asString();
                    if (obj.count("author") && obj.at("author").isString()) meta.author = obj.at("author").asString();
                }
            }

            // Parse [dependencies]
            if (const auto* deps = doc.get("dependencies")) {
                if (deps->isObject()) {
                    for (const auto& [name, val] : deps->asObject()) {
                        Dependency dep;
                        dep.name = name;
                        if (val.isString()) {
                            dep.version = val.asString();
                        } else if (val.isObject()) {
                            const auto& dep_obj = val.asObject();
                            if (dep_obj.count("version") && dep_obj.at("version").isString()) dep.version = dep_obj.at("version").asString();
                            if (dep_obj.count("path") && dep_obj.at("path").isString()) dep.path = dep_obj.at("path").asString();
                            if (dep_obj.count("git") && dep_obj.at("git").isString()) dep.git = dep_obj.at("git").asString();
                            if (dep_obj.count("github") && dep_obj.at("github").isString()) dep.git = "https://github.com/" + dep_obj.at("github").asString() + ".git";
                            if (dep_obj.count("tag") && dep_obj.at("tag").isString()) dep.tag = dep_obj.at("tag").asString();
                            if (dep_obj.count("branch") && dep_obj.at("branch").isString()) dep.tag = dep_obj.at("branch").asString();
                            if (dep_obj.count("rev") && dep_obj.at("rev").isString()) dep.tag = dep_obj.at("rev").asString();
                            if (dep_obj.count("features") && dep_obj.at("features").isArray()) {
                                for (const auto& f : dep_obj.at("features").asArray()) {
                                    if (f.isString()) dep.features.push_back(f.asString());
                                }
                            }
                        }
                        meta.dependencies[name] = dep;
                    }
                }
            }

            // Parse [patch] / [replace]
            auto parse_patch_map = [&](const NOL::Value* patch_val) {
                if (patch_val && patch_val->isObject()) {
                    for (const auto& [name, val] : patch_val->asObject()) {
                        Dependency dep;
                        dep.name = name;
                        if (val.isString()) {
                            dep.version = val.asString();
                        } else if (val.isObject()) {
                            const auto& dep_obj = val.asObject();
                            if (dep_obj.count("version") && dep_obj.at("version").isString()) dep.version = dep_obj.at("version").asString();
                            if (dep_obj.count("path") && dep_obj.at("path").isString()) dep.path = dep_obj.at("path").asString();
                            if (dep_obj.count("git") && dep_obj.at("git").isString()) dep.git = dep_obj.at("git").asString();
                            if (dep_obj.count("github") && dep_obj.at("github").isString()) dep.git = "https://github.com/" + dep_obj.at("github").asString() + ".git";
                            if (dep_obj.count("tag") && dep_obj.at("tag").isString()) dep.tag = dep_obj.at("tag").asString();
                            if (dep_obj.count("branch") && dep_obj.at("branch").isString()) dep.tag = dep_obj.at("branch").asString();
                            if (dep_obj.count("rev") && dep_obj.at("rev").isString()) dep.tag = dep_obj.at("rev").asString();
                        }
                        meta.patches[name] = dep;
                    }
                }
            };
            parse_patch_map(doc.get("patch"));
            parse_patch_map(doc.get("replace"));

            // Parse [features]
            if (const auto* feats = doc.get("features")) {
                if (feats->isObject()) {
                    for (const auto& [name, val] : feats->asObject()) {
                        if (val.isArray()) {
                            std::vector<std::string> feature_deps;
                            for (const auto& f : val.asArray()) {
                                if (f.isString()) feature_deps.push_back(f.asString());
                            }
                            meta.features[name] = feature_deps;
                        }
                    }
                }
            }

            // Parse [native]
            if (const auto* native = doc.get("native")) {
                if (native->isObject()) {
                    const auto& obj = native->asObject();
                    if (obj.count("sources")) meta.native_sources = string_list(obj.at("sources"));
                    if (obj.count("include_dirs")) meta.native_include_dirs = string_list(obj.at("include_dirs"));
                    if (obj.count("cflags")) meta.native_cflags = string_list(obj.at("cflags"));
                    if (obj.count("link_libs")) meta.native_link_libs = string_list(obj.at("link_libs"));
                    if (obj.count("headers")) meta.bind_headers = string_list(obj.at("headers"));
                    if (obj.count("bind_libs")) meta.bind_lib_paths = string_list(obj.at("bind_libs"));
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "error parsing " << filename << ": " << e.what() << std::endl;
        }

        return meta;
    }
};

#endif
