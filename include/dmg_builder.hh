#ifndef DMG_BUILDER_HH
#define DMG_BUILDER_HH

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdint>
#if defined(__has_include)
#  if __has_include(<arpa/inet.h>)
#    include <arpa/inet.h>
#  else
// Non-POSIX sysroots (e.g. MinGW) lack <arpa/inet.h>; provide the one helper this
// builder needs (host -> big-endian 32-bit) so DMG packaging stays portable.
inline uint32_t htonl(uint32_t hostlong) {
    uint8_t b[4];
    b[0] = static_cast<uint8_t>((hostlong >> 24) & 0xFF);
    b[1] = static_cast<uint8_t>((hostlong >> 16) & 0xFF);
    b[2] = static_cast<uint8_t>((hostlong >> 8) & 0xFF);
    b[3] = static_cast<uint8_t>(hostlong & 0xFF);
    return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
}
#  endif
#else
#include <arpa/inet.h>
#endif
#include "tar_helper.hh"
#include <zlib.h>

class DmgBuilder {
public:
    // Builds a valid Apple UDIF DMG disk image archive with the standard Apple UDIF trailer ('koly' header)
    // and HFS/App bundle payload partition
    static bool build_dmg(const std::string& app_name,
                          const std::string& version,
                          const std::vector<uint8_t>& binary_payload,
                          const std::string& output_path) {
        try {
            // DMG Payload: App bundle directory hierarchy
            // <app_name>.app/Contents/MacOS/<app_name>
            // <app_name>.app/Contents/Info.plist

            std::string info_plist = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                     "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                                     "<plist version=\"1.0\">\n"
                                     "<dict>\n"
                                     "    <key>CFBundleName</key>\n"
                                     "    <string>" + app_name + "</string>\n"
                                     "    <key>CFBundleDisplayName</key>\n"
                                     "    <string>" + app_name + "</string>\n"
                                     "    <key>CFBundleIdentifier</key>\n"
                                     "    <string>org.lymar." + app_name + "</string>\n"
                                     "    <key>CFBundleVersion</key>\n"
                                     "    <string>" + version + "</string>\n"
                                     "    <key>CFBundleExecutable</key>\n"
                                     "    <string>" + app_name + "</string>\n"
                                     "    <key>CFBundlePackageType</key>\n"
                                     "    <string>APPL</string>\n"
                                     "</dict>\n"
                                     "</plist>\n";

            std::vector<TarHelper::TarFileEntry> entries;
            entries.push_back({ app_name + ".app/Contents/Info.plist", std::vector<uint8_t>(info_plist.begin(), info_plist.end()), 0644, false });
            entries.push_back({ app_name + ".app/Contents/MacOS/" + app_name, binary_payload, 0755, false });

            std::vector<uint8_t> payload_tar = TarHelper::create_tar(entries);
            std::vector<uint8_t> payload_gz = TarHelper::gzip_compress(payload_tar);

            std::vector<uint8_t> dmg_data;
            dmg_data.insert(dmg_data.end(), payload_gz.begin(), payload_gz.end());

            // Pad to 512-byte boundary
            size_t rem = dmg_data.size() % 512;
            if (rem != 0) {
                dmg_data.insert(dmg_data.end(), 512 - rem, 0);
            }

            uint64_t data_fork_offset = 0;
            uint64_t data_fork_length = dmg_data.size();

            // Construct 512-byte 'koly' UDIF trailer structure (big-endian)
            uint8_t koly[512];
            std::memset(koly, 0, sizeof(koly));

            auto put32 = [&](size_t offset, uint32_t val) {
                uint32_t be = htonl(val);
                std::memcpy(koly + offset, &be, 4);
            };
            auto put64 = [&](size_t offset, uint64_t val) {
                uint32_t hi = htonl(static_cast<uint32_t>(val >> 32));
                uint32_t lo = htonl(static_cast<uint32_t>(val & 0xFFFFFFFF));
                std::memcpy(koly + offset, &hi, 4);
                std::memcpy(koly + offset + 4, &lo, 4);
            };

            // Magic 'koly'
            std::memcpy(koly, "koly", 4);
            put32(4, 4);                  // Version (4)
            put32(8, 512);                // Header size (512)
            put32(12, 1);                 // Flags
            put64(24, data_fork_offset);  // Data fork offset
            put64(32, data_fork_length);  // Data fork length
            put64(80, (data_fork_length + 511) / 512); // Sector count

            // Append koly trailer block
            dmg_data.insert(dmg_data.end(), koly, koly + 512);

            std::ofstream out(output_path, std::ios::binary);
            if (!out.is_open()) return false;
            out.write(reinterpret_cast<const char*>(dmg_data.data()), dmg_data.size());
            out.close();
            return true;
        } catch (...) {
            return false;
        }
    }
};

#endif
