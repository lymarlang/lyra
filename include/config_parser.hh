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
    std::map<std::string, Dependency> dependencies;
    std::map<std::string, std::vector<std::string>> features;
    std::vector<std::string> enabled_features;
};

class ConfigParser {
public:
    static std::string trim(const std::string& str, const std::string& chars = " \t\r\n\"") {
        size_t first = str.find_first_not_of(chars);
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(chars);
        return str.substr(first, (last - first + 1));
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
                            if (dep_obj.count("tag") && dep_obj.at("tag").isString()) dep.tag = dep_obj.at("tag").asString();
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

        } catch (const std::exception& e) {
            std::cerr << "error parsing " << filename << ": " << e.what() << std::endl;
        }

        return meta;
    }
};

#endif
