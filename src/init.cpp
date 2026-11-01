#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "nol.hpp"

namespace fs = std::filesystem;

int handle_init() {
    std::string project_name;
    // Default name is current directory name
    project_name = fs::current_path().filename().string();
    
    std::cout << "Initializing project '" << project_name << "'...\n";

    // Create lymar.nol
    NOL::Builder builder;
    NOL::Object pkg;
    pkg["name"] = project_name;
    pkg["version"] = "0.1.0";
    builder.set("package", pkg);
    builder.set("dependencies", NOL::Object{});

    std::ofstream config_file("lymar.nol");
    if (!config_file.is_open()) {
        std::cerr << "error: could not create lymar.nol\n";
        return 1;
    }
    config_file << "# Lymar project configuration\n\n";
    config_file << builder.build().dump();
    config_file.close();

    // Create src directory
    if (!fs::exists("src")) {
        fs::create_directory("src");
    }

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

int handle_new(int argc, char** argv) {
    std::string project_name = "my_project";
    std::string template_name = "cli"; // cli, web, wasm, lib, native

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--template=", 0) == 0) {
            template_name = arg.substr(11);
        } else if (arg == "-t" && i + 1 < argc) {
            template_name = argv[++i];
        } else if (arg[0] != '-') {
            project_name = arg;
        }
    }

    if (fs::exists(project_name)) {
        std::cerr << "error: destination directory '" << project_name << "' already exists\n";
        return 1;
    }

    std::cout << "Scaffolding new Lymar project '" << project_name << "' (template: " << template_name << ")...\n";
    fs::create_directories(project_name);
    fs::create_directories(fs::path(project_name) / "src");

    NOL::Builder builder;
    NOL::Object pkg;
    pkg["name"] = project_name;
    pkg["version"] = "0.1.0";
    pkg["license"] = "MIT";
    builder.set("package", pkg);

    NOL::Object deps;
    if (template_name == "web") {
        deps["@lymar/http"] = "^0.2.0";
        deps["@lymar/json"] = "^1.0.0";
    } else if (template_name == "wasm") {
        deps["@lymar/wasm-bindgen"] = "^0.1.0";
    }
    builder.set("dependencies", deps);

    if (template_name == "native") {
        NOL::Object nat;
        NOL::Array srcs;
        srcs.push_back(NOL::Value("native/binding.c"));
        nat["sources"] = srcs;
        builder.set("native", nat);

        fs::create_directories(fs::path(project_name) / "native");
        std::ofstream nat_f(fs::path(project_name) / "native" / "binding.c");
        nat_f << "#include <stdio.h>\n\nvoid native_hello() {\n    printf(\"Hello from native C!\\n\");\n}\n";
        nat_f.close();
    }

    std::ofstream config_file(fs::path(project_name) / "lymar.nol");
    config_file << "# Lymar project configuration\n\n" << builder.build().dump();
    config_file.close();

    std::ofstream main_f(fs::path(project_name) / "src" / (template_name == "lib" ? "lib.lm" : "main.lm"));
    if (template_name == "web") {
        main_f << "// Web API Starter\nfn main() {\n    print(\"Starting HTTP server on :8080...\");\n}\n";
    } else if (template_name == "wasm") {
        main_f << "// WebAssembly Starter\nfn export_calc(a: int, b: int): int {\n    return a + b;\n}\n";
    } else if (template_name == "lib") {
        main_f << "// Library Starter\nfn add(a: int, b: int): int {\n    return a + b;\n}\n";
    } else if (template_name == "native") {
        main_f << "// Native Binding Starter\nfn main() {\n    print(\"Calling native C bindings...\");\n}\n";
    } else {
        // CLI
        main_f << "// CLI Tool Starter\nfn main() {\n    print(\"Hello from " << project_name << " CLI!\");\n}\n";
    }
    main_f.close();

    std::cout << "[OK] Project '" << project_name << "' created successfully!\n";
    std::cout << "Next steps:\n";
    std::cout << "  cd " << project_name << "\n";
    std::cout << "  lyra run\n";
    return 0;
}
