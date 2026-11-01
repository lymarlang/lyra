#ifndef RESOLVER_HH
#define RESOLVER_HH

#include "config_parser.hh"
#include "semver.hh"
#include "fetch_system.hh"
#include "registry_client.hh"
#include "index_client.hh"
#include "crypto_helper.hh"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <optional>

namespace fs = std::filesystem;

struct ResolvedDependency {
    std::string name;
    std::string version;
    std::string path;
    std::string hash;
};

struct PackageCandidate {
    std::string name;
    std::string version;
    std::string path;
    std::map<std::string, std::string> dependencies;
    std::string hash;
};

class DependencyResolver {
public:
    static std::vector<ResolvedDependency> resolve(const std::string& project_root,
                                                   const std::vector<std::string>& enabled_features = {},
                                                  bool offline = false,
                                                  bool mvs = false) {
        std::string abs_root = fs::weakly_canonical(project_root).string();
        if (!fs::is_regular_file(fs::path(abs_root) / "lymar.nol")) {
            throw std::runtime_error("Missing dependency manifest: " + abs_root);
        }

        PackageMetadata root_meta = ConfigParser::parse(abs_root + "/lymar.nol");
        if (root_meta.name.empty()) {
            throw std::runtime_error("Invalid package manifest: " + abs_root);
        }

        std::map<std::string, std::string> root_constraints;
        std::map<std::string, Dependency> root_dep_specs;

        // Apply patches/overrides to root dependencies
        auto effective_deps = root_meta.dependencies;
        for (const auto& [name, patch_dep] : root_meta.patches) {
            effective_deps[name] = patch_dep;
        }

        for (const auto& [name, dep] : effective_deps) {
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

            std::string req = dep.version.empty() ? "*" : dep.version;
            root_constraints[name] = req;
            root_dep_specs[name] = dep;
        }

        std::map<std::string, PackageCandidate> selected;
        std::map<std::string, std::vector<std::pair<std::string, std::string>>> constraints;
        for (const auto& [name, req] : root_constraints) {
            constraints[name].push_back({root_meta.name, req});
        }

        if (!backtrack_solve(selected, constraints, root_dep_specs, root_meta.patches, abs_root, offline, mvs)) {
            std::cerr << "error: unable to find a compatible dependency version resolution (PubGrub conflict detected)\n";
            for (const auto& [pkg, req_list] : constraints) {
                std::cerr << "  Package '" << pkg << "' required with constraints:\n";
                for (const auto& [req_by, c] : req_list) {
                    std::cerr << "    - " << c << " (required by " << req_by << ")\n";
                }
            }
            throw std::runtime_error("Dependency resolution failed");
        }

        std::vector<ResolvedDependency> resolved;
        for (const auto& [name, cand] : selected) {
            ResolvedDependency rd;
            rd.name = name;
            rd.version = cand.version;
            rd.path = cand.path;
            rd.hash = cand.hash;
            if (rd.hash.empty() && !rd.path.empty() && fs::exists(rd.path)) {
                rd.hash = "sha256:" + CryptoHelper::calculate_package_hash(rd.path);
            }
            resolved.push_back(rd);
        }

        std::sort(resolved.begin(), resolved.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });

        return resolved;
    }

