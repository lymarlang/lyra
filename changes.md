# Lyra Project Changes

This document tracks the changes made during the development of Lyra, the Lymar package manager.

## 2024-03-22
- **New Project:** Created the `lyra/` directory with a standalone C++ project structure.
- **CLI Dispatcher:** Implemented a basic CLI in `lyra/src/main.cpp` supporting core commands.
- **Project Initialization:** Implemented `lyra init` to bootstrap new projects with `lymar.toml`.
- **Dependency Resolution:**
    - Developed a recursive dependency resolver in `lyra/include/resolver.hh`.
    - Added support for path-based and Git-based dependencies.
    - Implemented circular dependency detection.
- **Version Management:**
    - Built a Semver parser and matching system in `lyra/include/semver.hh`.
    - Integrated version validation into the resolver.
- **Lockfile System:**
    - Created `lymar.lock` generation to ensure deterministic builds.
    - Configured `run`, `build`, and `update` to automatically keep the lockfile in sync.
- **Git Integration:**
    - Implemented `FetchSystem` for cloning remote dependencies.
    - Added a local cache system in `~/.lyra/cache` to speed up subsequent builds.
- **Compiler Integration:**
    - Patched `src/main.cpp` in the `limitly` compiler to support include paths (`-I`).
    - Implemented a source-injection mechanism for `import` statements in the compiler.
    - Renamed all "luminar" occurrences to "lymar".
- **Bug Fixes:**
    - Resolved `libgccjit` compilation issues in the main compiler.
    - Fixed string literal path parsing in the compiler's `import` handler.
