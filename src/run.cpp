#include "handlers.hh"
#include "config_parser.hh"
#include "process_helper.hh"
#include "resolver.hh"
#include "lock_system.hh"
#include "watch_daemon.hh"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <sstream>

namespace fs = std::filesystem;

int handle_run(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    std::string entry_point = "src/main.lm";
    std::vector<std::string> forwarded_args;
    bool forwarding = false;
    std::vector<std::string> enabled_features;
    bool locked = false;
    bool frozen = false;
    bool offline = false;
    bool watch_mode = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--watch") {
            watch_mode = true;
            continue;
        }
    }

    if (watch_mode) {
        std::vector<char*> non_watch_argv;
        for (int i = 0; i < argc; ++i) {
            if (std::string(argv[i]) != "--watch") {
                non_watch_argv.push_back(argv[i]);
            }
        }
        std::vector<std::string> watch_dirs = {"src"};
        WatchDaemon::watch(watch_dirs, [&]() {
            handle_run(static_cast<int>(non_watch_argv.size()), non_watch_argv.data());
        });
        return 0;
    }

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
        if (arg == "--locked") {
            locked = true;
            continue;
        }
        if (arg == "--frozen") {
            frozen = true;
            locked = true;
            offline = true;
            continue;
        }
        if (arg == "--offline") {
            offline = true;
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
        deps = DependencyResolver::resolve(".", enabled_features, offline);
        LockSystem::generate_lockfile(".", deps);
    }

    std::vector<std::string> args;
    args.push_back("run");

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

    int result = Lyra::run_command(compiler_path, args);
    if (result == -1) {
        std::cerr << "error: failed to run " << compiler_path << "\n";
    }
    return result;
}
