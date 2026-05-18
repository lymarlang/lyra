#include "handlers.hh"
#include "config_parser.hh"
#include "registry_client.hh"
#include "nol.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iterator>

namespace fs = std::filesystem;

int handle_publish(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    std::cout << "Packaging project '" << meta.name << "' version " << meta.version << "...\n";
    
    // Create a dummy tarball for now
    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    std::string pack_cmd = "tar -czf \"" + tarball + "\" src lymar.nol";
    std::system(pack_cmd.c_str());

    int result = RegistryClient::publish(meta.name, meta.version, tarball);
    
    // Cleanup
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

    std::cout << "Adding dependency '" << pkg_name << "' (" << pkg_version << ")...\n";

    // Update lymar.nol using NOL library
    NOL::Value root;
    std::ifstream file("lymar.nol");
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        try {
            root = NOL::parse(content);
        } catch (...) {
            root = NOL::Value(NOL::Object{});
        }
    } else {
        root = NOL::Value(NOL::Object{});
    }

    if (!root.isObject()) root = NOL::Value(NOL::Object{});
    
    if (!root.asObject().count("dependencies")) {
        root.asObject()["dependencies"] = NOL::Value(NOL::Object{});
    }

    if (!root.asObject().count("package")) {
        NOL::Object pkg;
        pkg["name"] = fs::current_path().filename().string();
        pkg["version"] = "0.1.0";
        root.asObject()["package"] = NOL::Value(pkg);
    }

    root.asObject()["dependencies"].asObject()[pkg_name] = NOL::Value(pkg_version);

    std::ofstream out("lymar.nol");
    if (out.is_open()) {
        out << root.dump(2, 0, true);
        out.close();
    } else {
        std::cerr << "error: could not update lymar.nol\n";
        return 1;
    }

    std::cout << "Updated lymar.nol. Run 'lyra update' to fetch dependencies.\n";
    return 0;
}
