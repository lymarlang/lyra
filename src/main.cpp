#include <iostream>
#include <string>
#include <vector>
#include "handlers.hh"

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
    std::cout << "  link    Link a local package as a dependency\n";
    std::cout << "  publish Publish the current project to the registry\n";
    std::cout << "  pack    Package the project into a tarball\n";
    std::cout << "  install Install a package from a tarball\n";
    std::cout << "  search  Search for packages in the registry\n";
    std::cout << "  info    Show information about a package\n";
    std::cout << "  test    Run project tests\n";
    std::cout << "  deps    List project dependencies\n";
    std::cout << "  tree    Display dependency tree\n";
    std::cout << "  why     Show why a package is included\n";
    std::cout << "  clean   Clean build artifacts\n";
    std::cout << "  doctor  Check environment for issues\n";
    std::cout << "  env     Show environment info\n";
    std::cout << "  audit   Audit dependencies for security\n";
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
    } else if (command == "link") {
        return handle_link(argc, argv);
    } else if (command == "publish") {
        return handle_publish(argc, argv);
    } else if (command == "pack") {
        return handle_pack(argc, argv);
    } else if (command == "install") {
        return handle_install(argc, argv);
    } else if (command == "search") {
        return handle_search(argc, argv);
    } else if (command == "info") {
        return handle_info(argc, argv);
    } else if (command == "test") {
        return handle_test(argc, argv);
    } else if (command == "deps") {
        return handle_deps(argc, argv);
    } else if (command == "tree") {
        return handle_tree(argc, argv);
    } else if (command == "why") {
        return handle_why(argc, argv);
    } else if (command == "clean") {
        return handle_clean();
    } else if (command == "doctor") {
        return handle_doctor();
    } else if (command == "env") {
        return handle_env();
    } else if (command == "audit") {
        return handle_audit(argc, argv);
    }

    std::cerr << "error: unknown command '" << command << "'\n\n";
    print_help();
    return 1;
}
