#ifndef LOCK_SYSTEM_HH
#define LOCK_SYSTEM_HH

#include "resolver.hh"
#include "nol.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>

namespace fs = std::filesystem;

class LockSystem {
public:
    static void generate_lockfile(const std::string& project_root, const std::vector<ResolvedDependency>& deps) {
        NOL::Value root(NOL::Object{});
        NOL::Array packages;

        for (const auto& dep : deps) {
            NOL::Object pkg;
            pkg["name"] = NOL::Value(dep.name);
            pkg["version"] = NOL::Value(dep.version);
            if (!dep.path.empty()) {
                pkg["path"] = NOL::Value(dep.path);
            }
            packages.push_back(NOL::Value(pkg));
        }

        root.asObject()["package"] = NOL::Value(packages);

        std::ofstream out(project_root + "/lymar.lock");
        if (out.is_open()) {
            out << root.dump(2, 0, true);
            out.close();
        } else {
            std::cerr << "error: could not create lymar.lock\n";
        }
    }

    static bool lockfile_exists(const std::string& project_root) {
        return fs::exists(project_root + "/lymar.lock");
    }

    static std::vector<ResolvedDependency> read_lockfile(const std::string& project_root) {
        std::vector<ResolvedDependency> deps;
        std::ifstream file(project_root + "/lymar.lock");
        if (!file.is_open()) return deps;
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        try {
            NOL::Value root = NOL::parse(content);
            if (root.isObject() && root.asObject().count("package")) {
                const auto& pkgs = root.asObject().at("package");
                if (pkgs.isArray()) {
                    for (const auto& pkg_val : pkgs.asArray()) {
                        if (pkg_val.isObject()) {
                            const auto& pkg_obj = pkg_val.asObject();
                            ResolvedDependency dep;
                            if (pkg_obj.count("name")) dep.name = pkg_obj.at("name").asString();
                            if (pkg_obj.count("version")) dep.version = pkg_obj.at("version").asString();
                            if (pkg_obj.count("path")) dep.path = pkg_obj.at("path").asString();
                            deps.push_back(dep);
                        }
                    }
                }
            }
        } catch (...) {
            // Parse failed
        }

        return deps;
    }
};

#endif
