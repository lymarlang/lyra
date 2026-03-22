#include "handlers.hh"
#include "resolver.hh"
#include "process_helper.hh"
#include <iostream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

int handle_test(int argc, char** argv) {
    std::cout << "Running tests...\n";
    
    std::vector<std::string> test_files;
    if (fs::exists("tests")) {
        for (const auto& entry : fs::directory_iterator("tests")) {
            if (entry.path().extension() == ".lm") {
                test_files.push_back(entry.path().string());
            }
        }
    }

    if (test_files.empty()) {
        std::cout << "No tests found in tests/ directory.\n";
        return 0;
    }

    char* root_dir = getenv("REPOROOT");
    std::string compiler_path = root_dir ? std::string(root_dir) + "/bin/limitly" : "../bin/limitly";

    int failed = 0;
    for (const auto& test : test_files) {
        std::cout << "Testing " << test << "... ";
        std::vector<std::string> args = {test};
        int result = Lyra::run_command(compiler_path, args);
        if (result == 0) {
            std::cout << "PASSED\n";
        } else {
            std::cout << "FAILED\n";
            failed++;
        }
    }

    if (failed > 0) {
        std::cout << "\nTest session failed: " << failed << " tests failed.\n";
        return 1;
    }

    std::cout << "\nTest session passed.\n";
    return 0;
}

int handle_deps(int argc, char** argv) {
    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");
    if (deps.empty()) {
        std::cout << "No dependencies found.\n";
        return 0;
    }

    std::cout << "Dependency tree:\n";
    for (const auto& dep : deps) {
        std::cout << "├── " << dep.name << " (" << dep.version << ")\n";
        std::cout << "│   └── " << dep.path << "\n";
    }
    return 0;
}

int handle_doctor() {
    std::cout << "Checking environment...\n";
    
    char* root_dir = getenv("REPOROOT");
    std::string compiler_path = root_dir ? std::string(root_dir) + "/bin/limitly" : "../bin/limitly";
    
    if (fs::exists(compiler_path)) {
        std::cout << "[OK] Lymar compiler found at " << compiler_path << "\n";
    } else {
        std::cout << "[ERROR] Lymar compiler not found. Please set REPOROOT or ensure bin/limitly exists.\n";
    }

    if (std::system("git --version > /dev/null 2>&1") == 0) {
        std::cout << "[OK] git found\n";
    } else {
        std::cout << "[ERROR] git not found. Git dependencies will not work.\n";
    }

    if (std::system("curl --version > /dev/null 2>&1") == 0) {
        std::cout << "[OK] curl found\n";
    } else {
        std::cout << "[ERROR] curl not found. Registry features will not work.\n";
    }

    return 0;
}
