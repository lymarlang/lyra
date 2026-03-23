#include <iostream>
#include <string>
#include <vector>
#include "handlers.hh"
#include "config_parser.hh"

void print_help() {
    std::cout << "Lyra - Lymar (Limitly) Package Manager\n\n";
    std::cout << "Usage:\n";
    std::cout << "  lyra <command> [arguments]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  init    Initialize a new project\n";
    std::cout << "  run     Run the current project\n";
    std::cout << "  build   Build the current project\n";
    std::cout << "  update  Update dependencies and lockfile\n";
    std::cout << "  add     Add a dependency to the project\n";
    std::cout << "  publish Publish the current project to the registry\n";
    std::cout << "  test    Run project tests\n";
    std::cout << "  deps    List project dependencies\n";
    std::cout << "  doctor  Check environment for issues\n";
    std::cout << "  link    Link a local package\n";
    std::cout << "  pack    Package the current project\n";
    std::cout << "  install Install a local package tarball\n";
    std::cout << "  fetch   Fetch dependencies from lockfile\n";
    std::cout << "  tree    Display dependency tree\n";
    std::cout << "  why     Explain why a package is needed\n";
    std::cout << "  clean   Clean build artifacts\n";
    std::cout << "  cache   Manage lyra cache (e.g., lyra cache clear)\n";
    std::cout << "  lock    Update the lockfile\n";
    std::cout << "  verify  Verify lockfile integrity\n";
    std::cout << "  search  Search for packages\n";
    std::cout << "  info    Show package information\n";
    std::cout << "  new     Create a new project from a template\n";
    std::cout << "  env     Display lyra environment info\n";
    std::cout << "  audit   Audit dependencies for vulnerabilities\n";
    std::cout << "  bench   Run project benchmarks\n";
    std::cout << "  help    Show this help message\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string command = argv[1];

    if (command == "help" || command == "--help" || command == "-h") {
        print_help();
        return 0;
    }

    if (command == "init") {
        return handle_init();
    } else if (command == "run") {
        return handle_run(argc, argv);
    } else if (command == "build") {
        return handle_build(argc, argv);
    } else if (command == "update") {
        return handle_update(argc, argv);
    } else if (command == "add") {
        return handle_add(argc, argv);
    } else if (command == "publish") {
        return handle_publish(argc, argv);
    } else if (command == "test") {
        return handle_test(argc, argv);
    } else if (command == "deps") {
        return handle_deps(argc, argv);
    } else if (command == "doctor") {
        return handle_doctor();
    } else if (command == "link") {
        return handle_link(argc, argv);
    } else if (command == "pack") {
        return handle_pack(argc, argv);
    } else if (command == "install") {
        return handle_install(argc, argv);
    } else if (command == "fetch") {
        return handle_fetch(argc, argv);
    } else if (command == "tree") {
        return handle_tree(argc, argv);
    } else if (command == "why") {
        return handle_why(argc, argv);
    } else if (command == "clean") {
        return handle_clean();
    } else if (command == "cache") {
        if (argc > 2 && std::string(argv[2]) == "clear") {
            return handle_cache_clear();
        }
        std::cerr << "error: missing cache subcommand (e.g., 'clear')\n";
        return 1;
    } else if (command == "lock") {
        return handle_lock(argc, argv);
    } else if (command == "verify") {
        return handle_verify(argc, argv);
    } else if (command == "search") {
        return handle_search(argc, argv);
    } else if (command == "info") {
        return handle_info(argc, argv);
    } else if (command == "new") {
        return handle_new(argc, argv);
    } else if (command == "env") {
        return handle_env();
    } else if (command == "audit") {
        return handle_audit(argc, argv);
    } else if (command == "bench") {
        return handle_bench(argc, argv);
    }

    std::cerr << "error: unknown command '" << command << "'\n\n";
    print_help();
    return 1;
}
