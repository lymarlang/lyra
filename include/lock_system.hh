#ifndef LOCK_SYSTEM_HH
#define LOCK_SYSTEM_HH

#include "resolver.hh"
#include "nole.hh"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

class LockSystem {
public:
    static void generate_lockfile(const std::string& project_root, const std::vector<ResolvedDependency>& deps) {
        NoleDocument doc;
        doc.set_dependencies(deps);
        doc.save(project_root + "/lymar.lock");
    }

    static bool lockfile_exists(const std::string& project_root) {
        return fs::exists(project_root + "/lymar.lock");
    }

    static std::vector<ResolvedDependency> read_lockfile(const std::string& project_root) {
        std::vector<ResolvedDependency> deps;
        std::ifstream file(project_root + "/lymar.lock");
        if (!file.is_open()) return deps;

        std::string line;
        ResolvedDependency current_dep;
        bool in_package = false;

        while (std::getline(file, line)) {
            if (line == "[[package]]") {
                if (in_package) deps.push_back(current_dep);
                current_dep = ResolvedDependency();
                in_package = true;
                continue;
            }

            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);

                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t\""));
                value.erase(value.find_last_not_of(" \t\"") + 1);

                if (key == "name") current_dep.name = value;
                else if (key == "version") current_dep.version = value;
                else if (key == "path") current_dep.path = value;
            }
        }
        if (in_package) deps.push_back(current_dep);

        return deps;
    }
};

#endif
