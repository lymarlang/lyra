#ifndef WORKSPACE_HH
#define WORKSPACE_HH

#include "config_parser.hh"
#include "resolver.hh"
#include "lock_system.hh"
#include "nol.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

struct WorkspaceMember {
    std::string name;
    std::string path;
    PackageMetadata meta;
};

class Workspace {
public:
    static bool is_workspace(const std::string& root_dir = ".") {
        fs::path p = fs::path(root_dir) / "lymar.nol";
        if (!fs::exists(p)) return false;

        std::ifstream file(p);
        if (!file.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        try {
            NOL::Document doc = NOL::parse(content);
            return doc.exists("workspace");
        } catch (...) {
            return false;
        }
    }

    static std::vector<WorkspaceMember> get_members(const std::string& root_dir = ".") {
        std::vector<WorkspaceMember> members;
        fs::path p = fs::path(root_dir) / "lymar.nol";
        if (!fs::exists(p)) return members;

        std::ifstream file(p);
        if (!file.is_open()) return members;
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        try {
            NOL::Document doc = NOL::parse(content);
            const auto* ws = doc.get("workspace");
            if (!ws || !ws->isObject()) return members;

            const auto& ws_obj = ws->asObject();
            if (ws_obj.find("members") != ws_obj.end() && ws_obj.at("members").isArray()) {
                for (const auto& m_val : ws_obj.at("members").asArray()) {
                    if (!m_val.isString()) continue;
                    std::string pattern = m_val.asString();

                    if (pattern.back() == '*') {
                        std::string base = pattern.substr(0, pattern.size() - 1);
                        if (!base.empty() && base.back() == '/') base.pop_back();
                        fs::path base_path = fs::path(root_dir) / base;
                        if (fs::exists(base_path) && fs::is_directory(base_path)) {
                            for (const auto& entry : fs::directory_iterator(base_path)) {
                                if (entry.is_directory() && fs::exists(entry.path() / "lymar.nol")) {
                                    PackageMetadata meta = ConfigParser::parse((entry.path() / "lymar.nol").string());
                                    WorkspaceMember wm;
                                    wm.name = meta.name;
                                    wm.path = fs::relative(entry.path(), root_dir).generic_string();
                                    wm.meta = meta;
                                    members.push_back(wm);
                                }
                            }
                        }
                    } else {
                        fs::path member_path = fs::path(root_dir) / pattern;
                        if (fs::exists(member_path / "lymar.nol")) {
                            PackageMetadata meta = ConfigParser::parse((member_path / "lymar.nol").string());
                            WorkspaceMember wm;
                            wm.name = meta.name;
                            wm.path = pattern;
                            wm.meta = meta;
                            members.push_back(wm);
                        }
                    }
                }
            }
        } catch (...) {}

        std::sort(members.begin(), members.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });

        return members;
    }

    static std::vector<ResolvedDependency> resolve_workspace(const std::string& root_dir = ".") {
        auto members = get_members(root_dir);
        std::map<std::string, ResolvedDependency> all_deps;

        for (const auto& m : members) {
            fs::path member_dir = fs::path(root_dir) / m.path;
            auto deps = DependencyResolver::resolve(member_dir.string());
            for (const auto& d : deps) {
                all_deps[d.name] = d;
            }
        }

        std::vector<ResolvedDependency> combined;
        for (const auto& [_, d] : all_deps) {
            combined.push_back(d);
        }

        LockSystem::generate_lockfile(root_dir, combined);
        return combined;
    }
};

#endif
