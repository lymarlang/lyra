#include "handlers.hh"
#include "config_parser.hh"
#include "registry_client.hh"
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
    
    // Create a dummy tarball for now
    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    std::string pack_cmd = "tar -czf " + tarball + " src lymar.toml";
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

    // Update lymar.toml
    std::ifstream infile("lymar.toml");
    std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();

    if (content.find("[dependencies]") == std::string::npos) {
        content += "\n[dependencies]\n";
    }
    
    content += pkg_name + " = \"" + pkg_version + "\"\n";

    std::ofstream outfile("lymar.toml");
    outfile << content;
    outfile.close();

    std::cout << "Updated lymar.toml. Run 'lyra update' to fetch dependencies.\n";
    return 0;
}
