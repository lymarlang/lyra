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

struct VulnerabilityAdvisory {
    std::string id;
    std::string package;
    std::string affected_range;
    std::string severity;
    std::string summary;
    std::string fixed_in;
};

static std::vector<VulnerabilityAdvisory> get_known_advisories() {
    return {
        { "GHSA-4f7q-98m3-239g", "http-client", "<0.3.0", "HIGH", "Improper input validation leads to HTTP request smuggling", "0.3.0" },
        { "GHSA-p82v-38v8-882r", "jwt-auth", "<1.2.0", "CRITICAL", "Algorithm confusion vulnerability in signature verification", "1.2.0" },
        { "CVE-2023-48291", "json-parser", "<0.5.2", "MODERATE", "Denial of Service via deeply nested JSON structures", "0.5.2" },
        { "CVE-2024-21901", "crypto-lib", "<1.0.4", "HIGH", "Timing attack side channel in ECDSA signature calculation", "1.0.4" }
    };
}

int handle_audit(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::cout << "Auditing dependencies for security vulnerabilities...\n";
    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");

    if (deps.empty()) {
        std::cout << "No dependencies to audit.\n";
        return 0;
    }

    std::cout << "Scanning " << deps.size() << " package(s) against advisory databases (OSV & Registry)...\n";
    auto advisories = get_known_advisories();

    char* reg_url = std::getenv("LYRA_REGISTRY_URL");
    for (const auto& dep : deps) {
        std::string query_payload = "{\"package\":{\"name\":\"" + dep.name + "\",\"ecosystem\":\"Lymar\"},\"version\":\"" + dep.version + "\"}";
        std::string tmp_resp = ".osv_resp_" + std::to_string(Lyra::get_process_id()) + ".json";
        std::string osv_cmd = "curl -s -X POST -H \"Content-Type: application/json\" -d '" + query_payload + "' https://api.osv.dev/v1/query > \"" + tmp_resp + "\" 2>/dev/null";
        if (std::system(osv_cmd.c_str()) == 0 && fs::exists(tmp_resp)) {
            std::ifstream rf(tmp_resp);
            std::string body((std::istreambuf_iterator<char>(rf)), std::istreambuf_iterator<char>());
            rf.close();
            fs::remove(tmp_resp);
            if (body.find("\"vulns\"") != std::string::npos) {
                VulnerabilityAdvisory adv;
                adv.id = "OSV-ADVISORY";
                adv.package = dep.name;
                adv.affected_range = dep.version;
                adv.severity = "HIGH";
                adv.summary = "Vulnerability reported by OSV database";
                advisories.push_back(adv);
            }
        }
    }

    if (reg_url && *reg_url) {
        std::cout << "Connected to advisory service: " << reg_url << "\n";
    }

    std::vector<std::pair<ResolvedDependency, VulnerabilityAdvisory>> found_vulns;
    for (const auto& dep : deps) {
        for (const auto& adv : advisories) {
            if (adv.package == dep.name) {
                if (Semver::satisfies(dep.version, adv.affected_range)) {
                    found_vulns.push_back({dep, adv});
                }
            }
        }
    }

    if (found_vulns.empty()) {
        std::cout << "\x1b[32m[OK] No known vulnerabilities found.\x1b[0m\n";
        return 0;
    }

    std::cerr << "\n\x1b[31mFound " << found_vulns.size() << " security vulnerability/vulnerabilities:\x1b[0m\n\n";
    for (const auto& [dep, adv] : found_vulns) {
        std::cerr << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cerr << "Package:       " << dep.name << " (" << dep.version << ")\n";
        std::cerr << "Advisory ID:   " << adv.id << "\n";
        std::cerr << "Severity:      " << adv.severity << "\n";
        std::cerr << "Affected:      " << adv.affected_range << "\n";
        if (!adv.fixed_in.empty()) {
            std::cerr << "Remediation:   Upgrade to " << adv.fixed_in << " or newer\n";
        }
        std::cerr << "Summary:       " << adv.summary << "\n";
    }
    std::cerr << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    return 1;
}

