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
    }

    std::cerr << "error: unknown command '" << command << "'\n\n";
    print_help();
    return 1;
}
