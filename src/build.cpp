#include "handlers.hh"
#include "config_parser.hh"
#include "process_helper.hh"
#include "resolver.hh"
#include "lock_system.hh"
#include "workspace.hh"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

int handle_build(int argc, char** argv) {
    bool build_workspace = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--workspace") {
            build_workspace = true;
        }
    }

    if (build_workspace || Workspace::is_workspace(".")) {
        std::cout << "Building workspace members...\n";
        auto members = Workspace::get_members(".");
        if (members.empty()) {
            std::cout << "No workspace members found.\n";
            return 0;
        }
        Workspace::resolve_workspace(".");
        int overall_result = 0;
        for (const auto& m : members) {
            std::cout << "==> Building member '" << m.name << "' in " << m.path << "\n";
            std::string orig_dir = fs::current_path().string();
            fs::current_path(m.path);
            std::vector<char*> member_argv;
            for (int a = 0; a < argc; ++a) {
                if (std::string(argv[a]) != "--workspace") {
                    member_argv.push_back(argv[a]);
                }
            }
            int res = handle_build(static_cast<int>(member_argv.size()), member_argv.data());
            fs::current_path(orig_dir);
            if (res != 0) overall_result = res;
        }
        return overall_result;
    }

    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    std::string entry_point = "src/main.lm";
    std::string output_file = meta.name;
    std::string target_triple = "";
    bool locked = false;
    bool frozen = false;
    bool offline = false;
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
            } else if ((arg == "--target" || arg == "-t") && i + 1 < argc) {
                target_triple = argv[++i];
            } else if (arg.rfind("--target=", 0) == 0) {
                target_triple = arg.substr(9);
            } else if (arg == "--locked") {
                locked = true;
            } else if (arg == "--frozen") {
                frozen = true;
                locked = true;
                offline = true;
            } else if (arg == "--offline") {
                offline = true;
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

    std::vector<ResolvedDependency> deps;
    if (locked || frozen) {
        if (!LockSystem::lockfile_exists(".")) {
            std::cerr << "error: --locked / --frozen specified but lymar.lock does not exist\n";
            return 1;
        }
        deps = LockSystem::read_lockfile(".");
        if (!LockSystem::verify_lockfile_integrity(".")) {
            std::cerr << "error: lockfile integrity verification failed\n";
            return 1;
        }
    } else {
        deps = DependencyResolver::resolve(".", {}, offline);
        LockSystem::generate_lockfile(".", deps);
    }

    std::vector<std::string> args;
    args.push_back("build");
    args.push_back("-o");
    args.push_back(output_file);

    if (!target_triple.empty()) {
        args.push_back("--target");
        args.push_back(target_triple);
    }

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
