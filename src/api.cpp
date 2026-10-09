#include "lyra_api.hh"
#include "config_parser.hh"
#include "resolver.hh"
#include "lock_system.hh"
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace Lyra {

std::vector<std::string> resolve_package_include_dirs(const std::string& project_dir,
                                                       const std::vector<std::string>& enabled_features) {
    std::vector<std::string> include_dirs;
    fs::path dir(project_dir);
    if (!fs::exists(dir / "lymar.nol")) {
        return include_dirs;
    }

        PackageMetadata meta = ConfigParser::parse((dir / "lymar.nol").string());
        if (meta.name.empty()) {
            throw std::runtime_error("Invalid package manifest: " + (dir / "lymar.nol").string());
        }

        std::vector<ResolvedDependency> deps = DependencyResolver::resolve(project_dir, enabled_features);
        LockSystem::generate_lockfile(project_dir, deps);

        for (const auto& dep : deps) {
            if (!fs::is_regular_file(fs::path(dep.path) / "lymar.nol"))
                throw std::runtime_error("Missing dependency manifest: " + dep.path);
            if (fs::exists(dep.path)) {
                include_dirs.push_back(dep.path);
                fs::path src_path = fs::path(dep.path) / "src";
                if (fs::exists(src_path)) {
                    include_dirs.push_back(src_path.string());
                }
            }
        }

    return include_dirs;
}

} // namespace Lyra
