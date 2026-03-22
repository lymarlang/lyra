# Lyra Development Agent Notes

## Current Progress
Phases 0 through 7 of the Lyra specification have been implemented.

### Implemented Systems
1.  **CLI System (Phase 0):** Dispatcher for `init`, `run`, `build`, `update`, and `help`.
2.  **Project System (Phase 1):** `lyra init` generates `lymar.toml` and `src/main.lm`. Basic TOML parser implemented.
3.  **Execution System (Phase 2):** `lyra run` invokes `limitly` with resolved dependency include paths.
4.  **Build System (Phase 3):** `lyra build` invokes `limitly -jit` for AOT-style compilation.
5.  **Local Dependency System (Phase 4):** Path-based dependencies are resolved recursively with circular dependency detection.
6.  **Lock System (Phase 5):** `lymar.lock` is generated and updated during `run`, `build`, and `update`.
7.  **Version System (Phase 6):** Semver parser supporting `^`, `~`, and `>=`.
8.  **Fetch System (Phase 7):** Git dependency cloning and local caching in `~/.lyra/cache`.

## Building Lyra
Lyra is written in C++17 and can be built using its own Makefile:
```bash
cd lyra
make
```
The binary will be generated in `lyra/bin/lyra`.

## Integration with Limitly
Lyra relies on the `limitly` compiler. It expects the compiler to be available at `../bin/limitly` relative to the project root, or through the `REPOROOT` environment variable.

### Compiler Enhancements
To support Lyra, the `limitly` compiler was updated to:
- Accept `-I` flags for include directories.
- Support a basic `import` mechanism that injects source code into the AST before type checking.
- Strip quotes from string literal import paths.

## Key Files
- `lyra/include/config_parser.hh`: Robust parser for `lymar.toml`.
- `lyra/include/resolver.hh`: Core dependency resolution logic.
- `lyra/include/lock_system.hh`: Lockfile generation and reading.
- `lyra/include/semver.hh`: Semantic versioning logic.
- `lyra/include/fetch_system.hh`: Git cloning and caching logic.
- `lyra/include/process_helper.hh`: Platform-agnostic process execution.
