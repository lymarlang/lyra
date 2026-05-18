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

namespace fs = std::filesystem;

int handle_build(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    std::string entry_point = "src/main.lm";
    std::string output_file = meta.name;
    std::vector<std::string> forwarded_args;
    bool forwarding = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            forwarding = true;
            continue;
        }
        if (forwarding) {
            forwarded_args.push_back(arg);
        } else {
             if (arg == "-o" && i + 1 < argc) {
                output_file = argv[++i];
             } else if (arg[0] != '-') {
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

    // Always resolve and update lockfile to ensure it's in sync
    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");
    LockSystem::generate_lockfile(".", deps);

    std::vector<std::string> args;
    // Default build command uses -jit to create an executable
    args.push_back("-jit");
    for (const auto& arg : forwarded_args) {
        args.push_back(arg);
    }

    // Add include paths for dependencies
    for (const auto& dep : deps) {
        args.push_back("-I");
        args.push_back(dep.path + "/src");
    }

    args.push_back(entry_point);

    std::cout << "Building project '" << meta.name << "'...\n";
    std::cout << "(Note: Built-in JIT compilation might be unavailable in this environment)\n";
    
    int result = Lyra::run_command(compiler_path, args);
    if (result == -1) {
        std::cerr << "error: failed to run " << compiler_path << "\n";
    }
    if (result == 0) {
        std::cout << "Build successful.\n";
    }
    
    return result;
}
