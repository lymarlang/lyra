#ifndef LYRA_API_HH
#define LYRA_API_HH

#include <string>
#include <vector>
#include "handlers.hh"

namespace Lyra {

// In-process package dependency resolution API
std::vector<std::string> resolve_package_include_dirs(const std::string& project_dir = ".",
                                                       const std::vector<std::string>& enabled_features = {});

// Lyra command handlers for CLI forwarding
inline int handle_init() { return ::handle_init(); }
inline int handle_run(int argc, char** argv) { return ::handle_run(argc, argv); }
inline int handle_build(int argc, char** argv) { return ::handle_build(argc, argv); }
inline int handle_update(int argc, char** argv) { return ::handle_update(argc, argv); }
inline int handle_add(int argc, char** argv) { return ::handle_add(argc, argv); }
inline int handle_link(int argc, char** argv) { return ::handle_link(argc, argv); }
inline int handle_publish(int argc, char** argv) { return ::handle_publish(argc, argv); }
inline int handle_pack(int argc, char** argv) { return ::handle_pack(argc, argv); }
inline int handle_install(int argc, char** argv) { return ::handle_install(argc, argv); }
inline int handle_search(int argc, char** argv) { return ::handle_search(argc, argv); }
inline int handle_info(int argc, char** argv) { return ::handle_info(argc, argv); }
inline int handle_test(int argc, char** argv) { return ::handle_test(argc, argv); }
inline int handle_deps(int argc, char** argv) { return ::handle_deps(argc, argv); }
inline int handle_tree(int argc, char** argv) { return ::handle_tree(argc, argv); }
inline int handle_why(int argc, char** argv) { return ::handle_why(argc, argv); }
inline int handle_clean() { return ::handle_clean(); }
inline int handle_cache_clear() { return ::handle_cache_clear(); }
inline int handle_doctor() { return ::handle_doctor(); }
inline int handle_env() { return ::handle_env(); }
inline int handle_audit(int argc, char** argv) { return ::handle_audit(argc, argv); }
inline int handle_bench(int argc, char** argv) { return ::handle_bench(argc, argv); }

} // namespace Lyra

#endif
