#include "handlers.hh"
#include "nol.hh"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

int handle_init() {
    std::string project_name;
    project_name = fs::current_path().filename().string();
    
    std::cout << "Initializing project '" << project_name << "'...\n";

    NolDocument doc;
    doc.set_name(project_name);
    doc.set_version("0.1.0");
    doc.save("lymar.toml");

    fs::create_directory("src");

    std::ofstream main_file("src/main.lm");
    if (!main_file.is_open()) {
        std::cerr << "error: could not create src/main.lm\n";
        return 1;
    }
    main_file << "// Main entry point for " << project_name << "\n";
    main_file << "print(\"Hello, Lymar!\");\n";
    main_file.close();

    std::cout << "Project initialized successfully.\n";
    return 0;
}
