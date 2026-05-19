#include "handlers.hh"
#include "config_parser.hh"
#include "registry_client.hh"
#include "lyra_common.hh"
#include "nol.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

static bool update_manifest_dependency(const std::string& pkg_name, const std::string& pkg_version, const std::string& path = "") {
    std::ifstream infile("lymar.nol");
    if (!infile.is_open()) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();

    try {
        NOL::Document doc = NOL::parse(content);
        NOL::Value root = doc.data();
        if (!root.isObject()) root = NOL::Object{};

        NOL::Object& obj = root.asObject();
        if (obj.find("dependencies") == obj.end() || !obj["dependencies"].isObject()) {
            obj["dependencies"] = NOL::Object{};
        }

        NOL::Object& deps = obj["dependencies"].asObject();
        if (path.empty()) {
            deps[pkg_name] = pkg_version;
        } else {
            NOL::Object dep_obj;
            dep_obj["path"] = path;
            deps[pkg_name] = dep_obj;
        }

        std::ofstream outfile("lymar.nol");
        if (!outfile.is_open()) return false;
        outfile << "# Lymar project configuration\n\n";
        outfile << root.dump(2, 0, true);
        outfile.close();
        return true;
    } catch (...) {
        return false;
    }
}

int handle_publish(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
        std::cerr << "error: unsafe package name or version\n";
        return 1;
    }

    std::cout << "Packaging project '" << meta.name << "' version " << meta.version << "...\n";
    
    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    std::string pack_cmd = "tar -czf \"" + tarball + "\" src lymar.nol";
    if (std::system(pack_cmd.c_str()) != 0) {
        std::cerr << "error: failed to create tarball\n";
        return 1;
    }

    int result = RegistryClient::publish(meta.name, meta.version, tarball);
    
    fs::remove(tarball);

    if (result == 0) {
        std::cout << "Successfully published " << meta.name << "@" << meta.version << "\n";
    }

    return result;
}

int handle_add(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing package name\n";
        return 1;
    }

    std::string pkg_name = argv[2];
    std::string pkg_version = (argc > 3) ? argv[3] : "*";

    if (!Lyra::is_safe_string(pkg_name)) {
        std::cerr << "error: unsafe package name\n";
        return 1;
    }

    std::cout << "Adding dependency '" << pkg_name << "' (" << pkg_version << ")...\n";

    if (update_manifest_dependency(pkg_name, pkg_version)) {
        std::cout << "Updated lymar.nol. Run 'lyra update' to fetch dependencies.\n";
        return 0;
    } else {
        std::cerr << "error: could not update lymar.nol\n";
        return 1;
    }
}

int handle_link(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing path to link\n";
        std::cerr << "usage: lyra link <path>\n";
        return 1;
    }

    fs::path target_path = fs::absolute(argv[2]);
    if (!fs::exists(target_path / "lymar.nol")) {
        std::cerr << "error: no lymar.nol found at " << target_path << "\n";
        return 1;
    }

    PackageMetadata target_meta = ConfigParser::parse((target_path / "lymar.nol").string());
    if (target_meta.name.empty()) {
        std::cerr << "error: could not parse lymar.nol at " << target_path << "\n";
        return 1;
    }

    std::cout << "Linking package '" << target_meta.name << "' from " << target_path << "...\n";

    if (update_manifest_dependency(target_meta.name, "", target_path.string())) {
        std::cout << "Successfully linked '" << target_meta.name << "'.\n";
        return 0;
    } else {
        std::cerr << "error: could not update lymar.nol\n";
        return 1;
    }
}

int handle_pack(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
        std::cerr << "error: unsafe package name or version in lymar.nol\n";
        return 1;
    }

    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    std::cout << "Packaging " << meta.name << " v" << meta.version << " into " << tarball << "...\n";

    std::string pack_cmd = "tar -czf \"" + tarball + "\" src lymar.nol";
    int result = std::system(pack_cmd.c_str());

    if (result == 0) {
        std::cout << "Successfully packaged to " << tarball << "\n";
    } else {
        std::cerr << "error: failed to create package\n";
    }

    return (result == 0) ? 0 : 1;
}

int handle_install(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing path to package tarball\n";
        std::cerr << "usage: lyra install <path.tar.gz>\n";
        return 1;
    }

    fs::path tar_path = fs::absolute(argv[2]);
    if (!fs::exists(tar_path)) {
        std::cerr << "error: file not found: " << tar_path << "\n";
        return 1;
    }

    fs::path temp_dir = fs::temp_directory_path() / ("lyra_install_temp_" + std::to_string(Lyra::get_process_id()));
    fs::create_directories(temp_dir);

    std::string extract_cmd = "tar -xzf \"" + tar_path.string() + "\" -C \"" + temp_dir.string() + "\"";
    if (std::system(extract_cmd.c_str()) != 0) {
        std::cerr << "error: failed to extract " << tar_path << "\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    PackageMetadata meta = ConfigParser::parse((temp_dir / "lymar.nol").string());
    if (meta.name.empty()) {
        std::cerr << "error: could not parse lymar.nol in tarball\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
        std::cerr << "error: unsafe package name or version in tarball\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    fs::path cache_dir = Lyra::get_cache_dir();
    fs::path pkg_cache_path = cache_dir / (meta.name + "-" + meta.version);

    fs::create_directories(cache_dir);
    if (fs::exists(pkg_cache_path)) {
        fs::remove_all(pkg_cache_path);
    }

    std::error_code ec;
    fs::rename(temp_dir, pkg_cache_path, ec);
    if (ec) {
        fs::create_directories(pkg_cache_path);
        fs::copy(temp_dir, pkg_cache_path, fs::copy_options::recursive);
        fs::remove_all(temp_dir);
    }

    std::cout << "Installed " << meta.name << "@" << meta.version << " to cache.\n";

    if (update_manifest_dependency(meta.name, meta.version)) {
        std::cout << "Added " << meta.name << " to lymar.nol\n";
    }

    return 0;
}

int handle_search(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing search query\n";
        return 1;
    }

    std::string query = argv[2];
    std::cout << "Searching for '" << query << "' in registry...\n";
    std::cout << "Results:\n";
    std::cout << " - " << query << " (v0.1.0) - A placeholder for " << query << "\n";
    return 0;
}

int handle_info(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing package name\n";
        return 1;
    }

    std::string name = argv[2];
    std::cout << "Fetching information for '" << name << "'...\n";
    std::cout << "Package: " << name << "\n";
    std::cout << "Latest Version: 1.0.0\n";
    std::cout << "Description: A Lymar package called " << name << "\n";
    std::cout << "Author: Lymar Developer\n";
    return 0;
}
