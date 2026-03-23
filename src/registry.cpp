#include "handlers.hh"
#include "config_parser.hh"
#include "registry_client.hh"
#include "lyra_common.hh"
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
    std::string pack_cmd = "tar -czf \"" + tarball + "\" src lymar.toml";
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
    if (!infile.is_open()) {
        std::cerr << "error: could not open lymar.toml\n";
        return 1;
    }
    std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();

    size_t deps_section = content.find("[dependencies]");
    if (deps_section == std::string::npos) {
        content += "\n[dependencies]\n";
        deps_section = content.find("[dependencies]");
    }
    
    // Use safer check for existing dependency within the [dependencies] section
    std::string key = pkg_name + " =";
    size_t pos = content.find("\n" + key, deps_section);

    if (pos != std::string::npos) {
        pos++;
        std::cout << "Package '" << pkg_name << "' already in lymar.toml. Updating version...\n";
        size_t end = content.find('\n', pos);
        if (end == std::string::npos) end = content.length();
        content.replace(pos, end - pos, pkg_name + " = \"" + pkg_version + "\"");
    } else {
        if (content.empty() || content.back() != '\n') content += "\n";
        content += pkg_name + " = \"" + pkg_version + "\"\n";
    }

    std::ofstream outfile("lymar.toml");
    outfile << content;
    outfile.close();

    std::cout << "Updated lymar.toml. Run 'lyra update' to fetch dependencies.\n";
    return 0;
}

int handle_link(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing path to link\n";
        std::cerr << "usage: lyra link <path>\n";
        return 1;
    }

    fs::path target_path = fs::absolute(argv[2]);
    if (!fs::exists(target_path / "lymar.toml")) {
        std::cerr << "error: no lymar.toml found at " << target_path << "\n";
        return 1;
    }

    PackageMetadata target_meta = ConfigParser::parse((target_path / "lymar.toml").string());
    if (target_meta.name.empty()) {
        std::cerr << "error: could not parse lymar.toml at " << target_path << "\n";
        return 1;
    }

    std::cout << "Linking package '" << target_meta.name << "' from " << target_path << "...\n";

    // Update current project's lymar.toml
    std::ifstream infile("lymar.toml");
    if (!infile.is_open()) {
        std::cerr << "error: could not open lymar.toml\n";
        return 1;
    }
    std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();

    size_t deps_section = content.find("[dependencies]");
    if (deps_section == std::string::npos) {
        content += "\n[dependencies]\n";
        deps_section = content.find("[dependencies]");
    }

    // Check if already linked or added within the [dependencies] section
    std::string key = target_meta.name + " =";
    size_t pos = content.find("\n" + key, deps_section);

    if (pos != std::string::npos) {
        pos++; // Move past the newline
        std::cout << "Warning: package '" << target_meta.name << "' already exists in lymar.toml. Overwriting...\n";
        size_t end = content.find('\n', pos);
        if (end == std::string::npos) end = content.length();
        content.replace(pos, end - pos, target_meta.name + " = { path = \"" + target_path.string() + "\" }");
    } else {
        if (content.empty() || content.back() != '\n') content += "\n";
        content += target_meta.name + " = { path = \"" + target_path.string() + "\" }\n";
    }

    std::ofstream outfile("lymar.toml");
    outfile << content;
    outfile.close();

    std::cout << "Successfully linked '" << target_meta.name << "'.\n";
    return 0;
}

int handle_pack(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.toml");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.toml or package name\n";
        return 1;
    }

    if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
        std::cerr << "error: unsafe package name or version in lymar.toml\n";
        return 1;
    }

    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    std::cout << "Packaging " << meta.name << " v" << meta.version << " into " << tarball << "...\n";

    std::string pack_cmd = "tar -czf \"" + tarball + "\" src lymar.toml";
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

    // Extract name and version from filename (basic heuristic) or extract and parse
    // For now, let's extract to a temporary directory to get metadata
    fs::path temp_dir = fs::temp_directory_path() / ("lyra_install_temp_" + std::to_string(Lyra::get_process_id()));
    fs::create_directories(temp_dir);

    std::string extract_cmd = "tar -xzf \"" + tar_path.string() + "\" -C \"" + temp_dir.string() + "\"";
    if (std::system(extract_cmd.c_str()) != 0) {
        std::cerr << "error: failed to extract " << tar_path << "\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    PackageMetadata meta = ConfigParser::parse((temp_dir / "lymar.toml").string());
    if (meta.name.empty()) {
        std::cerr << "error: could not parse lymar.toml in tarball\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
        std::cerr << "error: unsafe package name or version in tarball\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    // Move to cache
    fs::path cache_dir = Lyra::get_cache_dir();
    fs::path pkg_cache_path = cache_dir / (meta.name + "-" + meta.version);

    fs::create_directories(cache_dir);
    if (fs::exists(pkg_cache_path)) {
        fs::remove_all(pkg_cache_path);
    }

    std::error_code ec;
    fs::rename(temp_dir, pkg_cache_path, ec);
    if (ec) {
        // Fallback for cross-device move
        fs::create_directories(pkg_cache_path);
        fs::copy(temp_dir, pkg_cache_path, fs::copy_options::recursive);
        fs::remove_all(temp_dir);
    }

    std::cout << "Installed " << meta.name << "@" << meta.version << " to cache.\n";

    // Add to current project's lymar.toml
    std::ifstream infile("lymar.toml");
    if (infile.is_open()) {
        std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        infile.close();

        size_t deps_section = content.find("[dependencies]");
        if (deps_section == std::string::npos) {
            content += "\n[dependencies]\n";
            deps_section = content.find("[dependencies]");
        }

        std::string key = meta.name + " =";
        size_t pos = content.find("\n" + key, deps_section);

        if (pos == std::string::npos) {
            if (content.empty() || content.back() != '\n') content += "\n";
            content += meta.name + " = \"" + meta.version + "\"\n";
            std::ofstream outfile("lymar.toml");
            outfile << content;
            outfile.close();
            std::cout << "Added " << meta.name << " to lymar.toml\n";
        } else {
            std::cout << "Package " << meta.name << " already in lymar.toml\n";
        }
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

    // Placeholder: In a real registry, this would call an API
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

    // Placeholder
    std::cout << "Package: " << name << "\n";
    std::cout << "Latest Version: 1.0.0\n";
    std::cout << "Description: A Lymar package called " << name << "\n";
    std::cout << "Author: Lymar Developer\n";

    return 0;
}
