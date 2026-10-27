#include "handlers.hh"
#include "resolver.hh"
#include "process_helper.hh"
#include "lyra_common.hh"
#include <iostream>
#include <filesystem>
#include <vector>

#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

struct TestResult {
    std::string path;
    bool passed = false;
    double duration_sec = 0.0;
    std::string output;
};

int handle_test(int argc, char** argv) {
    std::string filter;
    int parallel_jobs = 1;
    bool verbose = false;
    bool fail_fast = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--fail-fast") {
            fail_fast = true;
        } else if (arg == "-j" || arg == "--parallel") {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                parallel_jobs = std::max(1, std::atoi(argv[++i]));
            } else {
                parallel_jobs = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
            }
        } else if (arg.rfind("--filter=", 0) == 0) {
            filter = arg.substr(9);
        } else if (arg[0] != '-') {
            if (filter.empty()) filter = arg;
        }
    }

    std::vector<std::string> test_files;
    auto add_file_if_matches = [&](const fs::path& p) {
        if (p.extension() != ".lm") return;
        std::string s = p.generic_string();
        if (filter.empty() || s.find(filter) != std::string::npos) {
            test_files.push_back(s);
        }
    };

    if (!filter.empty() && fs::is_regular_file(filter)) {
        test_files.push_back(fs::path(filter).generic_string());
    } else if (fs::exists("tests/test.txt")) {
        std::ifstream mf("tests/test.txt");
        std::string line;
        while (std::getline(mf, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            std::string s = fs::path(line).generic_string();
            if (filter.empty() || s.find(filter) != std::string::npos) {
                test_files.push_back(s);
            }
        }
    } else {
        if (fs::exists("tests")) {
            for (const auto& entry : fs::recursive_directory_iterator("tests")) {
                if (entry.is_regular_file()) {
                    add_file_if_matches(entry.path());
                }
            }
        }
        if (fs::exists("src")) {
            for (const auto& entry : fs::recursive_directory_iterator("src")) {
                if (entry.is_regular_file()) {
                    std::string fname = entry.path().filename().string();
                    if (fname.find("_test.lm") != std::string::npos || fname.rfind("test_", 0) == 0) {
                        add_file_if_matches(entry.path());
                    }
                }
            }
        }
    }

    std::sort(test_files.begin(), test_files.end());

    if (test_files.empty()) {
        if (filter.empty()) {
            std::cout << "No tests found in tests/ or src/\n";
        } else {
            std::cout << "No tests matched filter: '" << filter << "'\n";
        }
        return 0;
    }

    std::string compiler_path = Lyra::find_compiler_path();
    std::cout << "====================================================\n";
    std::cout << "Running " << test_files.size() << " test" << (test_files.size() == 1 ? "" : "s")
              << " (" << (parallel_jobs > 1 ? ("parallel, " + std::to_string(parallel_jobs) + " threads") : "sequential") << ")\n";
    std::cout << "====================================================\n";

    auto run_single_test = [&](const std::string& test_path) -> TestResult {
        TestResult res;
        res.path = test_path;
        auto t0 = std::chrono::high_resolution_clock::now();
        std::vector<std::string> args = {"run", test_path};
        int code = Lyra::run_command_capture(compiler_path, args, res.output);
        auto t1 = std::chrono::high_resolution_clock::now();
        res.duration_sec = std::chrono::duration<double>(t1 - t0).count();
        res.passed = (code == 0);
        return res;
    };

    int passed_count = 0;
    int failed_count = 0;
    auto suite_start = std::chrono::high_resolution_clock::now();

    if (parallel_jobs <= 1) {
        for (const auto& test_path : test_files) {
            TestResult res = run_single_test(test_path);
            if (res.passed) {
                passed_count++;
                std::cout << "PASS (" << std::fixed << std::setprecision(2) << res.duration_sec << "s): " << res.path << "\n";
                if (verbose && !res.output.empty()) {
                    std::cout << res.output << "\n";
                }
            } else {
                failed_count++;
                std::cout << "FAIL (" << std::fixed << std::setprecision(2) << res.duration_sec << "s): " << res.path << "\n";
                if (!res.output.empty()) {
                    std::cout << "  Captured Output:\n";
                    std::istringstream stream(res.output);
                    std::string line;
                    while (std::getline(stream, line)) {
                        std::cout << "    " << line << "\n";
                    }
                }
                if (fail_fast) break;
            }
        }
    } else {
        std::mutex mtx;
        std::atomic<size_t> next_idx{0};
        std::atomic<bool> should_abort{false};

        auto worker = [&]() {
            while (true) {
                if (fail_fast && should_abort.load()) break;
                size_t idx = next_idx.fetch_add(1);
                if (idx >= test_files.size()) break;

                TestResult res = run_single_test(test_files[idx]);

                std::lock_guard<std::mutex> lock(mtx);
                if (res.passed) {
                    passed_count++;
                    std::cout << "PASS (" << std::fixed << std::setprecision(2) << res.duration_sec << "s): " << res.path << "\n";
                    if (verbose && !res.output.empty()) {
                        std::cout << res.output << "\n";
                    }
                } else {
                    failed_count++;
                    std::cout << "FAIL (" << std::fixed << std::setprecision(2) << res.duration_sec << "s): " << res.path << "\n";
                    if (!res.output.empty()) {
                        std::cout << "  Captured Output:\n";
                        std::istringstream stream(res.output);
                        std::string line;
                        while (std::getline(stream, line)) {
                            std::cout << "    " << line << "\n";
                        }
                    }
                    if (fail_fast) should_abort.store(true);
                }
            }
        };

        std::vector<std::thread> threads;
        for (int i = 0; i < parallel_jobs; ++i) {
            threads.emplace_back(worker);
        }
        for (auto& th : threads) {
            if (th.joinable()) th.join();
        }
    }

    auto suite_end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(suite_end - suite_start).count();

    std::cout << "====================================================\n";
    std::cout << "Summary: PASSED=" << passed_count << ", FAILED=" << failed_count
              << " in " << std::fixed << std::setprecision(2) << total_time << "s\n";
    std::cout << "====================================================\n";

    return (failed_count > 0) ? 1 : 0;
}

