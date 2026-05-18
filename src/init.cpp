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
    NOL::Builder builder;
    builder.set("package.name", NOL::Value(project_name));
    builder.set("package.version", NOL::Value("0.1.0"));
    builder.set("dependencies", NOL::Value(NOL::Object{}));

    std::ofstream out("lymar.nol");
    if (out.is_open()) {
        out << builder.build().dump(2);
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
