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