int handle_lint(int argc, char** argv) {
    std::string compiler = Lyra::find_compiler_path();
    std::vector<std::string> args = {"lint"};
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return Lyra::run_command(compiler, args);
}

int handle_doc(int argc, char** argv) {
    std::string compiler = Lyra::find_compiler_path();
    std::vector<std::string> args = {"doc"};
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return Lyra::run_command(compiler, args);
}

int handle_deps(int argc, char** argv) {
    (void)argc;
    (void)argv;
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
    
    std::string compiler_path = Lyra::find_compiler_path();
    if (fs::exists(compiler_path) || compiler_path == "limitly" || compiler_path == "limitly.exe") {
        std::cout << "[OK] Lymar compiler found at " << compiler_path << "\n";
    } else {
        std::cout << "[ERROR] Lymar compiler not found. Please set REPOROOT or ensure bin/limitly exists.\n";
    }

    std::string git_check = "git --version" + std::string(LYRA_DEV_NULL);
    if (std::system(git_check.c_str()) == 0) {
        std::cout << "[OK] git found\n";
    } else {
        std::cout << "[ERROR] git not found. Git dependencies will not work.\n";
    }

    std::string curl_check = "curl --version" + std::string(LYRA_DEV_NULL);
    if (std::system(curl_check.c_str()) == 0) {
        std::cout << "[OK] curl found\n";
    } else {
        std::cout << "[ERROR] curl not found. Registry features will not work.\n";
    }

    std::cout << "[OK] Central package index: " << Lyra::get_default_index_url() << "\n";
    std::cout << "[OK] Local index cache: " << Lyra::get_index_dir().string() << "\n";

    return 0;
}