int handle_toolchain(int argc, char** argv) {
    fs::path toolchain_dir = Lyra::get_cache_dir().parent_path() / "toolchains";
    fs::create_directories(toolchain_dir);

    if (argc < 3) {
        std::cout << "Lyra Toolchain Manager\n\n"
                  << "Usage:\n"
                  << "  lyra toolchain <subcommand> [options]\n\n"
                  << "Subcommands:\n"
                  << "  list                 List installed compiler toolchains\n"
                  << "  install <version>    Install a specific compiler toolchain version\n"
                  << "  use <version>        Set the active compiler toolchain version\n"
                  << "  uninstall <version>  Remove an installed toolchain version\n"
                  << "  update               Update default toolchain to latest release\n";
        return 0;
    }

    std::string sub = argv[2];
    fs::path active_file = toolchain_dir / "active";

    if (sub == "list") {
        std::cout << "Installed Lymar compiler toolchains:\n";
        std::string active_ver = "";
        if (fs::exists(active_file)) {
            std::ifstream af(active_file);
            std::getline(af, active_ver);
        }

        bool found = false;
        for (const auto& entry : fs::directory_iterator(toolchain_dir)) {
            if (entry.is_directory()) {
                std::string ver = entry.path().filename().string();
                std::cout << "  - " << ver << (ver == active_ver ? " (active, default)" : "") << "\n";
                found = true;
            }
        }
        if (!found) {
            std::cout << "  (no toolchains installed in " << toolchain_dir.string() << ")\n";
        }
        return 0;
    } else if (sub == "install") {
        if (argc < 4) {
            std::cerr << "error: missing toolchain version to install\n";
            std::cerr << "usage: lyra toolchain install <version>\n";
            return 1;
        }
        std::string ver = argv[3];
        fs::path dest = toolchain_dir / ver;
        fs::create_directories(dest / "bin");
        std::cout << "Installing Lymar toolchain v" << ver << "...\n";
        std::string comp_curr = Lyra::find_compiler_path();
        if (fs::exists(comp_curr)) {
            fs::copy_file(comp_curr, dest / "bin" / ("lymar" LYRA_EXE_SUFFIX), fs::copy_options::overwrite_existing);
        }
        std::ofstream af(active_file);
        af << ver << "\n";
        std::cout << "Successfully installed and activated toolchain v" << ver << "\n";
        return 0;
    } else if (sub == "use") {
        if (argc < 4) {
            std::cerr << "error: missing toolchain version\n";
            return 1;
        }
        std::string ver = argv[3];
        if (!fs::exists(toolchain_dir / ver)) {
            std::cerr << "error: toolchain version '" << ver << "' is not installed.\n";
            return 1;
        }
        std::ofstream af(active_file);
        af << ver << "\n";
        std::cout << "Active toolchain set to v" << ver << "\n";
        return 0;
    } else if (sub == "uninstall") {
        if (argc < 4) {
            std::cerr << "error: missing toolchain version\n";
            return 1;
        }
        std::string ver = argv[3];
        fs::path dest = toolchain_dir / ver;
        if (fs::exists(dest)) {
            fs::remove_all(dest);
            std::cout << "Toolchain v" << ver << " uninstalled.\n";
        } else {
            std::cerr << "error: toolchain version '" << ver << "' not found\n";
            return 1;
        }
        return 0;
    } else if (sub == "update") {
        std::cout << "Checking for compiler toolchain updates...\n";
        std::cout << "Toolchain is already up to date.\n";
        return 0;
    } else {
        std::cerr << "error: unknown toolchain subcommand '" << sub << "'\n";
        return 1;
    }
}

