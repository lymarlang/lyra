# Lyra Development Agent Notes

## Current Progress
Implemented clean APIs (`NolDocument`, `NolWriter`, `NoleDocument`, `NoleWriter`) for managing package configuration and lockfiles.

### Implemented Systems
1.  **CLI System (Phase 0):** Dispatcher for `init`, `run`, `build`, `update`, and `help`.
2.  **Project System (Phase 1):** `lyra init` generates `lymar.toml` (Nol file).
3.  **Execution System (Phase 2):** `lyra run` invokes `limitly` with resolved dependency include paths.
4.  **Build System (Phase 3):** `lyra build` invokes `limitly -jit` for AOT-style compilation.
5.  **Local Dependency System (Phase 4):** Path-based dependencies are resolved recursively.
6.  **Lock System (Phase 5):** `lymar.lock` (NOLE file) is generated and updated.
7.  **Version System (Phase 6):** Semver parser supporting `^`, `~`, and `>=`.
8.  **Fetch System (Phase 7):** Git dependency cloning and local caching.

## Building Lyra
```bash
make
```
The binary is at `bin/lyra`.

## Nol and NOLE Files
- **Nol (`lymar.toml`)**: Clean API via `NolDocument` in `include/nol.hh`.
- **NOLE (`lymar.lock`)**: Clean API via `NoleDocument` in `include/nole.hh`.

## Key Files
- `include/nol.hh`: Nol file API.
- `include/nole.hh`: NOLE file API.
- `include/config_parser.hh`: Parser for Nol files.
- `include/lock_system.hh`: System for managing NOLE files.
- `include/resolver.hh`: Core dependency resolution logic.
- `include/semver.hh`: Semantic versioning logic.
- `include/fetch_system.hh`: Git cloning and caching logic.
- `include/process_helper.hh`: Platform-agnostic process execution.
