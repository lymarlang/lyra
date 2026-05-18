#ifndef CONFIG_PARSER_HH
#define CONFIG_PARSER_HH

#include "nol.hpp"
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iterator>

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
    static PackageMetadata parse(const std::string& filename) {
        PackageMetadata meta;
        std::ifstream file(filename);
        if (!file.is_open()) return meta;
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        try {
            NOL::Document doc = NOL::parse(content);
            const NOL::Value& root = doc.data();

            if (root.isObject()) {
                const auto& root_obj = root.asObject();
                if (root_obj.count("package")) {
                    const auto& pkg = root_obj.at("package");
                    if (pkg.isObject()) {
                        const auto& pkg_obj = pkg.asObject();
                        if (pkg_obj.count("name")) meta.name = pkg_obj.at("name").asString();
                        if (pkg_obj.count("version")) meta.version = pkg_obj.at("version").asString();
                    }
                }

                if (root_obj.count("dependencies")) {
                    const auto& deps = root_obj.at("dependencies");
                    if (deps.isObject()) {
                        for (auto const& [name, val] : deps.asObject()) {
                            Dependency dep;
                            dep.name = name;
                            if (val.isString()) {
                                dep.version = val.asString();
                            } else if (val.isObject()) {
                                const auto& dep_obj = val.asObject();
                                if (dep_obj.count("version")) dep.version = dep_obj.at("version").asString();
                                if (dep_obj.count("path")) dep.path = dep_obj.at("path").asString();
                                if (dep_obj.count("git")) dep.git = dep_obj.at("git").asString();
                                if (dep_obj.count("tag")) dep.tag = dep_obj.at("tag").asString();

                                if (dep_obj.count("features")) {
                                    const auto& feats = dep_obj.at("features");
                                    if (feats.isArray()) {
                                        for (const auto& f : feats.asArray()) {
                                            if (f.isString()) dep.features.push_back(f.asString());
                                        }
                                    }
                                }
                            }
                            meta.dependencies[name] = dep;
                        }
                    }
                }

                if (root_obj.count("features")) {
                    const auto& features = root_obj.at("features");
                    if (features.isObject()) {
                        for (auto const& [name, val] : features.asObject()) {
                            std::vector<std::string> feat_list;
                            if (val.isArray()) {
                                for (const auto& f : val.asArray()) {
                                    if (f.isString()) feat_list.push_back(f.asString());
                                }
                            }
                            meta.features[name] = feat_list;
                        }
                    }
                }
            }
        } catch (...) {
            // Parse failed
        }

        return meta;
    }
};

#endif
