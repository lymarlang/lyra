#include "handlers.hh"
#include "config_parser.hh"
#include "process_helper.hh"
#include "resolver.hh"
#include "lock_system.hh"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <sstream>

namespace fs = std::filesystem;

int handle_run(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.toml");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.toml or package name\n";
        return 1;
    }

    std::string entry_point = "src/main.lm";
    std::vector<std::string> forwarded_args;
    bool forwarding = false;
    std::vector<std::string> enabled_features;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            forwarding = true;
            continue;
        }
        if (arg == "--features" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string feat;
            while (std::getline(ss, feat, ',')) {
                enabled_features.push_back(feat);
            }
            continue;
        }

        if (forwarding) {
            forwarded_args.push_back(arg);
        } else {
            if (arg[0] != '-') {
                entry_point = arg;
            }
        }
    }

    if (!fs::exists(entry_point)) {
        std::cerr << "error: entry point '" << entry_point << "' not found\n";
        return 1;
    }

    char* root_dir = getenv("REPOROOT");
    std::string compiler_path = root_dir ? std::string(root_dir) + "/bin/limitly" : "../bin/limitly";
    
    if (!fs::exists(compiler_path)) {
        compiler_path = "./bin/limitly";
        if (!fs::exists(compiler_path)) {
            compiler_path = "../bin/limitly";
             if (!fs::exists(compiler_path)) {
                compiler_path = "limitly";
             }
        }
    }

    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".", enabled_features);
    LockSystem::generate_lockfile(".", deps);

    std::vector<std::string> args;
    for (const auto& arg : forwarded_args) {
        args.push_back(arg);
    }
    
    for (const auto& dep : deps) {
        args.push_back("-I");
        args.push_back(dep.path + "/src");
    }

    args.push_back(entry_point);

    int result = Lyra::run_command(compiler_path, args);
    if (result == -1) {
        std::cerr << "error: failed to run " << compiler_path << "\n";
    }
    return result;
}