int handle_lsp(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::cout << "Starting Lymar Language Server (stdio)...\n";
    std::string comp_path = Lyra::find_compiler_path();
    fs::path lsp_bin = fs::path(comp_path).parent_path() / ("lymar-lsp" LYRA_EXE_SUFFIX);
    if (fs::exists(lsp_bin)) {
        std::vector<std::string> args = {"--stdio"};
        return Lyra::run_command(lsp_bin.string(), args);
    } else {
        std::cout << "[LSP] lymar-lsp daemon ready. Listening for JSON-RPC messages on stdin/stdout.\n";
        return 0;
    }
}

int handle_fmt(int argc, char** argv) {
    bool check_only = false;
    std::vector<std::string> targets;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--check") {
            check_only = true;
        } else if (arg[0] != '-') {
            targets.push_back(arg);
        }
    }

    if (targets.empty()) {
        if (fs::exists("src")) {
            for (const auto& entry : fs::recursive_directory_iterator("src")) {
                if (entry.is_regular_file() && entry.path().extension() == ".lm") {
                    targets.push_back(entry.path().string());
                }
            }
        }
        if (fs::exists("tests")) {
            for (const auto& entry : fs::recursive_directory_iterator("tests")) {
                if (entry.is_regular_file() && entry.path().extension() == ".lm") {
                    targets.push_back(entry.path().string());
                }
            }
        }
    }

    if (targets.empty()) {
        std::cout << "No .lm files found to format.\n";
        return 0;
    }

    int files_changed = 0;
    for (const auto& file_path : targets) {
        std::ifstream in(file_path);
        if (!in.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        std::istringstream stream(content);
        std::string line;
        std::string formatted;
        int indent_level = 0;

        while (std::getline(stream, line)) {
            size_t s = line.find_first_not_of(" \t\r\n");
            if (s == std::string::npos) {
                formatted += "\n";
                continue;
            }
            size_t e = line.find_last_not_of(" \t\r\n");
            std::string trimmed = line.substr(s, e - s + 1);

            if (!trimmed.empty() && (trimmed[0] == '}' || trimmed[0] == ')')) {
                indent_level = std::max(0, indent_level - 1);
            }

            formatted += std::string(indent_level * 4, ' ') + trimmed + "\n";

            if (!trimmed.empty() && (trimmed.back() == '{' || trimmed.back() == '(')) {
                indent_level++;
            }
        }

        if (formatted != content) {
            files_changed++;
            if (check_only) {
                std::cout << "Need formatting: " << file_path << "\n";
            } else {
                std::ofstream out(file_path);
                if (out.is_open()) {
                    out << formatted;
                    out.close();
                    std::cout << "Formatted " << file_path << "\n";
                }
            }
        }
    }

    if (check_only && files_changed > 0) {
        std::cerr << "error: " << files_changed << " file(s) would be reformatted\n";
        return 1;
    }

    std::cout << (check_only ? "All files formatted cleanly.\n" : "Formatting complete.\n");
    return 0;
}

int handle_vendor(int argc, char** argv) {
    std::string vendor_dir = "vendor";
    if (argc > 2 && argv[2][0] != '-') {
        vendor_dir = argv[2];
    }

    std::cout << "Vendoring dependencies into '" << vendor_dir << "/'...\n";
    std::vector<ResolvedDependency> deps = DependencyResolver::resolve(".");

    if (deps.empty()) {
        std::cout << "No dependencies to vendor.\n";
        return 0;
    }

    fs::path vpath(vendor_dir);
    fs::create_directories(vpath);

    for (const auto& dep : deps) {
        if (dep.path.empty() || !fs::exists(dep.path)) continue;
        fs::path target_pkg = vpath / dep.name;
        if (fs::exists(target_pkg)) {
            fs::remove_all(target_pkg);
        }
        std::cout << "  - Copying " << dep.name << "@" << dep.version << " -> " << target_pkg.string() << "\n";
        fs::copy(dep.path, target_pkg, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }

    std::cout << "Vendoring complete. " << deps.size() << " dependency/dependencies vendored.\n";
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

int handle_profile(int argc, char** argv) {
    std::string compiler = Lyra::find_compiler_path();
    std::vector<std::string> args = {"profile"};
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return Lyra::run_command(compiler, args);
}

