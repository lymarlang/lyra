#ifndef SNAP_BUILDER_HH
#define SNAP_BUILDER_HH

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdint>
#include "tar_helper.hh"

class SnapBuilder {
public:
    // Builds an in-process Snap package container containing meta/snap.yaml, command launcher, and bin payload
    static bool build_snap(const std::string& app_name,
                           const std::string& version,
                           const std::string& description,
                           const std::vector<uint8_t>& binary_payload,
                           const std::string& output_path) {
        try {
            std::string snap_yaml = "name: " + app_name + "\n"
                                    "version: '" + version + "'\n"
                                    "summary: " + (description.empty() ? app_name : description) + "\n"
                                    "description: |\n  " + (description.empty() ? (app_name + " application") : description) + "\n"
                                    "grade: stable\n"
                                    "confinement: classic\n"
                                    "base: core22\n\n"
                                    "apps:\n"
                                    "  " + app_name + ":\n"
                                    "    command: bin/" + app_name + "\n";

            std::vector<TarHelper::TarFileEntry> entries;
            entries.push_back({ "meta/snap.yaml", std::vector<uint8_t>(snap_yaml.begin(), snap_yaml.end()), 0644, false });
            entries.push_back({ "bin/" + app_name, binary_payload, 0755, false });

            std::vector<TarHelper::TarFileEntry> dir_entries;
            dir_entries.push_back({ "meta", {}, 0755, true });
            dir_entries.push_back({ "bin", {}, 0755, true });
            dir_entries.insert(dir_entries.end(), entries.begin(), entries.end());

            std::vector<uint8_t> tar_data = TarHelper::create_tar(dir_entries);
            std::vector<uint8_t> gz_data = TarHelper::gzip_compress(tar_data);

            std::ofstream out(output_path, std::ios::binary);
            if (!out.is_open()) return false;
            out.write(reinterpret_cast<const char*>(gz_data.data()), gz_data.size());
            out.close();
            return true;
        } catch (...) {
            return false;
        }
    }
};

#endif
