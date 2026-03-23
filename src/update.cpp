#include "handlers.hh"
#include "resolver.hh"
#include "lock_system.hh"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

int handle_update(int argc, char** argv) {
    std::cout << "Updating dependencies...\n";

    std::vector<std::string> enabled_features;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--features" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string feat;
            while (std::getline(ss, feat, ',')) {
                enabled_features.push_back(feat);
            }
        }
    }

    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".", enabled_features);
    LockSystem::generate_lockfile(".", deps);
    std::cout << "Lockfile updated successfully.\n";
    return 0;
}

int handle_fetch(int argc, char** argv) {
    if (!LockSystem::lockfile_exists(".")) {
        std::cerr << "error: lymar.lock not found. Run 'lyra update' first.\n";
        return 1;
    }

    std::cout << "Fetching dependencies from lymar.lock...\n";
    std::vector<ResolvedDependency> deps = LockSystem::read_lockfile(".");

    for (const auto& dep : deps) {
        if (dep.path.empty()) continue; // Should not happen with valid lockfile

        // If it's a path dependency, we don't need to "fetch" it, just verify it exists
        if (dep.path.find(".lyra/cache") == std::string::npos && dep.path.find(".lyra_cache") == std::string::npos) {
             if (!fs::exists(dep.path)) {
                std::cerr << "error: path dependency '" << dep.name << "' not found at " << dep.path << "\n";
                return 1;
             }
             continue;
        }

        // It's a cached dependency. Check if it's actually there.
        if (!fs::exists(dep.path)) {
            std::cout << "Restoring " << dep.name << " (" << dep.version << ")..." << std::endl;
            // We need to know if it's a git or registry dependency to restore it properly.
            // For now, let's assume if it's in cache, we can try to re-download it.
            // However, our LockSystem doesn't currently store the source URL.
            // This is a limitation of the current LockSystem.
            // For now, let's just use RegistryClient::download as a fallback.
            RegistryClient::download(dep.name, dep.version);
        } else {
            std::cout << "Dependency " << dep.name << " (" << dep.version << ") is already in cache.\n";
        }
    }

    std::cout << "All dependencies fetched.\n";
    return 0;
}

int handle_lock(int argc, char** argv) {
    std::cout << "Updating lockfile...\n";
    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");
    LockSystem::generate_lockfile(".", deps);
    std::cout << "Lockfile updated successfully.\n";
    return 0;
}

int handle_verify(int argc, char** argv) {
    if (!LockSystem::lockfile_exists(".")) {
        std::cerr << "error: lymar.lock not found.\n";
        return 1;
    }

    std::cout << "Verifying lockfile integrity...\n";

    // Simple verification: re-resolve and compare names and versions
    std::vector<ResolvedDependency> current_deps = DependencyResolver::resolve(".");
    std::vector<ResolvedDependency> locked_deps = LockSystem::read_lockfile(".");

    if (current_deps.size() != locked_deps.size()) {
        std::cerr << "error: lockfile out of sync (dependency count mismatch)\n";
        return 1;
    }

    for (const auto& cd : current_deps) {
        bool found = false;
        for (const auto& ld : locked_deps) {
            if (cd.name == ld.name) {
                if (cd.version != ld.version) {
                    std::cerr << "error: lockfile out of sync for '" << cd.name << "': "
                              << "found " << ld.version << ", expected " << cd.version << "\n";
                    return 1;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "error: lockfile out of sync (dependency '" << cd.name << "' missing from lockfile)\n";
            return 1;
        }
    }

    std::cout << "Lockfile is up-to-date.\n";
    return 0;
}
