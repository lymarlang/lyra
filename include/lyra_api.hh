#ifndef LYRA_API_HH
#define LYRA_API_HH

#include <string>
#include <vector>
#include "handlers.hh"

namespace Lyra {

// In-process package dependency resolution API
std::vector<std::string> resolve_package_include_dirs(const std::string& project_dir = ".",
                                                       const std::vector<std::string>& enabled_features = {});

// Project lifecycle & creation
inline int handle_init() { return ::handle_init(); }
inline int handle_new(int argc, char** argv) { return ::handle_new(argc, argv); }
inline int handle_repl(int argc, char** argv) { return ::handle_repl(argc, argv); }
inline int handle_expand(int argc, char** argv) { return ::handle_expand(argc, argv); }

// Execution & Compilation
inline int handle_run(int argc, char** argv) { return ::handle_run(argc, argv); }
inline int handle_build(int argc, char** argv) { return ::handle_build(argc, argv); }

// Dependency management
inline int handle_update(int argc, char** argv) { return ::handle_update(argc, argv); }
inline int handle_add(int argc, char** argv) { return ::handle_add(argc, argv); }
inline int handle_link(int argc, char** argv) { return ::handle_link(argc, argv); }
inline int handle_deps(int argc, char** argv) { return ::handle_deps(argc, argv); }
inline int handle_tree(int argc, char** argv) { return ::handle_tree(argc, argv); }
inline int handle_why(int argc, char** argv) { return ::handle_why(argc, argv); }
inline int handle_vendor(int argc, char** argv) { return ::handle_vendor(argc, argv); }

// Registry & Publishing
inline int handle_publish(int argc, char** argv) { return ::handle_publish(argc, argv); }
inline int handle_pack(int argc, char** argv) { return ::handle_pack(argc, argv); }
inline int handle_install(int argc, char** argv) { return ::handle_install(argc, argv); }
inline int handle_search(int argc, char** argv) { return ::handle_search(argc, argv); }
inline int handle_info(int argc, char** argv) { return ::handle_info(argc, argv); }

// Packaging & Installers
inline int handle_deb(int argc, char** argv) { return ::handle_deb(argc, argv); }
inline int handle_msi(int argc, char** argv) { return ::handle_msi(argc, argv); }
inline int handle_appimage(int argc, char** argv) { return ::handle_appimage(argc, argv); }
inline int handle_snap(int argc, char** argv) { return ::handle_snap(argc, argv); }
inline int handle_flatpak(int argc, char** argv) { return ::handle_flatpak(argc, argv); }
inline int handle_zip(int argc, char** argv) { return ::handle_zip(argc, argv); }
inline int handle_dmg(int argc, char** argv) { return ::handle_dmg(argc, argv); }

// Cryptography & Signing
inline int handle_sign(int argc, char** argv) { return ::handle_sign(argc, argv); }
inline int handle_verify(int argc, char** argv) { return ::handle_verify(argc, argv); }
inline int handle_keygen(int argc, char** argv) { return ::handle_keygen(argc, argv); }

// Security, Compliance & Auditing
inline int handle_audit(int argc, char** argv) { return ::handle_audit(argc, argv); }
inline int handle_license(int argc, char** argv) { return ::handle_license(argc, argv); }
inline int handle_sbom(int argc, char** argv) { return ::handle_sbom(argc, argv); }

// Maintenance & Environment
inline int handle_clean() { return ::handle_clean(); }
inline int handle_cache_clear() { return ::handle_cache_clear(); }
inline int handle_doctor() { return ::handle_doctor(); }
inline int handle_env() { return ::handle_env(); }
inline int handle_toolchain(int argc, char** argv) { return ::handle_toolchain(argc, argv); }

// Tooling & Quality
inline int handle_test(int argc, char** argv) { return ::handle_test(argc, argv); }
inline int handle_bench(int argc, char** argv) { return ::handle_bench(argc, argv); }
inline int handle_profile(int argc, char** argv) { return ::handle_profile(argc, argv); }
inline int handle_lint(int argc, char** argv) { return ::handle_lint(argc, argv); }
inline int handle_doc(int argc, char** argv) { return ::handle_doc(argc, argv); }
inline int handle_fmt(int argc, char** argv) { return ::handle_fmt(argc, argv); }
inline int handle_watch(int argc, char** argv) { return ::handle_watch(argc, argv); }
inline int handle_lsp(int argc, char** argv) { return ::handle_lsp(argc, argv); }

} // namespace Lyra

#endif
