#include "handlers.hh"
#include "nol.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

int handle_init() {
    std::string project_name;
    // Default name is current directory name
    project_name = fs::current_path().filename().string();
    
    std::cout << "Initializing project '" << project_name << "'...\n";

    // Create lymar.nol
    NOL::Value root(NOL::Object{});
    NOL::Object pkg;
    pkg["name"] = project_name;
    pkg["version"] = "0.1.0";
    root["package"] = NOL::Value(pkg);
    root["dependencies"] = NOL::Value(NOL::Object{});

    std::ofstream out("lymar.nol");
    if (out.is_open()) {
        out << root.dump(2, 0, true);
        out.close();
    } else {
        std::cerr << "error: could not create lymar.nol\n";
        return 1;
    }

    // Create src directory
    fs::create_directory("src");

    // Create src/main.lm
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
