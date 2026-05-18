#ifndef NOL_HH
#define NOL_HH

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

struct Dependency {
    std::string name;
    std::string version;
    std::string path;
    std::string git;
    std::string tag;
    std::vector<std::string> features;
};

struct PackageMetadata {
    std::string name;
    std::string version;
    std::map<std::string, Dependency> dependencies;
    std::map<std::string, std::vector<std::string>> features;
    std::vector<std::string> enabled_features;
};

class NolWriter {
public:
    static std::string serialize(const PackageMetadata& meta) {
        std::stringstream ss;
        ss << "[package]\n";
        ss << "name = \"" << meta.name << "\"\n";
        ss << "version = \"" << meta.version << "\"\n\n";

        if (!meta.dependencies.empty()) {
            ss << "[dependencies]\n";
            for (auto const& [name, dep] : meta.dependencies) {
                if (!dep.path.empty() || !dep.git.empty() || !dep.features.empty()) {
                    ss << name << " = { ";
                    bool first = true;
                    if (!dep.version.empty()) {
                        ss << "version = \"" << dep.version << "\"";
                        first = false;
                    }
                    if (!dep.path.empty()) {
                        if (!first) ss << ", ";
                        ss << "path = \"" << dep.path << "\"";
                        first = false;
                    }
                    if (!dep.git.empty()) {
                        if (!first) ss << ", ";
                        ss << "git = \"" << dep.git << "\"";
                        if (!dep.tag.empty()) ss << ", tag = \"" << dep.tag << "\"";
                        first = false;
                    }
                    if (!dep.features.empty()) {
                        if (!first) ss << ", ";
                        ss << "features = [";
                        for (size_t i = 0; i < dep.features.size(); ++i) {
                            ss << "\"" << dep.features[i] << "\"";
                            if (i < dep.features.size() - 1) ss << ", ";
                        }
                        ss << "]";
                    }
                    ss << " }\n";
                } else {
                    ss << name << " = \"" << dep.version << "\"\n";
                }
            }
            ss << "\n";
        }

        if (!meta.features.empty()) {
            ss << "[features]\n";
            for (auto const& [name, deps] : meta.features) {
                ss << name << " = [";
                for (size_t i = 0; i < deps.size(); ++i) {
                    ss << "\"" << deps[i] << "\"";
                    if (i < deps.size() - 1) ss << ", ";
                }
                ss << "]\n";
            }
        }

        return ss.str();
    }
};

class NolDocument {
public:
    NolDocument() = default;

    bool load(const std::string& filename) {
        (void)filename;
        return true;
    }

    void save(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << NolWriter::serialize(metadata);
            file.close();
        }
    }

    PackageMetadata& get_metadata() { return metadata; }
    const PackageMetadata& get_metadata() const { return metadata; }

    void set_name(const std::string& name) { metadata.name = name; }
    void set_version(const std::string& version) { metadata.version = version; }

    void add_dependency(const std::string& name, const std::string& version) {
        Dependency dep;
        dep.name = name;
        dep.version = version;
        metadata.dependencies[name] = dep;
    }

private:
    PackageMetadata metadata;
};

#endif
