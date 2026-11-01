#ifndef ZIP_BUILDER_HH
#define ZIP_BUILDER_HH

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <zlib.h>

class ZipBuilder {
public:
    struct ZipEntry {
        std::string filename;
        std::vector<uint8_t> data;
    };

    // Raw deflate compression using zlib (windowBits = -15 for raw deflate without zlib/gzip headers)
    static std::vector<uint8_t> deflate_raw(const std::vector<uint8_t>& input) {
        if (input.empty()) return {};

        z_stream zs;
        std::memset(&zs, 0, sizeof(zs));
        if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            return input;
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
            return input;
        }
        return output;
    }

    static bool build_zip(const std::vector<ZipEntry>& entries, const std::string& output_path) {
        std::vector<uint8_t> zip_bytes;

        auto w16 = [&](uint16_t v) {
            zip_bytes.push_back(v & 0xFF);
            zip_bytes.push_back((v >> 8) & 0xFF);
        };
        auto w32 = [&](uint32_t v) {
            zip_bytes.push_back(v & 0xFF);
            zip_bytes.push_back((v >> 8) & 0xFF);
            zip_bytes.push_back((v >> 16) & 0xFF);
            zip_bytes.push_back((v >> 24) & 0xFF);
        };

        struct CentralDirEntry {
            std::string filename;
            uint32_t crc = 0;
            uint32_t comp_size = 0;
            uint32_t uncomp_size = 0;
            uint32_t local_hdr_offset = 0;
            uint16_t comp_method = 8; // 8 = Deflate
        };

        std::vector<CentralDirEntry> cd_entries;

        for (const auto& entry : entries) {
            uint32_t local_hdr_offset = static_cast<uint32_t>(zip_bytes.size());
            uint32_t crc = static_cast<uint32_t>(crc32(0L, Z_NULL, 0));
            if (!entry.data.empty()) {
                crc = static_cast<uint32_t>(crc32(crc, entry.data.data(), entry.data.size()));
            }

            std::vector<uint8_t> compressed = deflate_raw(entry.data);
            uint16_t comp_method = 8;
            uint32_t comp_size = static_cast<uint32_t>(compressed.size());
            // If compressed data is not smaller, store uncompressed
            if (comp_size >= entry.data.size()) {
                compressed = entry.data;
                comp_method = 0; // Store
                comp_size = static_cast<uint32_t>(compressed.size());
            }

            uint32_t uncomp_size = static_cast<uint32_t>(entry.data.size());

            // Local file header: signature 0x04034b50 ("PK\x03\x04")
            w32(0x04034b50);
            w16(20);          // version needed to extract (2.0)
            w16(0);           // general purpose bit flag
            w16(comp_method); // compression method
            w16(0x5600);      // file last mod time
            w16(0x7000);      // file last mod date
            w32(crc);         // CRC-32
            w32(comp_size);   // compressed size
            w32(uncomp_size); // uncompressed size
            w16(static_cast<uint16_t>(entry.filename.size())); // file name length
            w16(0);           // extra field length

            // File name
            zip_bytes.insert(zip_bytes.end(), entry.filename.begin(), entry.filename.end());

            // File data
            zip_bytes.insert(zip_bytes.end(), compressed.begin(), compressed.end());

            CentralDirEntry cd;
            cd.filename = entry.filename;
            cd.crc = crc;
            cd.comp_size = comp_size;
            cd.uncomp_size = uncomp_size;
            cd.local_hdr_offset = local_hdr_offset;
            cd.comp_method = comp_method;
            cd_entries.push_back(cd);
        }

        uint32_t cd_offset = static_cast<uint32_t>(zip_bytes.size());

        // Central directory headers
        for (const auto& cd : cd_entries) {
            // Central directory file header signature 0x02014b50 ("PK\x01\x02")
            w32(0x02014b50);
            w16(20);             // version made by
            w16(20);             // version needed to extract
            w16(0);              // general purpose bit flag
            w16(cd.comp_method); // compression method
            w16(0x5600);         // file last mod time
            w16(0x7000);         // file last mod date
            w32(cd.crc);         // crc-32
            w32(cd.comp_size);   // compressed size
            w32(cd.uncomp_size); // uncompressed size
            w16(static_cast<uint16_t>(cd.filename.size())); // file name length
            w16(0);              // extra field length
            w16(0);              // file comment length
            w16(0);              // disk number start
            w16(0);              // internal file attributes
            w32(0x81B60000);     // external file attributes (-rw-rw-r--)
            w32(cd.local_hdr_offset); // relative offset of local header

            zip_bytes.insert(zip_bytes.end(), cd.filename.begin(), cd.filename.end());
        }

        uint32_t cd_size = static_cast<uint32_t>(zip_bytes.size() - cd_offset);

        // End of central directory record (EOCD): signature 0x06054b50 ("PK\x05\x06")
        w32(0x06054b50);
        w16(0); // number of this disk
        w16(0); // number of the disk with the start of the central directory
        w16(static_cast<uint16_t>(cd_entries.size())); // total number of entries on this disk
        w16(static_cast<uint16_t>(cd_entries.size())); // total number of entries in central dir
        w32(cd_size);   // size of the central directory
        w32(cd_offset); // offset of start of central directory
        w16(0); // .ZIP file comment length

        std::ofstream out(output_path, std::ios::binary);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(zip_bytes.data()), zip_bytes.size());
        out.close();
        return true;
    }
};

#endif
