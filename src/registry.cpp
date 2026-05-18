#include "handlers.hh"
#include "config_parser.hh"
#include "registry_client.hh"
#include "nol.hh"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int handle_publish(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.toml");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.toml or package name\n";
        return 1;
    }

    std::cout << "Packaging project '" << meta.name << "' version " << meta.version << "...\n";
    
    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    std::string pack_cmd = "tar -czf " + tarball + " src lymar.toml";
    std::system(pack_cmd.c_str());

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

    std::cout << "Adding dependency '" << pkg_name << "' (" << pkg_version << ")...\n";

    PackageMetadata meta = ConfigParser::parse("lymar.toml");
    if (meta.name.empty()) {
        // Fallback or create new if not exists
        meta.name = fs::current_path().filename().string();
        meta.version = "0.1.0";
    }

    Dependency dep;
    dep.name = pkg_name;
    dep.version = pkg_version;
    meta.dependencies[pkg_name] = dep;

    NolDocument doc;
    doc.get_metadata() = meta;
    doc.save("lymar.toml");

    std::cout << "Updated lymar.toml. Run 'lyra update' to fetch dependencies.\n";
    return 0;
}
