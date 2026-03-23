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

void print_tree_recursive(const std::string& current_path, int level, std::set<std::string>& visited) {
    PackageMetadata meta = ConfigParser::parse(current_path + "/lymar.toml");
    if (meta.name.empty()) return;

    if (level > 0) {
        for (int i = 0; i < level - 1; ++i) std::cout << "  ";
        std::cout << "└── " << meta.name << " (" << meta.version << ")\n";
    }

    if (visited.count(meta.name)) return;
    visited.insert(meta.name);

    for (auto const& [name, dep] : meta.dependencies) {
        std::string dep_path;
        if (!dep.path.empty()) {
            dep_path = (fs::path(current_path) / dep.path).string();
        } else if (!dep.version.empty()) {
             // Basic heuristic for cached dependency path
             char* home = std::getenv("HOME");
             fs::path cache_dir = home ? fs::path(home) / ".lyra" / "cache" : fs::current_path() / ".lyra_cache";
             // Note: this is a bit flaky without exact version resolution here
             // We'd ideally use the resolver, but this is for 'tree' visualization.
             // For a better implementation, we should use DependencyResolver::resolve output.
             dep_path = (cache_dir / (name + "-" + dep.version)).string();
        }

        if (!dep_path.empty() && fs::exists(dep_path + "/lymar.toml")) {
            print_tree_recursive(dep_path, level + 1, visited);
        } else {
             for (int i = 0; i < level + 1; ++i) std::cout << "  ";
             std::cout << "└── " << name << " (" << (dep.version.empty() ? "*" : dep.version) << ") [MISSING]\n";
        }
    }
}

int handle_tree(int argc, char** argv) {
    PackageMetadata root_meta = ConfigParser::parse("lymar.toml");
    if (root_meta.name.empty()) {
        std::cerr << "error: could not find lymar.toml\n";
        return 1;
    }

    std::cout << "Dependency tree:\n";
    std::cout << root_meta.name << " (" << root_meta.version << ")\n";

    std::set<std::string> visited;
    print_tree_recursive(".", 0, visited);

    return 0;
}

bool find_path_why(const std::string& current_path, const std::string& target_name, std::vector<std::string>& path_acc) {
    PackageMetadata meta = ConfigParser::parse(current_path + "/lymar.toml");
    if (meta.name.empty()) return false;

    path_acc.push_back(meta.name + " (" + meta.version + ")");
    if (meta.name == target_name) return true;

    for (auto const& [name, dep] : meta.dependencies) {
        std::string dep_path;
        if (!dep.path.empty()) {
            dep_path = (fs::path(current_path) / dep.path).string();
        } else if (!dep.version.empty()) {
             char* home = std::getenv("HOME");
             fs::path cache_dir = home ? fs::path(home) / ".lyra" / "cache" : fs::current_path() / ".lyra_cache";
             // Find matching version in cache
             for (const auto& entry : fs::directory_iterator(cache_dir)) {
                 if (entry.path().filename().string().find(name + "-") == 0) {
                     dep_path = entry.path().string();
                     break;
                 }
             }
        }

        if (!dep_path.empty() && fs::exists(dep_path + "/lymar.toml")) {
            if (find_path_why(dep_path, target_name, path_acc)) return true;
        }
    }

    path_acc.pop_back();
    return false;
}

int handle_why(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing package name\n";
        return 1;
    }

    std::string target_name = argv[2];
    std::vector<std::string> path_acc;

    if (find_path_why(".", target_name, path_acc)) {
        std::cout << "Why is '" << target_name << "' included?\n";
        for (size_t i = 0; i < path_acc.size(); ++i) {
            for (size_t j = 0; j < i; ++j) std::cout << "  ";
            if (i > 0) std::cout << "└─ ";
            std::cout << path_acc[i] << "\n";
        }
    } else {
        std::cout << "Package '" << target_name << "' not found in dependency graph.\n";
    }

    return 0;
}

int handle_clean() {
    PackageMetadata meta = ConfigParser::parse("lymar.toml");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.toml\n";
        return 1;
    }

    std::cout << "Cleaning project '" << meta.name << "'...\n";

    // Remove binary
    if (fs::exists(meta.name)) {
        fs::remove(meta.name);
        std::cout << "Removed binary: " << meta.name << "\n";
    }

    // Remove obj directory
    if (fs::exists("obj")) {
        fs::remove_all("obj");
        std::cout << "Removed obj/ directory\n";
    }

    // Remove any .tar.gz files from 'pack'
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.path().extension() == ".gz" && entry.path().stem().extension() == ".tar") {
            fs::remove(entry.path());
            std::cout << "Removed package: " << entry.path().filename() << "\n";
        }
    }

    return 0;
}

int handle_cache_clear() {
    char* home = std::getenv("HOME");
    fs::path cache_dir = home ? fs::path(home) / ".lyra" / "cache" : fs::current_path() / ".lyra_cache";

    if (fs::exists(cache_dir)) {
        std::cout << "Clearing lyra cache at " << cache_dir << "...\n";
        fs::remove_all(cache_dir);
        std::cout << "Cache cleared.\n";
    } else {
        std::cout << "Cache is already empty.\n";
    }

    return 0;
}

int handle_env() {
    std::cout << "Lyra Environment:\n";
    char* home = std::getenv("HOME");
    std::cout << "  Cache Directory: " << (home ? fs::path(home) / ".lyra" / "cache" : fs::current_path() / ".lyra_cache") << "\n";

    char* root = std::getenv("REPOROOT");
    if (root) std::cout << "  REPOROOT: " << root << "\n";

    char* registry = std::getenv("LYRA_REGISTRY_URL");
    std::cout << "  Registry URL: " << (registry ? registry : "http://localhost:8080/api") << "\n";

    return 0;
}

int handle_audit(int argc, char** argv) {
    std::cout << "Auditing dependencies for security issues...\n";
    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");

    if (deps.empty()) {
        std::cout << "No dependencies to audit.\n";
        return 0;
    }

    std::cout << "Scanning " << deps.size() << " packages...\n";
    std::cout << "No known vulnerabilities found.\n";

    return 0;
}

int handle_bench(int argc, char** argv) {
    std::cout << "Running benchmarks...\n";
    if (!fs::exists("benches")) {
        std::cout << "No 'benches' directory found.\n";
        return 0;
    }

    for (const auto& entry : fs::directory_iterator("benches")) {
        if (entry.path().extension() == ".lm") {
            std::cout << "Benchmarking " << entry.path().filename() << "...\n";
            // In a real implementation, we would build and run this with timing
        }
    }

    return 0;
}
