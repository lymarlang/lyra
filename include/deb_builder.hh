#ifndef DEB_BUILDER_HH
#define DEB_BUILDER_HH

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <zlib.h>
#include <openssl/evp.h>

namespace fs = std::filesystem;

class DebBuilder {
public:
    struct FileEntry {
        std::string install_path; // e.g. "usr/bin/myprog"
        std::vector<uint8_t> data;
        uint32_t mode = 0755;
    };

    static std::string calculate_md5(const std::vector<uint8_t>& data) {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int md_len = 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (ctx) {
            EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
            EVP_DigestUpdate(ctx, data.data(), data.size());
            EVP_DigestFinal_ex(ctx, md, &md_len);
            EVP_MD_CTX_free(ctx);
        }
        std::ostringstream ss;
        for (unsigned int i = 0; i < md_len; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)md[i];
        }
        return ss.str();
    }

    // Creates a ustar tar archive in-memory
    static std::vector<uint8_t> create_tar(const std::vector<FileEntry>& entries) {
        std::vector<uint8_t> tar_data;

        for (const auto& entry : entries) {
            // Standard tar 512-byte header block
            uint8_t header[512];
            std::memset(header, 0, sizeof(header));

            // File name (100 bytes)
            std::strncpy(reinterpret_cast<char*>(header), entry.install_path.c_str(), 100);

            // File mode (8 bytes octal)
            snprintf(reinterpret_cast<char*>(header + 100), 8, "%07o", entry.mode);

            // UID & GID (8 bytes each octal)
            snprintf(reinterpret_cast<char*>(header + 108), 8, "%07o", 0);
            snprintf(reinterpret_cast<char*>(header + 116), 8, "%07o", 0);

            // File size (12 bytes octal)
            snprintf(reinterpret_cast<char*>(header + 124), 12, "%011zo", entry.data.size());

            // Modification time (12 bytes octal)
            snprintf(reinterpret_cast<char*>(header + 136), 12, "%011lo", static_cast<long>(1700000000));

            // Checksum placeholder (8 spaces)
            std::memset(header + 148, ' ', 8);

            // Typeflag ('0' = regular file)
            header[156] = '0';

            // Magic ("ustar\0") and version ("00")
            std::memcpy(header + 257, "ustar\0", 6);
            std::memcpy(header + 263, "00", 2);

            // Owner and group name
            std::strncpy(reinterpret_cast<char*>(header + 265), "root", 32);
            std::strncpy(reinterpret_cast<char*>(header + 297), "root", 32);

            // Calculate checksum
            unsigned int checksum = 0;
            for (size_t i = 0; i < 512; ++i) {
                checksum += header[i];
            }
            snprintf(reinterpret_cast<char*>(header + 148), 8, "%06o", checksum);

            // Append header to tar_data
            tar_data.insert(tar_data.end(), header, header + 512);

            // Append file data
            tar_data.insert(tar_data.end(), entry.data.begin(), entry.data.end());

            // Pad file data to 512 bytes
            size_t rem = entry.data.size() % 512;
            if (rem != 0) {
                tar_data.insert(tar_data.end(), 512 - rem, 0);
            }
        }

        // Two 512-byte zero blocks at the end of tar
        tar_data.insert(tar_data.end(), 1024, 0);

        return tar_data;
    }

    // In-process gzip compression using zlib
    static std::vector<uint8_t> gzip_compress(const std::vector<uint8_t>& input) {
        z_stream zs;
        std::memset(&zs, 0, sizeof(zs));

        // windowBits = 15 + 16 specifies gzip header and trailer format
        if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            throw std::runtime_error("zlib deflateInit2 failed");
        }

        zs.next_in = const_cast<Bytef*>(input.data());
        zs.avail_in = input.size();

        std::vector<uint8_t> output;
        uint8_t buffer[32768];

        int ret = Z_OK;
        while (ret == Z_OK) {
            zs.next_out = buffer;
            zs.avail_out = sizeof(buffer);

            ret = deflate(&zs, Z_FINISH);
            size_t produced = sizeof(buffer) - zs.avail_out;
            if (produced > 0) {
                output.insert(output.end(), buffer, buffer + produced);
            }
        }

        deflateEnd(&zs);

        if (ret != Z_STREAM_END) {
            throw std::runtime_error("zlib deflate failed to finish");
        }

        return output;
    }

    // Create an ar entry header (60 bytes)
    static std::vector<uint8_t> create_ar_header(const std::string& filename, size_t file_size) {
        std::vector<uint8_t> hdr(60, ' ');
        // File identifier (16 bytes, terminated by '/' in GNU ar format)
        std::string id = filename + "/";
        if (id.size() > 16) id = id.substr(0, 16);
        std::memcpy(hdr.data(), id.data(), id.size());

        // File modification timestamp (12 bytes)
        std::string mtime = "0";
        std::memcpy(hdr.data() + 16, mtime.data(), mtime.size());

        // Owner ID (6 bytes)
        std::string uid = "0";
        std::memcpy(hdr.data() + 28, uid.data(), uid.size());

        // Group ID (6 bytes)
        std::string gid = "0";
        std::memcpy(hdr.data() + 34, gid.data(), gid.size());

        // File mode (8 bytes octal)
        std::string mode = "100644";
        std::memcpy(hdr.data() + 40, mode.data(), mode.size());

        // File size in bytes (10 bytes decimal)
        std::string sz = std::to_string(file_size);
        std::memcpy(hdr.data() + 48, sz.data(), sz.size());

        // Ending characters `\n
        hdr[58] = '`';
        hdr[59] = '\n';

        return hdr;
    }

    // Build complete .deb file
    static bool build_deb(const std::string& package_name,
                          const std::string& version,
                          const std::string& description,
                          const std::string& author,
                          const std::string& arch,
                          const std::vector<FileEntry>& payload_files,
                          const std::string& output_deb_path) {
        // 1. debian-binary content: "2.0\n"
        std::string debian_binary = "2.0\n";
        std::vector<uint8_t> debian_binary_bytes(debian_binary.begin(), debian_binary.end());

        // 2. data.tar.gz
        std::vector<uint8_t> data_tar = create_tar(payload_files);
        std::vector<uint8_t> data_tar_gz = gzip_compress(data_tar);

        // Calculate installed size in KB
        size_t installed_size_kb = 0;
        for (const auto& f : payload_files) {
            installed_size_kb += (f.data.size() + 1023) / 1024;
        }

        // 3. control file content
        std::string maintainer = author.empty() ? "Lymar Developer <developer@lymar.lang>" : author;
        std::string desc = description.empty() ? (package_name + " application") : description;

        std::ostringstream ctrl;
        ctrl << "Package: " << package_name << "\n";
        ctrl << "Version: " << version << "\n";
        ctrl << "Section: utils\n";
        ctrl << "Priority: optional\n";
        ctrl << "Architecture: " << (arch.empty() ? "amd64" : arch) << "\n";
        ctrl << "Installed-Size: " << installed_size_kb << "\n";
        ctrl << "Maintainer: " << maintainer << "\n";
        ctrl << "Description: " << desc << "\n";

        std::string ctrl_str = ctrl.str();
        std::vector<uint8_t> ctrl_bytes(ctrl_str.begin(), ctrl_str.end());

        // md5sums content
        std::ostringstream md5s;
        for (const auto& f : payload_files) {
            md5s << calculate_md5(f.data) << "  " << f.install_path << "\n";
        }
        std::string md5s_str = md5s.str();
        std::vector<uint8_t> md5s_bytes(md5s_str.begin(), md5s_str.end());

        std::vector<FileEntry> control_entries = {
            {"./control", ctrl_bytes, 0644},
            {"./md5sums", md5s_bytes, 0644}
        };

        std::vector<uint8_t> control_tar = create_tar(control_entries);
        std::vector<uint8_t> control_tar_gz = gzip_compress(control_tar);

        // 4. Assemble ar archive:
        // "!<arch>\n"
        // debian-binary header + data
        // control.tar.gz header + data (padded to even 2-byte boundary if needed)
        // data.tar.gz header + data (padded to even 2-byte boundary if needed)
        std::vector<uint8_t> ar_data;
        const char ar_magic[] = "!<arch>\n";
        ar_data.insert(ar_data.end(), ar_magic, ar_magic + 8);

        auto append_ar_file = [&](const std::string& name, const std::vector<uint8_t>& data) {
            auto hdr = create_ar_header(name, data.size());
            ar_data.insert(ar_data.end(), hdr.begin(), hdr.end());
            ar_data.insert(ar_data.end(), data.begin(), data.end());
            if (data.size() % 2 != 0) {
                ar_data.push_back('\n'); // ar members are 2-byte aligned
            }
        };

        append_ar_file("debian-binary", debian_binary_bytes);
        append_ar_file("control.tar.gz", control_tar_gz);
        append_ar_file("data.tar.gz", data_tar_gz);

        // Write output file
        std::ofstream out(output_deb_path, std::ios::binary);
        if (!out.is_open()) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(ar_data.data()), ar_data.size());
        out.close();

        return true;
    }
};

#endif