private:
    static bool backtrack_solve(std::map<std::string, PackageCandidate>& selected,
                                std::map<std::string, std::vector<std::pair<std::string, std::string>>>& constraints,
                                const std::map<std::string, Dependency>& root_dep_specs,
                                const std::map<std::string, Dependency>& patches,
                                const std::string& project_root,
                                bool offline,
                                bool mvs) {
        std::string next_pkg;
        for (const auto& [pkg, reqs] : constraints) {
            if (selected.find(pkg) == selected.end() && !reqs.empty()) {
                next_pkg = pkg;
                break;
            }
        }

        if (next_pkg.empty()) {
            return true;
        }

        // Check if package is overridden by [patch] / [replace]
        std::map<std::string, Dependency> effective_specs = root_dep_specs;
        if (patches.find(next_pkg) != patches.end()) {
            effective_specs[next_pkg] = patches.at(next_pkg);
        }

        std::vector<PackageCandidate> candidates = get_candidates(next_pkg, effective_specs, project_root, offline);

        std::sort(candidates.begin(), candidates.end(), [mvs](const PackageCandidate& a, const PackageCandidate& b) {
            Version va = Version::parse(a.version);
            Version vb = Version::parse(b.version);
            if (mvs) {
                // Minimal Version Selection: resolve oldest compatible version
                return va < vb;
            }
            return vb < va;
        });

        const auto& active_constraints = constraints[next_pkg];

        for (const auto& cand : candidates) {
            bool all_satisfied = true;
            for (const auto& [req_by, req] : active_constraints) {
                if (!Semver::satisfies(cand.version, req)) {
                    all_satisfied = false;
                    break;
                }
            }
            if (!all_satisfied) continue;

            selected[next_pkg] = cand;

            std::map<std::string, std::vector<std::pair<std::string, std::string>>> new_constraints = constraints;
            bool conflict = false;
            for (const auto& [dep_name, dep_req] : cand.dependencies) {
                if (selected.find(dep_name) != selected.end()) {
                    if (!Semver::satisfies(selected[dep_name].version, dep_req)) {
                        conflict = true;
                        break;
                    }
                }
                new_constraints[dep_name].push_back({cand.name, dep_req});
            }

            if (!conflict) {
                if (backtrack_solve(selected, new_constraints, root_dep_specs, patches, project_root, offline, mvs)) {
                    return true;
                }
            }

            selected.erase(next_pkg);
        }

        return false;
    }

    static std::vector<PackageCandidate> get_candidates(const std::string& name,
                                                       const std::map<std::string, Dependency>& root_dep_specs,
                                                       const std::string& project_root,
                                                       bool offline) {
        std::vector<PackageCandidate> candidates;

        if (root_dep_specs.find(name) != root_dep_specs.end()) {
            const auto& spec = root_dep_specs.at(name);
            if (!spec.path.empty()) {
                fs::path p = fs::path(project_root) / spec.path;
                if (fs::exists(p / "lymar.nol")) {
                    PackageMetadata meta = ConfigParser::parse((p / "lymar.nol").string());
                    PackageCandidate c;
                    c.name = name;
                    c.version = meta.version.empty() ? "0.0.0" : meta.version;
                    c.path = fs::weakly_canonical(p).string();
                    for (const auto& [dname, d] : meta.dependencies) {
                        c.dependencies[dname] = d.version.empty() ? "*" : d.version;
                    }
                    candidates.push_back(c);
                    return candidates;
                }
            } else if (!spec.git.empty() && !offline) {
                std::string dep_path = FetchSystem::fetch_git(name, spec.git, spec.tag);
                if (!dep_path.empty() && fs::exists(dep_path + "/lymar.nol")) {
                    PackageMetadata meta = ConfigParser::parse(dep_path + "/lymar.nol");
                    PackageCandidate c;
                    c.name = name;
                    c.version = meta.version.empty() ? "0.0.0" : meta.version;
                    c.path = fs::weakly_canonical(dep_path).string();
                    for (const auto& [dname, d] : meta.dependencies) {
                        c.dependencies[dname] = d.version.empty() ? "*" : d.version;
                    }
                    candidates.push_back(c);
                    return candidates;
                }
            }
        }

        fs::path vendor_path = fs::path(project_root) / "vendor" / name;
        if (fs::exists(vendor_path / "lymar.nol")) {
            PackageMetadata meta = ConfigParser::parse((vendor_path / "lymar.nol").string());
            PackageCandidate c;
            c.name = name;
            c.version = meta.version.empty() ? "0.0.0" : meta.version;
            c.path = fs::weakly_canonical(vendor_path).string();
            for (const auto& [dname, d] : meta.dependencies) {
                c.dependencies[dname] = d.version.empty() ? "*" : d.version;
            }
            candidates.push_back(c);
            return candidates;
        }

        IndexPackage ipkg;
        if (IndexClient::get_package(name, ipkg)) {
            for (const auto& [v_str, v_info] : ipkg.versions) {
                PackageCandidate c;
                c.name = name;
                c.version = v_str;
                c.dependencies = v_info.dependencies;
                c.hash = v_info.hash;

                fs::path cached = Lyra::get_cache_dir() / (name + "-" + v_str);
                if (fs::exists(cached / "lymar.nol")) {
                    c.path = fs::weakly_canonical(cached).string();
                } else if (!offline) {
                    std::string fetched = IndexClient::fetch(name, v_str);
                    if (!fetched.empty()) {
                        c.path = fetched;
                    }
                }
                candidates.push_back(c);
            }
        }

        fs::path cache_root = Lyra::get_cache_dir();
        if (fs::exists(cache_root)) {
            for (const auto& entry : fs::directory_iterator(cache_root)) {
                if (entry.is_directory()) {
                    std::string dname = entry.path().filename().string();
                    if (dname.rfind(name + "-", 0) == 0) {
                        std::string v = dname.substr(name.size() + 1);
                        if (fs::exists(entry.path() / "lymar.nol")) {
                            PackageMetadata m = ConfigParser::parse((entry.path() / "lymar.nol").string());
                            PackageCandidate c;
                            c.name = name;
                            c.version = m.version.empty() ? v : m.version;
                            c.path = fs::weakly_canonical(entry.path()).string();
                            for (const auto& [dname, d] : m.dependencies) {
                                c.dependencies[dname] = d.version.empty() ? "*" : d.version;
                            }
                            bool dup = false;
                            for (const auto& ex : candidates) {
                                if (ex.version == c.version) { dup = true; break; }
                            }
                            if (!dup) candidates.push_back(c);
                        }
                    }
                }
            }
        }

        return candidates;
    }
};

#endif
