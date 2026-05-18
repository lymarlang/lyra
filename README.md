# Lyra Package Manager
Lyra is the official package manager for the Lymar (Limitly) programming language.

## Nol and NOLE Files
Lyra uses two main files for package management:
- **Nol Files (`lymar.toml`):** These files contain package metadata, including name, version, and dependencies.
- **NOLE Files (`lymar.lock`):** These files are automatically generated and contain the exact versions and paths of all dependencies to ensure reproducible builds.

## API
Lyra provides a clean C++ API for interacting with Nol and NOLE files:
- `NolDocument`: A class for loading, manipulating, and saving Nol files.
- `NolWriter`: A class for serializing package metadata to the Nol format.
- `NoleDocument`: A class for managing and saving NOLE files.
- `NoleWriter`: A class for serializing resolved dependencies to the NOLE format.

## Building Lyra
Lyra is written in C++17 and can be built using its own Makefile:
```bash
make
```
The binary will be generated in `bin/lyra`.

## Usage
```bash
lyra init         # Initialize a new project
lyra add <pkg>    # Add a dependency
lyra update       # Update dependencies and lockfile
lyra run          # Run the project
lyra build        # Build the project
```
