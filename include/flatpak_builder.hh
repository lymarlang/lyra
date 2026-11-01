#ifndef FLATPAK_BUILDER_HH
#define FLATPAK_BUILDER_HH

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdint>
#include "tar_helper.hh"

class FlatpakBuilder {
public:
    // Builds an in-process Flatpak application bundle archive containing standard Flatpak metadata, desktop entry, and bin files
    static bool build_flatpak(const std::string& app_name,
                              const std::string& version,
                              const std::string& description,
                              const std::vector<uint8_t>& binary_payload,
                              const std::string& output_path) {
        (void)version;
        try {
            std::string app_id = "org.lymar." + app_name;

            // Flatpak metadata file
            std::string metadata = "[Application]\n"
                                   "name=" + app_id + "\n"
                                   "runtime=org.freedesktop.Platform/x86_64/23.08\n"
                                   "sdk=org.freedesktop.Sdk/x86_64/23.08\n"
                                   "command=" + app_name + "\n\n"
                                   "[Context]\n"
                                   "shared=network;ipc;\n"
                                   "sockets=x11;wayland;pulseaudio;\n"
                                   "devices=dri;\n"
                                   "filesystems=host;\n";

            std::string desktop_entry = "[Desktop Entry]\n"
                                        "Type=Application\n"
                                        "Name=" + app_name + "\n"
                                        "Exec=" + app_name + "\n"
                                        "Comment=" + (description.empty() ? (app_name + " application") : description) + "\n"
                                        "Categories=Development;IDE;\n";

            std::vector<TarHelper::TarFileEntry> entries;
            entries.push_back({ "metadata", std::vector<uint8_t>(metadata.begin(), metadata.end()), 0644, false });
            entries.push_back({ "export/share/applications/" + app_id + ".desktop", std::vector<uint8_t>(desktop_entry.begin(), desktop_entry.end()), 0644, false });
            entries.push_back({ "files/bin/" + app_name, binary_payload, 0755, false });

            std::vector<uint8_t> tar_data = TarHelper::create_tar(entries);
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
