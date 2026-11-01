#ifndef TAR_HELPER_HH
#define TAR_HELPER_HH

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <zlib.h>

namespace fs = std::filesystem;

class TarHelper {
public:
    struct TarFileEntry {
        std::string filename; // relative path within archive
        std::vector<uint8_t> data;
        uint32_t mode = 0644;
        bool is_dir = false;
    };

    static std::vector<uint8_t> gzip_compress(const std::vector<uint8_t>& input) {
        z_stream zs;
        std::memset(&zs, 0, sizeof(zs));

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

    static std::vector<uint8_t> gzip_decompress(const std::vector<uint8_t>& input) {
        z_stream zs;
        std::memset(&zs, 0, sizeof(zs));

        if (inflateInit2(&zs, 15 + 32) != Z_OK) {
            throw std::runtime_error("zlib inflateInit2 failed");
        }

        zs.next_in = const_cast<Bytef*>(input.data());
        zs.avail_in = input.size();

        std::vector<uint8_t> output;
        uint8_t buffer[32768];

        int ret = Z_OK;
        while (ret == Z_OK) {
            zs.next_out = buffer;
            zs.avail_out = sizeof(buffer);

            ret = inflate(&zs, Z_NO_FLUSH);
            size_t produced = sizeof(buffer) - zs.avail_out;
            if (produced > 0) {
                output.insert(output.end(), buffer, buffer + produced);
            }
        }

        inflateEnd(&zs);

        if (ret != Z_STREAM_END && ret != Z_OK) {
            throw std::runtime_error("zlib inflate failed: " + std::to_string(ret));
        }

        return output;
    }

    static std::vector<uint8_t> create_tar(const std::vector<TarFileEntry>& entries) {
        std::vector<uint8_t> tar_data;

        for (const auto& entry : entries) {
            uint8_t header[512];
            std::memset(header, 0, sizeof(header));

            std::string path = entry.filename;
            if (entry.is_dir && !path.empty() && path.back() != '/') {
                path += '/';
            }

            std::strncpy(reinterpret_cast<char*>(header), path.c_str(), 100);
            snprintf(reinterpret_cast<char*>(header + 100), 8, "%07o", entry.mode);
            snprintf(reinterpret_cast<char*>(header + 108), 8, "%07o", 0);
            snprintf(reinterpret_cast<char*>(header + 116), 8, "%07o", 0);
            size_t file_size = entry.is_dir ? 0 : entry.data.size();
            snprintf(reinterpret_cast<char*>(header + 124), 12, "%011zo", file_size);
            snprintf(reinterpret_cast<char*>(header + 136), 12, "%011lo", static_cast<long>(1700000000));
            std::memset(header + 148, ' ', 8);
            header[156] = entry.is_dir ? '5' : '0';
            std::memcpy(header + 257, "ustar\0", 6);
            std::memcpy(header + 263, "00", 2);
            std::strncpy(reinterpret_cast<char*>(header + 265), "root", 32);
            std::strncpy(reinterpret_cast<char*>(header + 297), "root", 32);

            unsigned int checksum = 0;
            for (size_t i = 0; i < 512; ++i) {
                checksum += header[i];
            }
            snprintf(reinterpret_cast<char*>(header + 148), 8, "%06o", checksum);

            tar_data.insert(tar_data.end(), header, header + 512);

            if (!entry.is_dir && !entry.data.empty()) {
                tar_data.insert(tar_data.end(), entry.data.begin(), entry.data.end());
                size_t rem = entry.data.size() % 512;
                if (rem != 0) {
                    tar_data.insert(tar_data.end(), 512 - rem, 0);
                }
            }
        }

        tar_data.insert(tar_data.end(), 1024, 0);
        return tar_data;
    }

    static std::vector<TarFileEntry> parse_tar(const std::vector<uint8_t>& tar_data) {
        std::vector<TarFileEntry> entries;
        size_t offset = 0;

        while (offset + 512 <= tar_data.size()) {
            const uint8_t* header = tar_data.data() + offset;

            bool all_zero = true;
            for (size_t i = 0; i < 512; ++i) {
                if (header[i] != 0) {
                    all_zero = false;
                    break;
                }
            }
            if (all_zero) break;

            char name_buf[101];
            std::memcpy(name_buf, header, 100);
            name_buf[100] = '\0';
            std::string name(name_buf);

            char size_buf[13];
            std::memcpy(size_buf, header + 124, 12);
            size_buf[12] = '\0';
            size_t file_size = 0;
            try {
                file_size = std::strtoull(size_buf, nullptr, 8);
            } catch (...) {
                file_size = 0;
            }

            char mode_buf[9];
            std::memcpy(mode_buf, header + 100, 8);
            mode_buf[8] = '\0';
            uint32_t mode = 0644;
            try {
                mode = std::strtoul(mode_buf, nullptr, 8);
            } catch (...) {}

            char typeflag = header[156];
            bool is_dir = (typeflag == '5' || (!name.empty() && name.back() == '/'));

            offset += 512;

            TarFileEntry entry;
            entry.filename = name;
            entry.mode = mode;
            entry.is_dir = is_dir;

            if (!is_dir && file_size > 0) {
                if (offset + file_size > tar_data.size()) {
                    throw std::runtime_error("Corrupted tar archive: truncated file data");
                }
                entry.data.assign(tar_data.data() + offset, tar_data.data() + offset + file_size);
                size_t padded_size = ((file_size + 511) / 512) * 512;
                offset += padded_size;
            }

            entries.push_back(entry);
        }

        return entries;
    }

    static bool pack_to_tar_gz(const std::vector<std::string>& source_paths, const std::string& output_tar_gz) {
        try {
            std::vector<TarFileEntry> entries;

            for (const auto& sp : source_paths) {
                fs::path p(sp);
                if (!fs::exists(p)) continue;

                if (fs::is_directory(p)) {
                    for (const auto& item : fs::recursive_directory_iterator(p)) {
                        TarFileEntry e;
                        e.filename = fs::relative(item.path(), fs::current_path()).generic_string();
                        if (item.is_directory()) {
                            e.is_dir = true;
                            e.mode = 0755;
                        } else {
                            e.is_dir = false;
                            e.mode = 0644;
                            std::ifstream f(item.path(), std::ios::binary);
                            if (f.is_open()) {
                                e.data.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                            }
                        }
                        entries.push_back(e);
                    }
                } else if (fs::is_regular_file(p)) {
                    TarFileEntry e;
                    e.filename = p.filename().generic_string();
                    e.is_dir = false;
                    e.mode = 0644;
                    std::ifstream f(p, std::ios::binary);
                    if (f.is_open()) {
                        e.data.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    }
                    entries.push_back(e);
                }
            }

            std::vector<uint8_t> tar_bytes = create_tar(entries);
            std::vector<uint8_t> gz_bytes = gzip_compress(tar_bytes);

            std::ofstream out(output_tar_gz, std::ios::binary);
            if (!out.is_open()) return false;
            out.write(reinterpret_cast<const char*>(gz_bytes.data()), gz_bytes.size());
            out.close();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "error packing tar.gz: " << e.what() << "\n";
            return false;
        }
    }

    static bool extract_tar_gz(const std::string& tar_gz_path, const std::string& dest_dir) {
        try {
            std::ifstream in(tar_gz_path, std::ios::binary);
            if (!in.is_open()) return false;
            std::vector<uint8_t> file_bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();

            if (file_bytes.empty()) return false;

            std::vector<uint8_t> tar_bytes;
            if (file_bytes.size() >= 2 && file_bytes[0] == 0x1f && file_bytes[1] == 0x8b) {
                tar_bytes = gzip_decompress(file_bytes);
            } else {
                tar_bytes = file_bytes;
            }

            auto entries = parse_tar(tar_bytes);
            fs::path target_base(dest_dir);
            fs::create_directories(target_base);

            for (const auto& e : entries) {
                fs::path out_path = target_base / e.filename;
                fs::path norm_out = fs::weakly_canonical(out_path);
                fs::path norm_base = fs::weakly_canonical(target_base);
                if (norm_out.string().find(norm_base.string()) != 0) {
                    continue;
                }

                if (e.is_dir) {
                    fs::create_directories(out_path);
                } else {
                    fs::create_directories(out_path.parent_path());
                    std::ofstream out_file(out_path, std::ios::binary);
                    if (out_file.is_open()) {
                        out_file.write(reinterpret_cast<const char*>(e.data.data()), e.data.size());
                        out_file.close();
                    }
                }
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "error extracting tar.gz: " << e.what() << "\n";
            return false;
        }
    }
};

#endif
