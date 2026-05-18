#ifndef REGISTRY_CLIENT_HH
#define REGISTRY_CLIENT_HH

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

class RegistryClient {
public:
    static std::string get_registry_url() {
        char* url = std::getenv("LYRA_REGISTRY_URL");
        return url ? std::string(url) : "http://localhost:8080/api";
    }

    static int publish(const std::string& name, const std::string& version, const std::string& file_path) {
        std::string url = get_registry_url() + "/publish";
        // Using -f to fail on HTTP errors and -sS to show errors even in silent mode
        std::string cmd = "curl -f -sS -X POST -F \"name=" + name + "\" -F \"version=" + version +
                          "\" -F \"file=@" + file_path + "\" \"" + url + "\"";
        
        std::cout << "Publishing to registry..." << std::endl;
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "error: failed to publish package to registry (exit code " << result << ")\n";
            return 1;
        }
        return 0;
    }

    static std::string download(const std::string& name, const std::string& version) {
        fs::path cache_root = get_cache_dir();
        fs::path pkg_dir = cache_root / (name + "-" + version);

        if (fs::exists(pkg_dir)) {
            return pkg_dir.string();
        }

        std::string url = get_registry_url() + "/packages/" + name + "/" + version + "/download";
        fs::create_directories(cache_root);
        fs::path tar_path = cache_root / (name + "-" + version + ".tar.gz");

        std::string cmd = "curl -f -sS -L -o \"" + tar_path.string() + "\" \"" + url + "\"";
        std::cout << "Downloading '" << name << "@" << version << "' from registry..." << std::endl;
        
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "error: failed to download " << name << " from registry (exit code " << result << ")\n";
            if (fs::exists(tar_path)) fs::remove(tar_path);
            return "";
        }

        // Extract
        fs::create_directories(pkg_dir);
        std::string extract_cmd = "tar -xzf \"" + tar_path.string() + "\" -C \"" + pkg_dir.string() + "\" 2>/dev/null";
        int extract_result = std::system(extract_cmd.c_str());

        if (extract_result != 0) {
            std::cerr << "error: failed to extract package " << name << "\n";
            fs::remove_all(pkg_dir);
            fs::remove(tar_path);
            return "";
        }

        return pkg_dir.string();
    }

private:
    static fs::path get_cache_dir() {
        char* home = std::getenv("HOME");
        if (home) {
            return fs::path(home) / ".lyra" / "cache";
        }
        return fs::current_path() / ".lyra_cache";
    }
};

#endif
