#ifndef RESOLVER_HH
#define RESOLVER_HH

#include "config_parser.hh"
#include "semver.hh"
#include "fetch_system.hh"
#include "registry_client.hh"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

struct ResolvedDependency {
    std::string name;
    std::string version;
    std::string path;
    std::string origin_url;
    std::string origin_tag;
};

class DependencyResolver {
public:
    static std::vector<ResolvedDependency> resolve(const std::string& project_root, const std::vector<std::string>& enabled_features = {}) {
        std::vector<ResolvedDependency> resolved;
        std::set<std::string> visiting;
        std::set<std::string> visited;
        
        resolve_recursive(project_root, resolved, visiting, visited, enabled_features);
        
        return resolved;
    }

private:
    static void resolve_recursive(const std::string& current_path, 
                                std::vector<ResolvedDependency>& resolved,
                                std::set<std::string>& visiting,
                                std::set<std::string>& visited,
                                const std::vector<std::string>& enabled_features) {
        
        std::string abs_path = fs::absolute(current_path).string();
        
        if (visiting.count(abs_path)) {
            std::cerr << "error: circular dependency detected involving '" << abs_path << "'\n";
            exit(1);
        }
        
        if (visited.count(abs_path)) return;
        
        visiting.insert(abs_path);
        
        PackageMetadata meta = ConfigParser::parse(current_path + "/lymar.toml");
        if (meta.name.empty()) {
            visiting.erase(abs_path);
            visited.insert(abs_path);
            return;
        }

        for (auto const& [name, dep] : meta.dependencies) {
            // Check if this dependency is feature-gated
            if (!dep.features.empty()) {
                bool feature_satisfied = false;
                for (const auto& f : dep.features) {
                    if (std::find(enabled_features.begin(), enabled_features.end(), f) != enabled_features.end()) {
                        feature_satisfied = true;
                        break;
                    }
                }
                if (!feature_satisfied) continue;
            }

            std::string dep_path;
            if (!dep.path.empty()) {
                dep_path = (fs::path(current_path) / dep.path).string();
            } else if (!dep.git.empty()) {
                dep_path = FetchSystem::fetch_git(name, dep.git, dep.tag);
                if (dep_path.empty()) {
                    std::cerr << "error: failed to fetch git dependency '" << name << "'\n";
                    exit(1);
                }
            } else if (!dep.version.empty()) {
                dep_path = RegistryClient::download(name, dep.version);
            }

            if (!dep_path.empty()) {
                resolve_recursive(dep_path, resolved, visiting, visited, {});
                
                PackageMetadata dep_meta = ConfigParser::parse(dep_path + "/lymar.toml");
                if (!dep_meta.version.empty() && !dep.version.empty() && dep.version != "*") {
                    if (!Semver::satisfies(dep_meta.version, dep.version)) {
                        std::cerr << "error: dependency version mismatch for '" << name << "'. "
                                  << "Required " << dep.version << ", found " << dep_meta.version << "\n";
                        exit(1);
                    }
                }

                ResolvedDependency rd;
                rd.name = name;
                rd.version = dep_meta.version;
                rd.path = fs::absolute(dep_path).string();
                rd.origin_url = dep.git;
                rd.origin_tag = dep.tag;
                
                bool exists = false;
                for (const auto& r : resolved) {
                    if (r.path == rd.path) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    resolved.push_back(rd);
                }
            }
        }
        
        visiting.erase(abs_path);
        visited.insert(abs_path);
    }
};

#endif