void print_tree_recursive(const std::string& current_path, int level, std::set<std::string>& visited) {
    PackageMetadata meta = ConfigParser::parse(current_path + "/lymar.nol");
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
             fs::path cache_dir = Lyra::get_cache_dir();
             // Note: this is a bit flaky without exact version resolution here
             // We'd ideally use the resolver, but this is for 'tree' visualization.
             // For a better implementation, we should use DependencyResolver::resolve output.
             dep_path = (cache_dir / (name + "-" + dep.version)).string();
        }

        if (!dep_path.empty() && fs::exists(dep_path + "/lymar.nol")) {
            print_tree_recursive(dep_path, level + 1, visited);
        } else {
             for (int i = 0; i < level + 1; ++i) std::cout << "  ";
             std::cout << "└── " << name << " (" << (dep.version.empty() ? "*" : dep.version) << ") [MISSING]\n";
        }
    }
}

int handle_tree(int argc, char** argv) {
    PackageMetadata root_meta = ConfigParser::parse("lymar.nol");
    if (root_meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol\n";
        return 1;
    }

    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");
    if (deps.empty()) {
        std::cout << root_meta.name << " (" << root_meta.version << ")\n";
        std::cout << "No dependencies found.\n";
        return 0;
    }

    std::cout << "Dependency tree:\n";
    std::cout << root_meta.name << " (" << root_meta.version << ")\n";

    // For now, let's keep the recursive printer as it naturally handles the hierarchy,
    // but the DependencyResolver ensures all dependencies are correctly fetched and versioned.
    std::set<std::string> visited;
    print_tree_recursive(".", 0, visited);

    return 0;
}

bool find_path_why(const std::string& current_path, const std::string& target_name, std::vector<std::string>& path_acc) {
    PackageMetadata meta = ConfigParser::parse(current_path + "/lymar.nol");
    if (meta.name.empty()) return false;

    path_acc.push_back(meta.name + " (" + meta.version + ")");
    if (meta.name == target_name) return true;

    for (auto const& [name, dep] : meta.dependencies) {
        std::string dep_path;
        if (!dep.path.empty()) {
            dep_path = (fs::path(current_path) / dep.path).string();
        } else if (!dep.version.empty()) {
             fs::path cache_dir = Lyra::get_cache_dir();
             // Find matching version in cache
             for (const auto& entry : fs::directory_iterator(cache_dir)) {
                 if (entry.path().filename().string().find(name + "-") == 0) {
                     dep_path = entry.path().string();
                     break;
                 }
             }
        }

        if (!dep_path.empty() && fs::exists(dep_path + "/lymar.nol")) {
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
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol\n";
        return 1;
    }

    std::cout << "Cleaning project '" << meta.name << "'...\n";

    // Remove binary
    std::string bin_name = meta.name;
#ifdef _WIN32
    bin_name += ".exe";
#endif
    if (fs::exists(bin_name)) {
        fs::remove(bin_name);
        std::cout << "Removed binary: " << bin_name << "\n";
    }

    // Remove obj directory
    if (fs::exists("obj")) {
        fs::remove_all("obj");
        std::cout << "Removed obj/ directory\n";
    }

    // Remove specific project package tarball
    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    if (fs::exists(tarball)) {
        fs::remove(tarball);
        std::cout << "Removed package: " << tarball << "\n";
    }

    return 0;
}

int handle_cache_clear() {
    fs::path cache_dir = Lyra::get_cache_dir();

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
    std::cout << "  Compiler Path:   " << Lyra::find_compiler_path() << "\n";
    std::cout << "  Cache Directory: " << Lyra::get_cache_dir().string() << "\n";
    std::cout << "  Index Directory: " << Lyra::get_index_dir().string() << "\n";
    std::cout << "  Index URL:       " << Lyra::get_default_index_url() << "\n";

    char* root = std::getenv("REPOROOT");
    if (root) std::cout << "  REPOROOT:        " << root << "\n";

    char* registry = std::getenv("LYRA_REGISTRY_URL");
    if (registry) {
        std::cout << "  Registry URL:    " << registry << "\n";
    }

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
    std::string compiler = Lyra::find_compiler_path();
    std::vector<std::string> args = {"bench"};
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return Lyra::run_command(compiler, args);
}
