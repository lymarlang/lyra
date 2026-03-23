#ifndef REGISTRY_CLIENT_HH
#define REGISTRY_CLIENT_HH

#include "lyra_common.hh"
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
        std::string cmd = "curl -X POST -F \"name=" + name + "\" -F \"version=" + version + 
                          "\" -F \"file=@" + file_path + "\" " + url + " -s";
        
        std::cout << "Publishing to registry..." << std::endl;
        int result = std::system(cmd.c_str());
        return (result == 0) ? 0 : 1;
    }

    static std::string download(const std::string& name, const std::string& version) {
        fs::path cache_root = Lyra::get_cache_dir();
        fs::path pkg_dir = cache_root / (name + "-" + version);

        if (fs::exists(pkg_dir)) {
            return pkg_dir.string();
        }

        std::string url = get_registry_url() + "/packages/" + name + "/" + version + "/download";
        fs::create_directories(cache_root);
        fs::path tar_path = cache_root / (name + "-" + version + ".tar.gz");

        std::string cmd = "curl -L -o " + tar_path.string() + " " + url + " -s";
        std::cout << "Downloading '" << name << "@" << version << "' from registry..." << std::endl;
        
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "error: failed to download " << name << "\n";
            return "";
        }

        // Extract
        fs::create_directories(pkg_dir);
        std::string extract_cmd = "tar -xzf " + tar_path.string() + " -C " + pkg_dir.string() + " 2>/dev/null";
        std::system(extract_cmd.c_str());

        return pkg_dir.string();
    }

private:
};

#endif
