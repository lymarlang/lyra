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

#ifdef _WIN32
    if (output_file.find(".exe") == std::string::npos) {
        output_file += ".exe";
    }
#endif

    if (!fs::exists(entry_point)) {
        std::cerr << "error: entry point '" << entry_point << "' not found\n";
        return 1;
    }

    std::string compiler_path = Lyra::find_compiler_path();

    // Always resolve and update lockfile to ensure it's in sync
    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");
    LockSystem::generate_lockfile(".", deps);

    std::vector<std::string> args;
    args.push_back("build");
    args.push_back("-o");
    args.push_back(output_file);

    args.push_back("-I");
    args.push_back(".");

    fs::path comp_root = fs::path(compiler_path).parent_path().parent_path();
    if (fs::exists(comp_root / "std")) {
        args.push_back("-I");
        args.push_back(comp_root.string());
    }

    for (const auto& dep : deps) {
        args.push_back("-I");
        args.push_back(dep.path);
        args.push_back("-I");
        args.push_back(dep.path + "/src");
    }

    for (const auto& arg : forwarded_args) {
        args.push_back(arg);
    }

    args.push_back(entry_point);

    std::cout << "Building project '" << meta.name << "'...\n";
    int result = Lyra::run_command(compiler_path, args);
    if (result == -1) {
        std::cerr << "error: failed to run " << compiler_path << "\n";
    }
    if (result == 0) {
        std::cout << "Build successful.\n";
    }
    
    return result;
}
