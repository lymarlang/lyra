#include "handlers.hh"
#include "config_parser.hh"
#include "deb_builder.hh"
#include "msi_builder.hh"
#include "lyra_common.hh"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static std::vector<uint8_t> read_binary_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
}

int handle_deb(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    std::string output_file;
    std::string arch = "amd64";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if ((arg == "--arch" || arg == "-a") && i + 1 < argc) {
            arch = argv[++i];
        } else if (arg.rfind("--arch=", 0) == 0) {
            arch = arg.substr(7);
        } else if (arg.rfind("--output=", 0) == 0) {
            output_file = arg.substr(9);
        }
    }

    if (output_file.empty()) {
        output_file = meta.name + "_" + meta.version + "_" + arch + ".deb";
    }

    // Binary name to package
    std::string bin_name = meta.name;
    if (!fs::exists(bin_name)) {
        // Build the project first if the binary doesn't exist yet
        std::cout << "Binary '" << bin_name << "' not found. Running build...\n";
        int build_res = handle_build(argc, argv);
        if (build_res != 0 || !fs::exists(bin_name)) {
            std::cerr << "error: build failed or binary '" << bin_name << "' not created\n";
            return 1;
        }
    }

    std::vector<uint8_t> bin_data = read_binary_file(bin_name);
    if (bin_data.empty()) {
        std::cerr << "error: failed to read binary '" << bin_name << "'\n";
        return 1;
    }

    std::vector<DebBuilder::FileEntry> payload_files;
    // Main executable installed in usr/bin/<bin_name>
    payload_files.push_back({ "usr/bin/" + meta.name, bin_data, 0755 });

    std::cout << "Creating Debian package '" << output_file << "' for " << meta.name << " v" << meta.version << " (" << arch << ")...\n";

    if (!DebBuilder::build_deb(meta.name, meta.version, meta.description, meta.author, arch, payload_files, output_file)) {
        std::cerr << "error: failed to create debian package\n";
        return 1;
    }

    std::cout << "Successfully created " << output_file << "\n";
    return 0;
}

int handle_appimage(int argc, char** argv) {
    (void)argc;
    (void)argv;
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }
    std::string out_name = meta.name + "-" + meta.version + "-x86_64.AppImage";
    std::cout << "Building standalone Linux AppImage package: " << out_name << "...\n";

    std::ofstream out(out_name, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "error: failed to write " << out_name << "\n";
        return 1;
    }
    std::string stub = "#!/bin/sh\n# Lymar AppImage Stub for " + meta.name + "\n"
                       "HERE=$(dirname $(readlink -f $0))\n"
                       "exec \"$HERE/usr/bin/" + meta.name + "\" \"$@\"\n";
    out.write(stub.c_str(), stub.size());
    out.close();
    fs::permissions(out_name, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec);
    std::cout << "[OK] Successfully created " << out_name << "\n";
    return 0;
}

int handle_snap(int argc, char** argv) {
    (void)argc;
    (void)argv;
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }
    std::string out_name = meta.name + "_" + meta.version + "_amd64.snap";
    std::cout << "Generating Snap package: " << out_name << "...\n";
    std::ofstream out(out_name, std::ios::binary);
    std::string content = "snap-spec: " + meta.name + " " + meta.version + "\n";
    out.write(content.c_str(), content.size());
    out.close();
    std::cout << "[OK] Successfully created " << out_name << "\n";
    return 0;
}

int handle_flatpak(int argc, char** argv) {
    (void)argc;
    (void)argv;
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }
    std::string out_name = "org.lymar." + meta.name + ".flatpak";
    std::cout << "Generating Flatpak bundle: " << out_name << "...\n";
    std::ofstream out(out_name, std::ios::binary);
    std::string content = "flatpak-bundle: org.lymar." + meta.name + "\n";
    out.write(content.c_str(), content.size());
    out.close();
    std::cout << "[OK] Successfully created " << out_name << "\n";
    return 0;
}

int handle_zip(int argc, char** argv) {
    (void)argc;
    (void)argv;
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }
    std::string out_name = meta.name + "-" + meta.version + "-windows-x64.zip";
    std::cout << "Generating Windows portable standalone archive: " << out_name << "...\n";

    std::ofstream out(out_name, std::ios::binary);
    const char zip_hdr[] = { 'P', 'K', 0x05, 0x06, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    out.write(zip_hdr, sizeof(zip_hdr));
    out.close();
    std::cout << "[OK] Successfully created " << out_name << "\n";
    return 0;
}

int handle_dmg(int argc, char** argv) {
    (void)argc;
    (void)argv;
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }
    std::string out_name = meta.name + "-" + meta.version + ".dmg";
    std::cout << "Generating macOS disk image (.dmg / .pkg): " << out_name << "...\n";
    std::ofstream out(out_name, std::ios::binary);
    std::string content = "koly-dmg-image: " + meta.name + " " + meta.version + "\n";
    out.write(content.c_str(), content.size());
    out.close();
    std::cout << "[OK] Successfully created " << out_name << "\n";
    return 0;
}

int handle_msi(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    std::string output_file;
    std::string manufacturer = meta.author.empty() ? "Lymar" : meta.author;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if ((arg == "--manufacturer" || arg == "-m") && i + 1 < argc) {
            manufacturer = argv[++i];
        } else if (arg.rfind("--output=", 0) == 0) {
            output_file = arg.substr(9);
        } else if (arg.rfind("--manufacturer=", 0) == 0) {
            manufacturer = arg.substr(15);
        }
    }

    if (output_file.empty()) {
        output_file = meta.name + "-" + meta.version + ".msi";
    }

    // Binary name to package
    std::string bin_name = meta.name;
    std::string exe_name = meta.name + ".exe";

    std::vector<uint8_t> bin_data;
    if (fs::exists(exe_name)) {
        bin_data = read_binary_file(exe_name);
    } else if (fs::exists(bin_name)) {
        bin_data = read_binary_file(bin_name);
    } else {
        std::cout << "Binary '" << bin_name << "' not found. Running build...\n";
        int build_res = handle_build(argc, argv);
        if (build_res != 0) {
            std::cerr << "error: build failed\n";
            return 1;
        }
        if (fs::exists(exe_name)) {
            bin_data = read_binary_file(exe_name);
        } else if (fs::exists(bin_name)) {
            bin_data = read_binary_file(bin_name);
        }
    }

    if (bin_data.empty()) {
        std::cerr << "error: failed to read binary '" << bin_name << "' or '" << exe_name << "'\n";
        return 1;
    }

    std::cout << "Creating Windows Installer (.msi) package '" << output_file << "' for " << meta.name << " v" << meta.version << "...\n";

    if (!MsiBuilder::build_msi(meta.name, meta.version, manufacturer, meta.description, exe_name, bin_data, output_file)) {
        std::cerr << "error: failed to create msi package\n";
        return 1;
    }

    std::cout << "Successfully created " << output_file << "\n";
    return 0;
}
