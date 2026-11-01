#ifndef NATIVE_TOOLCHAIN_HH
#define NATIVE_TOOLCHAIN_HH

#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include "config_parser.hh"
#include "lyra_common.hh"

namespace fs = std::filesystem;

class NativeToolchain {
public:
    struct HostCompiler {
        std::string path;
        std::string type; // "clang", "gcc", "msvc"
        bool is_cpp;
    };

    static std::string discover_compiler(bool cpp = true) {
        // 1. Environment variables
        const char* env_c = cpp ? std::getenv("CXX") : std::getenv("CC");
        if (env_c && *env_c) return env_c;

        // 2. Clang / GCC search
        std::vector<std::string> candidates = cpp ?
            std::vector<std::string>{"clang++", "g++", "c++", "cl.exe"} :
            std::vector<std::string>{"clang", "gcc", "cc", "cl.exe"};

        for (const auto& cmd : candidates) {
            std::string check_cmd = "which " + cmd + LYRA_DEV_NULL;
#ifdef _WIN32
            check_cmd = "where " + cmd + LYRA_DEV_NULL;
#endif
            if (std::system(check_cmd.c_str()) == 0) {
                return cmd;
            }
        }

        return cpp ? "g++" : "gcc";
    }

    // Build native sources defined under [native] section in lymar.nol
    static bool build_native(const PackageMetadata& meta, const std::string& build_dir = "build/native") {
        if (meta.native_sources.empty()) {
            return true; // Nothing to build
        }

        std::cout << "[Native Toolchain] Discovered host C/C++ compiler for native bindings...\n";
        std::string cc = discover_compiler(false);
        std::string cxx = discover_compiler(true);

        fs::create_directories(build_dir);
        std::vector<std::string> obj_files;

        for (const auto& src : meta.native_sources) {
            if (!fs::exists(src)) {
                std::cerr << "error: native source file '" << src << "' not found\n";
                return false;
            }

            bool is_cpp = (src.rfind(".cpp") != std::string::npos ||
                           src.rfind(".cc") != std::string::npos ||
                           src.rfind(".cxx") != std::string::npos);
            std::string compiler = is_cpp ? cxx : cc;

            fs::path src_path(src);
            fs::path obj_path = fs::path(build_dir) / (src_path.stem().string() + ".o");
            obj_files.push_back(obj_path.string());

            std::string cmd = compiler + " -fPIC -c \"" + src + "\" -o \"" + obj_path.string() + "\"";
            for (const auto& inc : meta.native_include_dirs) {
                cmd += " -I\"" + inc + "\"";
            }
            for (const auto& flag : meta.native_cflags) {
                cmd += " " + flag;
            }

            std::cout << "  - Compiling " << src << " -> " << obj_path.string() << "\n";
            int res = std::system(cmd.c_str());
            if (res != 0) {
                std::cerr << "error: native compilation failed for " << src << "\n";
                return false;
            }
        }

        // Create static library archive
        std::string lib_name = "lib" + meta.name + "_native.a";
        fs::path lib_path = fs::path(build_dir) / lib_name;
        std::string ar_cmd = "ar rcs \"" + lib_path.string() + "\"";
        for (const auto& obj : obj_files) {
            ar_cmd += " \"" + obj + "\"";
        }
        std::cout << "  - Archiving native library: " << lib_path.string() << "\n";
        int ar_res = std::system(ar_cmd.c_str());
        if (ar_res != 0) {
            std::cerr << "error: failed to create native static archive " << lib_path.string() << "\n";
            return false;
        }

        std::cout << "[Native Toolchain] Built native bindings successfully: " << lib_path.string() << "\n";
        return true;
    }
};

#endif
