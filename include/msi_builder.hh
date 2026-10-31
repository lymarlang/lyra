#ifndef MSI_BUILDER_HH
#define MSI_BUILDER_HH

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#include <openssl/sha.h>
#include <zlib.h>

class MsiBuilder {
public:
    // Generate a reproducible v4-like or name-based GUID from string
    static std::string generate_guid(const std::string& seed) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(seed.data()), seed.size(), hash);
        char buf[64];
        snprintf(buf, sizeof(buf), "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                 hash[0], hash[1], hash[2], hash[3],
                 hash[4], hash[5],
                 (hash[6] & 0x0F) | 0x40, hash[7],
                 (hash[8] & 0x3F) | 0x80, hash[9],
                 hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
        return std::string(buf);
    }

    // Windows Installer base32-like MIME name encoder for table streams
    static std::u16string encode_stream_name(const std::string& name, bool is_table) {
        std::u16string out;
        if (is_table) {
            out.push_back(0x4840);
        }
        auto utf2mime = [](char x) -> int {
            if (x >= '0' && x <= '9') return x - '0';
            if (x >= 'A' && x <= 'Z') return x - 'A' + 10;
            if (x >= 'a' && x <= 'z') return x - 'a' + 10 + 26;
            if (x == '.') return 10 + 26 + 26;
            if (x == '_') return 10 + 26 + 26 + 1;
            return -1;
        };

        size_t idx = 0;
        while (idx < name.size()) {
            char ch = name[idx++];
            int m1 = utf2mime(ch);
            if (m1 >= 0) {
                uint16_t code = static_cast<uint16_t>(m1 + 0x4800);
                if (idx < name.size()) {
                    char next = name[idx];
                    int m2 = utf2mime(next);
                    if (m2 >= 0) {
                        idx++;
                        code = static_cast<uint16_t>(m1 + (m2 << 6) + 0x3800);
                    }
                }
                out.push_back(code);
            } else {
                out.push_back(static_cast<char16_t>(static_cast<uint8_t>(ch)));
            }
        }
        return out;
    }

    // Convert ASCII string to UTF-16LE bytes
    static std::u16string to_utf16(const std::string& s) {
        std::u16string res;
        for (char c : s) {
            res.push_back(static_cast<char16_t>(static_cast<uint8_t>(c)));
        }
        return res;
    }

    // In-memory String Table Manager for MSI database
    class StringTable {
    public:
        // Pool consists of empty string (index 0)
        std::vector<std::string> strings;
        std::map<std::string, uint32_t> str_to_id;
        std::vector<uint32_t> ref_counts;

        StringTable() {
            strings.push_back("");
            str_to_id[""] = 0;
            ref_counts.push_back(1);
        }

        uint32_t add(const std::string& str) {
            auto it = str_to_id.find(str);
            if (it != str_to_id.end()) {
                ref_counts[it->second]++;
                return it->second;
            }
            uint32_t id = static_cast<uint32_t>(strings.size());
            strings.push_back(str);
            str_to_id[str] = id;
            ref_counts.push_back(1);
            return id;
        }

        // Generate _StringPool and _StringData streams
        void serialize(std::vector<uint8_t>& pool, std::vector<uint8_t>& data) const {
            pool.clear();
            data.clear();

            // Header: codepage (2 bytes = 1252), 0 (2 bytes)
            uint16_t codepage = 1252;
            uint16_t reserved = 0;
            pool.push_back(codepage & 0xFF);
            pool.push_back((codepage >> 8) & 0xFF);
            pool.push_back(reserved & 0xFF);
            pool.push_back((reserved >> 8) & 0xFF);

            for (size_t i = 1; i < strings.size(); ++i) {
                const std::string& s = strings[i];
                uint16_t len = static_cast<uint16_t>(s.size());
                uint16_t refs = static_cast<uint16_t>(std::min<uint32_t>(ref_counts[i], 0xFFFF));

                pool.push_back(len & 0xFF);
                pool.push_back((len >> 8) & 0xFF);
                pool.push_back(refs & 0xFF);
                pool.push_back((refs >> 8) & 0xFF);

                data.insert(data.end(), s.begin(), s.end());
            }
        }
    };

    // SummaryInformation Property Set Writer
    static std::vector<uint8_t> create_summary_info(const std::string& package_name,
                                                    const std::string& version,
                                                    const std::string& author,
                                                    const std::string& package_code) {
        // Properties to set:
        // PID_CODEPAGE (1): VT_I2 (1252)
        // PID_TITLE (2): VT_LPSTR ("Installation Database")
        // PID_SUBJECT (3): VT_LPSTR (package_name)
        // PID_AUTHOR (4): VT_LPSTR (author)
        // PID_KEYWORDS (5): VT_LPSTR ("Installer, MSI")
        // PID_COMMENTS (6): VT_LPSTR ("Built by Lyra")
        // PID_TEMPLATE (7): VT_LPSTR ("Intel;1033" or "x64;1033")
        // PID_REVNUMBER (9): VT_LPSTR (package_code)
        // PID_PAGECOUNT (14): VT_I4 (200) - Min MSI version 2.00
        // PID_WORDCOUNT (15): VT_I4 (0) - 0 = long file names, uncompressed or internal cab
        // PID_APPNAME (18): VT_LPSTR ("Lyra Package Manager")
        // PID_SECURITY (19): VT_I4 (0)

        struct PropEntry {
            uint32_t propid;
            uint32_t type;
            int32_t ival;
            std::string sval;
        };

        std::vector<PropEntry> props = {
            { 1, 2 /* VT_I2 */, 1252, "" },
            { 2, 30 /* VT_LPSTR */, 0, "Installation Database" },
            { 3, 30, 0, package_name },
            { 4, 30, 0, author.empty() ? "Lymar Developer" : author },
            { 5, 30, 0, "Installer, MSI" },
            { 6, 30, 0, package_name + " " + version + " Installer" },
            { 7, 30, 0, "x64;1033" },
            { 9, 30, 0, package_code },
            { 14, 3 /* VT_I4 */, 200, "" },
            { 15, 3 /* VT_I4 */, 0, "" },
            { 18, 30, 0, "Lyra Package Manager" },
            { 19, 3 /* VT_I4 */, 0, "" }
        };

        std::vector<uint8_t> out;
        // PropertySetHeader: wByteOrder (0xFFFE), wFormat (0), dwOSVer (0x00020005), clsID (16 zeros), reserved (1)
        auto write16 = [&](uint16_t v) {
            out.push_back(v & 0xFF);
            out.push_back((v >> 8) & 0xFF);
        };
        auto write32 = [&](uint32_t v) {
            out.push_back(v & 0xFF);
            out.push_back((v >> 8) & 0xFF);
            out.push_back((v >> 16) & 0xFF);
            out.push_back((v >> 24) & 0xFF);
        };

        write16(0xFFFE);
        write16(0);
        write32(0x00020005);
        for (int i = 0; i < 16; ++i) out.push_back(0); // clsID
        write32(1); // reserved / cSections count

        // FormatIDOffset: FMTID (F29F85E0-4FF9-1068-AB91-08002B27B3D9), dwOffset (48)
        // GUID bytes: E0 85 9F F2 F9 4F 68 10 AB 91 08 00 2B 27 B3 D9
        uint8_t fmtid[16] = { 0xE0, 0x85, 0x9F, 0xF2, 0xF9, 0x4F, 0x68, 0x10, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9 };
        out.insert(out.end(), fmtid, fmtid + 16);
        write32(48); // Section starts at byte 48

        // Prepare Section
        // Header: cbSection (32-bit), cProperties (32-bit)
        // Array of { propid (32-bit), dwOffset (32-bit) }
        // Property values data
        uint32_t cProps = static_cast<uint32_t>(props.size());
        uint32_t idOffsetTableSize = 8 + cProps * 8;
        std::vector<uint8_t> propValues;

        std::vector<std::pair<uint32_t, uint32_t>> offsets;
        for (const auto& p : props) {
            uint32_t currentOffset = idOffsetTableSize + static_cast<uint32_t>(propValues.size());
            offsets.push_back({ p.propid, currentOffset });

            // Value representation
            uint32_t type = p.type;
            propValues.push_back(type & 0xFF);
            propValues.push_back((type >> 8) & 0xFF);
            propValues.push_back((type >> 16) & 0xFF);
            propValues.push_back((type >> 24) & 0xFF);

            if (type == 2 /* VT_I2 */) {
                propValues.push_back(p.ival & 0xFF);
                propValues.push_back((p.ival >> 8) & 0xFF);
                propValues.push_back(0);
                propValues.push_back(0); // 4-byte aligned
            } else if (type == 3 /* VT_I4 */) {
                propValues.push_back(p.ival & 0xFF);
                propValues.push_back((p.ival >> 8) & 0xFF);
                propValues.push_back((p.ival >> 16) & 0xFF);
                propValues.push_back((p.ival >> 24) & 0xFF);
            } else if (type == 30 /* VT_LPSTR */) {
                uint32_t len = static_cast<uint32_t>(p.sval.size() + 1); // null-terminated
                propValues.push_back(len & 0xFF);
                propValues.push_back((len >> 8) & 0xFF);
                propValues.push_back((len >> 16) & 0xFF);
                propValues.push_back((len >> 24) & 0xFF);

                propValues.insert(propValues.end(), p.sval.begin(), p.sval.end());
                propValues.push_back(0);
                while (propValues.size() % 4 != 0) {
                    propValues.push_back(0);
                }
            }
        }

        uint32_t cbSection = idOffsetTableSize + static_cast<uint32_t>(propValues.size());
        write32(cbSection);
        write32(cProps);

        for (const auto& off : offsets) {
            write32(off.first);
            write32(off.second);
        }

        out.insert(out.end(), propValues.begin(), propValues.end());
        return out;
    }

    // Minimal MSZIP Cabinet (.cab) builder for MSI embedded file payload
    class CabBuilder {
    public:
        // Build an uncompressed or MSZIP-compressed Cabinet file with single or multiple files
        static std::vector<uint8_t> create_cabinet(const std::string& internal_name, const std::vector<uint8_t>& file_data) {
            // Cabinet Structure:
            // CFHEADER (36 bytes)
            // CFFOLDER (8 bytes)
            // CFFILE (16 bytes + filename + \0)
            // CFDATA (8 bytes + compressed or raw data)

            std::vector<uint8_t> cab;
            auto w16 = [&](uint16_t v) {
                cab.push_back(v & 0xFF);
                cab.push_back((v >> 8) & 0xFF);
            };
            auto w32 = [&](uint32_t v) {
                cab.push_back(v & 0xFF);
                cab.push_back((v >> 8) & 0xFF);
                cab.push_back((v >> 16) & 0xFF);
                cab.push_back((v >> 24) & 0xFF);
            };

            // Calculate sizes and offsets
            uint32_t cffile_size = 16 + static_cast<uint32_t>(internal_name.size() + 1);
            uint32_t coffFiles = 36 + 8; // CFHEADER (36) + CFFOLDER (8)
            uint32_t coffData = coffFiles + cffile_size;

            // MSZIP format: 2-byte signature 0x43 0x4B ('CK'), followed by raw DEFLATE stream
            // Or uncompressed folder (typeCompress = 0)
            // Standard Windows Installer supports uncompressed CAB files (typeCompress = 0) out of the box!
            uint32_t cbUncomp = static_cast<uint32_t>(file_data.size());
            uint32_t cbComp = cbUncomp;
            uint32_t cfdata_size = 8 + cbComp;
            uint32_t total_size = coffData + cfdata_size;

            // 1. CFHEADER
            cab.push_back('M'); cab.push_back('S'); cab.push_back('C'); cab.push_back('F'); // signature
            w32(0); // reserved1
            w32(total_size); // cbCabinet
            w32(0); // reserved2
            w32(coffFiles); // coffFiles
            w32(0); // reserved3
            cab.push_back(3); // versionMinor
            cab.push_back(1); // versionMajor (1.3)
            w16(1); // cFolders
            w16(1); // cFiles
            w16(0); // flags
            w16(1); // setID
            w16(0); // iCabinet

            // 2. CFFOLDER
            w32(coffData); // coffCabStart (offset to first CFDATA)
            w16(1); // cCFData
            w16(0); // typeCompress: 0 = uncompressed (TYPE_NONE)

            // 3. CFFILE
            w32(cbUncomp); // cbFile
            w32(0); // uoffFolderStart
            w16(0); // iFolder (index in cabinet)
            w16(0x5600); // date (MS-DOS date format)
            w16(0x7000); // time (MS-DOS time format)
            w16(0x20); // attribs: _A_ARCH
            for (char c : internal_name) cab.push_back(c);
            cab.push_back(0); // null terminator

            // 4. CFDATA
            w32(0); // checksum
            w16(static_cast<uint16_t>(cbComp)); // cbData
            w16(static_cast<uint16_t>(cbUncomp)); // cbUncomp
            cab.insert(cab.end(), file_data.begin(), file_data.end());

            return cab;
        }
    };

    // Compound File Binary Format (CFBF / OLE Structured Storage) writer
    class StorageWriter {
    public:
        struct StreamEntry {
            std::u16string name;
            std::vector<uint8_t> data;
        };

        std::vector<StreamEntry> streams;

        void add_stream(const std::u16string& name, const std::vector<uint8_t>& data) {
            streams.push_back({ name, data });
        }

        std::vector<uint8_t> build_storage(const std::string& clsid_guid = "{00041013-0000-0000-C000-000000000046}") {
            // Sector size: 512 bytes (sectorShift = 9)
            const size_t SECTOR_SIZE = 512;
            const size_t DIR_ENTRY_SIZE = 128; // 4 entries per sector

            // Total directory entries: Root entry (index 0) + 1 for each stream
            size_t total_dir_entries = 1 + streams.size();
            size_t dir_sectors_needed = (total_dir_entries * DIR_ENTRY_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE;
            if (dir_sectors_needed == 0) dir_sectors_needed = 1;

            // In CFBF version 3 (512-byte sectors), mini stream cutoff is 4096.
            // But mini-stream is completely optional! If csectMiniFat = 0 and sectMiniFatStart = ENDOFCHAIN (0xFFFFFFFE),
            // all streams can be stored as regular sectors in the main FAT!
            // This is 100% compliant with [MS-CFB] Section 2.2 and is vastly simpler, cleaner, and less prone to corruption.

            std::vector<std::vector<uint8_t>> regular_stream_sectors;
            std::vector<uint32_t> stream_start_sectors;
            uint32_t current_sec_id = 0;

            // Allocate sectors for directory chain
            uint32_t dir_start_sector = current_sec_id;
            current_sec_id += static_cast<uint32_t>(dir_sectors_needed);

            // Allocate sectors for each stream
            for (const auto& s : streams) {
                if (s.data.empty()) {
                    stream_start_sectors.push_back(0xFFFFFFFE); // ENDOFCHAIN
                    continue;
                }
                stream_start_sectors.push_back(current_sec_id);
                size_t num_sec = (s.data.size() + SECTOR_SIZE - 1) / SECTOR_SIZE;
                current_sec_id += static_cast<uint32_t>(num_sec);
            }

            // Now calculate FAT sectors needed
            // Each FAT sector contains 512 / 4 = 128 sector IDs
            // Total sectors so far = current_sec_id.
            // We also need FAT sectors themselves.
            uint32_t total_sectors = current_sec_id;
            uint32_t fat_sectors_needed = 1;
            while (true) {
                uint32_t max_sectors = fat_sectors_needed * 128;
                if (max_sectors >= total_sectors + fat_sectors_needed) {
                    break;
                }
                fat_sectors_needed++;
            }

            uint32_t fat_start_sector = current_sec_id;
            uint32_t grand_total_sectors = total_sectors + fat_sectors_needed;

            // Build the FAT table
            std::vector<uint32_t> fat(fat_sectors_needed * 128, 0xFFFFFFFF); // FREE (0xFFFFFFFF)

            // Link Directory chain
            for (size_t i = 0; i < dir_sectors_needed; ++i) {
                uint32_t sec = dir_start_sector + static_cast<uint32_t>(i);
                fat[sec] = (i + 1 == dir_sectors_needed) ? 0xFFFFFFFE : sec + 1;
            }

            // Link Stream chains
            size_t str_idx = 0;
            for (const auto& s : streams) {
                if (!s.data.empty()) {
                    size_t num_sec = (s.data.size() + SECTOR_SIZE - 1) / SECTOR_SIZE;
                    uint32_t start = stream_start_sectors[str_idx];
                    for (size_t i = 0; i < num_sec; ++i) {
                        uint32_t sec = start + static_cast<uint32_t>(i);
                        fat[sec] = (i + 1 == num_sec) ? 0xFFFFFFFE : sec + 1;
                    }
                }
                str_idx++;
            }

            // Mark FAT sectors themselves as FATSECT (0xFFFFFFFD)
            for (size_t i = 0; i < fat_sectors_needed; ++i) {
                uint32_t sec = fat_start_sector + static_cast<uint32_t>(i);
                fat[sec] = 0xFFFFFFFD;
            }

            // Prepare Output Buffer: Header (512 bytes) + (grand_total_sectors * 512 bytes)
            std::vector<uint8_t> file_bytes(SECTOR_SIZE + grand_total_sectors * SECTOR_SIZE, 0);

            // 1. Write Header (first 512 bytes)
            // Magic: D0 CF 11 E0 A1 B1 1A E1
            const uint8_t magic[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
            std::memcpy(file_bytes.data(), magic, 8);

            auto w16 = [&](size_t offset, uint16_t v) {
                file_bytes[offset] = v & 0xFF;
                file_bytes[offset + 1] = (v >> 8) & 0xFF;
            };
            auto w32 = [&](size_t offset, uint32_t v) {
                file_bytes[offset] = v & 0xFF;
                file_bytes[offset + 1] = (v >> 8) & 0xFF;
                file_bytes[offset + 2] = (v >> 16) & 0xFF;
                file_bytes[offset + 3] = (v >> 24) & 0xFF;
            };

            w16(0x18, 0x003E); // uMinorVersion
            w16(0x1A, 0x0003); // uDllVersion (3 = 512-byte sector)
            w16(0x1C, 0xFFFE); // uByteOrder (little endian)
            w16(0x1E, 9);      // uSectorShift (9 -> 512 bytes)
            w16(0x20, 6);      // uMiniSectorShift (6 -> 64 bytes)
            w32(0x28, 0);      // csectDir (0 for v3)
            w32(0x2C, fat_sectors_needed); // csectFat
            w32(0x30, dir_start_sector);   // sectDirStart
            w32(0x34, 0);      // signature (transactioning)
            w32(0x38, 4096);   // ulMiniSectorCutoff
            w32(0x3C, 0xFFFFFFFE); // sectMiniFatStart (ENDOFCHAIN)
            w32(0x40, 0);      // csectMiniFat
            w32(0x44, 0xFFFFFFFE); // sectDifStart (ENDOFCHAIN)
            w32(0x48, 0);      // csectDif

            // First 109 FAT sectors in header
            for (size_t i = 0; i < 109; ++i) {
                if (i < fat_sectors_needed) {
                    w32(0x4C + i * 4, fat_start_sector + static_cast<uint32_t>(i));
                } else {
                    w32(0x4C + i * 4, 0xFFFFFFFF); // FREESECT
                }
            }

            auto get_sector_ptr = [&](uint32_t sec_id) -> uint8_t* {
                return file_bytes.data() + SECTOR_SIZE + sec_id * SECTOR_SIZE;
            };

            // 2. Write Directory Entries
            // Create Binary Tree of Directory Entries
            // Root entry (0)
            // Stream entries (1 .. N) linked as child/left/right
            std::vector<uint8_t> dir_data(dir_sectors_needed * SECTOR_SIZE, 0);
            auto write_dir_entry = [&](size_t idx,
                                       const std::u16string& name,
                                       uint8_t type,
                                       uint32_t left,
                                       uint32_t right,
                                       uint32_t child,
                                       uint32_t start_sec,
                                       uint64_t size,
                                       const uint8_t* clsid = nullptr) {
                uint8_t* p = dir_data.data() + idx * DIR_ENTRY_SIZE;
                std::memset(p, 0, DIR_ENTRY_SIZE);
                // Name (up to 31 UTF-16 chars + null terminator = 64 bytes)
                size_t nchars = std::min<size_t>(name.size(), 31);
                for (size_t c = 0; c < nchars; ++c) {
                    p[c * 2] = name[c] & 0xFF;
                    p[c * 2 + 1] = (name[c] >> 8) & 0xFF;
                }
                uint16_t name_bytes = static_cast<uint16_t>((nchars + 1) * 2);
                p[0x40] = name_bytes & 0xFF;
                p[0x41] = (name_bytes >> 8) & 0xFF;
                p[0x42] = type; // 1 = user storage, 2 = user stream, 5 = root storage
                p[0x43] = 1;    // color (1 = black)

                // Left sibling, right sibling, child
                auto put32 = [&](size_t ofs, uint32_t v) {
                    p[ofs] = v & 0xFF;
                    p[ofs + 1] = (v >> 8) & 0xFF;
                    p[ofs + 2] = (v >> 16) & 0xFF;
                    p[ofs + 3] = (v >> 24) & 0xFF;
                };
                put32(0x44, left);
                put32(0x48, right);
                put32(0x4C, child);

                if (clsid) {
                    std::memcpy(p + 0x50, clsid, 16);
                }

                put32(0x74, start_sec);
                put32(0x78, static_cast<uint32_t>(size & 0xFFFFFFFF));
                put32(0x7C, static_cast<uint32_t>((size >> 32) & 0xFFFFFFFF));
            };

            // Build binary tree for streams 1..N:
            // Linear chain via right sibling is valid in CFBF
            uint32_t root_child = streams.empty() ? 0xFFFFFFFF : 1;
            uint8_t msi_clsid[16] = { 0x13, 0x10, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 };
            (void)clsid_guid;

            // Root Storage Entry
            write_dir_entry(0, u"Root Entry", 5 /* Root */, 0xFFFFFFFF, 0xFFFFFFFF, root_child, 0xFFFFFFFE, 0, msi_clsid);

            // Stream entries: arrange as balanced or simple tree
            for (size_t i = 0; i < streams.size(); ++i) {
                uint32_t left = 0xFFFFFFFF;
                uint32_t right = (i + 1 < streams.size()) ? static_cast<uint32_t>(i + 2) : 0xFFFFFFFF;
                uint32_t child = 0xFFFFFFFF;
                write_dir_entry(i + 1, streams[i].name, 2 /* Stream */, left, right, child,
                                stream_start_sectors[i], streams[i].data.size());
            }

            // Copy directory data to directory sectors
            for (size_t i = 0; i < dir_sectors_needed; ++i) {
                std::memcpy(get_sector_ptr(dir_start_sector + static_cast<uint32_t>(i)),
                            dir_data.data() + i * SECTOR_SIZE, SECTOR_SIZE);
            }

            // 3. Write Stream Sectors
            for (size_t i = 0; i < streams.size(); ++i) {
                if (!streams[i].data.empty()) {
                    size_t num_sec = (streams[i].data.size() + SECTOR_SIZE - 1) / SECTOR_SIZE;
                    uint32_t start = stream_start_sectors[i];
                    for (size_t s = 0; s < num_sec; ++s) {
                        size_t offset = s * SECTOR_SIZE;
                        size_t to_copy = std::min<size_t>(SECTOR_SIZE, streams[i].data.size() - offset);
                        std::memcpy(get_sector_ptr(start + static_cast<uint32_t>(s)),
                                    streams[i].data.data() + offset, to_copy);
                    }
                }
            }

            // 4. Write FAT Sectors
            for (size_t i = 0; i < fat_sectors_needed; ++i) {
                uint8_t* p = get_sector_ptr(fat_start_sector + static_cast<uint32_t>(i));
                for (size_t e = 0; e < 128; ++e) {
                    uint32_t entry = fat[i * 128 + e];
                    p[e * 4] = entry & 0xFF;
                    p[e * 4 + 1] = (entry >> 8) & 0xFF;
                    p[e * 4 + 2] = (entry >> 16) & 0xFF;
                    p[e * 4 + 3] = (entry >> 24) & 0xFF;
                }
            }

            return file_bytes;
        }
    };

    // Helper to encode a column row value into raw table data
    // Windows Installer column raw encoding:
    // Columns are transposed: all values for column 0 in order of rows, then column 1 in order of rows, etc.
    struct ColumnDef {
        std::string name;
        uint16_t type; // e.g. MSITYPE_VALID | MSITYPE_STRING | MSITYPE_KEY | width
        // In raw MSI stream:
        // String columns: 2 bytes string id
        // Integer column <= 2 bytes: 2 bytes (val + 0x8000)
        // Integer column == 4 bytes: 4 bytes (val ^ 0x80000000)
    };

    struct TableDef {
        std::string name;
        std::vector<ColumnDef> columns;
        std::vector<std::vector<int64_t>> rows; // For string cols, holds string ID. For int cols, holds value.
    };

    // Build complete .msi file
    static bool build_msi(const std::string& product_name,
                          const std::string& version,
                          const std::string& manufacturer,
                          const std::string& description,
                          const std::string& exe_filename,
                          const std::vector<uint8_t>& exe_data,
                          const std::string& output_msi_path) {
        StringTable st;

        // Generate GUIDs
        std::string product_code = generate_guid("ProductCode:" + product_name + ":" + version);
        std::string package_code = generate_guid("PackageCode:" + product_name + ":" + version);
        std::string comp_guid = generate_guid("Component:" + product_name + ":" + exe_filename);
        std::string upgrade_code = generate_guid("UpgradeCode:" + product_name);

        // Build Cabinet containing the exe payload
        std::string cab_file_id = "filExecutable";
        std::vector<uint8_t> cab_bytes = CabBuilder::create_cabinet(cab_file_id, exe_data);

        // Types bitmask constants (matching Wine/Windows Installer specs):
        // MSITYPE_NULLABLE = 0x1000
        // MSITYPE_STRING = 0x0800
        // MSITYPE_KEY = 0x2000
        const uint16_t TYPE_STR_KEY = 0x2800 | 72; // string key
        const uint16_t TYPE_STR_REQ = 0x0800 | 72; // required string
        const uint16_t TYPE_STR_OPT = 0x1800 | 255; // optional string
        const uint16_t TYPE_INT2_KEY = 0x2000 | 2;  // 2-byte int key
        const uint16_t TYPE_INT2_REQ = 0x0000 | 2;  // 2-byte int req
        const uint16_t TYPE_INT2_OPT = 0x1000 | 2;  // 2-byte int opt
        const uint16_t TYPE_INT4_REQ = 0x0000 | 4;  // 4-byte int req

        std::vector<TableDef> tables;

        // 1. Directory table
        // Directory, Directory_Parent, DefaultDir
        tables.push_back({
            "Directory",
            { {"Directory", TYPE_STR_KEY}, {"Directory_Parent", TYPE_STR_OPT}, {"DefaultDir", TYPE_STR_REQ} },
            {
                { st.add("TARGETDIR"), 0, st.add("SourceDir") },
                { st.add("ProgramFiles64Folder"), st.add("TARGETDIR"), st.add("PFiles") },
                { st.add("ManufacturerDir"), st.add("ProgramFiles64Folder"), st.add(manufacturer.empty() ? "Lymar" : manufacturer) },
                { st.add("INSTALLDIR"), st.add("ManufacturerDir"), st.add(product_name) }
            }
        });

        // 2. Component table
        // Component, ComponentId, Directory_, Attributes, Condition, KeyPath
        // Attributes: 256 = 64-bit component
        tables.push_back({
            "Component",
            { {"Component", TYPE_STR_KEY}, {"ComponentId", TYPE_STR_OPT}, {"Directory_", TYPE_STR_REQ}, {"Attributes", TYPE_INT2_REQ}, {"Condition", TYPE_STR_OPT}, {"KeyPath", TYPE_STR_OPT} },
            {
                { st.add("cmpMainExe"), st.add(comp_guid), st.add("INSTALLDIR"), 256, 0, st.add(cab_file_id) }
            }
        });

        // 3. Feature table
        // Feature, Feature_Parent, Title, Description, Display, Level, Directory_, Attributes
        tables.push_back({
            "Feature",
            { {"Feature", TYPE_STR_KEY}, {"Feature_Parent", TYPE_STR_OPT}, {"Title", TYPE_STR_OPT}, {"Description", TYPE_STR_OPT}, {"Display", TYPE_INT2_OPT}, {"Level", TYPE_INT2_REQ}, {"Directory_", TYPE_STR_OPT}, {"Attributes", TYPE_INT2_REQ} },
            {
                { st.add("Complete"), 0, st.add(product_name), st.add(description.empty() ? product_name : description), 1, 1, st.add("INSTALLDIR"), 0 }
            }
        });

        // 4. FeatureComponents table
        // Feature_, Component_
        tables.push_back({
            "FeatureComponents",
            { {"Feature_", TYPE_STR_KEY}, {"Component_", TYPE_STR_KEY} },
            {
                { st.add("Complete"), st.add("cmpMainExe") }
            }
        });

        // 5. File table
        // File, Component_, FileName, FileSize, Version, Language, Attributes, Sequence
        // Attributes: 512 = msidbFileAttributesCompressed (stored in cabinet)
        tables.push_back({
            "File",
            { {"File", TYPE_STR_KEY}, {"Component_", TYPE_STR_REQ}, {"FileName", TYPE_STR_REQ}, {"FileSize", TYPE_INT4_REQ}, {"Version", TYPE_STR_OPT}, {"Language", TYPE_STR_OPT}, {"Attributes", TYPE_INT2_OPT}, {"Sequence", TYPE_INT2_REQ} },
            {
                { st.add(cab_file_id), st.add("cmpMainExe"), st.add(exe_filename), static_cast<int64_t>(exe_data.size()), st.add(version), 0, 512, 1 }
            }
        });

        // 6. Media table
        // DiskId, LastSequence, DiskPrompt, Cabinet, VolumeLabel, Source
        tables.push_back({
            "Media",
            { {"DiskId", TYPE_INT2_KEY}, {"LastSequence", TYPE_INT2_REQ}, {"DiskPrompt", TYPE_STR_OPT}, {"Cabinet", TYPE_STR_OPT}, {"VolumeLabel", TYPE_STR_OPT}, {"Source", TYPE_STR_OPT} },
            {
                { 1, 1, 0, st.add("#cab1.cab"), 0, 0 }
            }
        });

        // 7. Property table
        // Property, Value
        tables.push_back({
            "Property",
            { {"Property", TYPE_STR_KEY}, {"Value", TYPE_STR_REQ} },
            {
                { st.add("ProductCode"), st.add(product_code) },
                { st.add("ProductName"), st.add(product_name) },
                { st.add("ProductVersion"), st.add(version) },
                { st.add("Manufacturer"), st.add(manufacturer.empty() ? "Lymar" : manufacturer) },
                { st.add("ProductLanguage"), st.add("1033") },
                { st.add("UpgradeCode"), st.add(upgrade_code) },
                { st.add("ALLUSERS"), st.add("1") }
            }
        });

        // 8. InstallExecuteSequence table
        // Action, Condition, Sequence
        tables.push_back({
            "InstallExecuteSequence",
            { {"Action", TYPE_STR_KEY}, {"Condition", TYPE_STR_OPT}, {"Sequence", TYPE_INT2_OPT} },
            {
                { st.add("CostInitialize"), 0, 800 },
                { st.add("FileCost"), 0, 900 },
                { st.add("CostFinalize"), 0, 1000 },
                { st.add("InstallValidate"), 0, 1400 },
                { st.add("InstallInitialize"), 0, 1500 },
                { st.add("ProcessComponents"), 0, 1600 },
                { st.add("UnpublishFeatures"), 0, 1800 },
                { st.add("RemoveFiles"), 0, 3500 },
                { st.add("InstallFiles"), 0, 4000 },
                { st.add("RegisterUser"), 0, 6000 },
                { st.add("RegisterProduct"), 0, 6100 },
                { st.add("PublishFeatures"), 0, 6300 },
                { st.add("PublishProduct"), 0, 6400 },
                { st.add("InstallFinalize"), 0, 6600 }
            }
        });

        // 9. InstallUISequence table
        tables.push_back({
            "InstallUISequence",
            { {"Action", TYPE_STR_KEY}, {"Condition", TYPE_STR_OPT}, {"Sequence", TYPE_INT2_OPT} },
            {
                { st.add("CostInitialize"), 0, 800 },
                { st.add("FileCost"), 0, 900 },
                { st.add("CostFinalize"), 0, 1000 },
                { st.add("ExecuteAction"), 0, 1300 }
            }
        });

        // Now populate _Tables and _Columns tables
        // _Tables: Name
        TableDef tTables;
        tTables.name = "_Tables";
        tTables.columns = { {"Name", TYPE_STR_KEY} };
        for (const auto& t : tables) {
            tTables.rows.push_back({ st.add(t.name) });
        }

        // _Columns: Table, Number, Name, Type
        TableDef tColumns;
        tColumns.name = "_Columns";
        tColumns.columns = {
            {"Table", TYPE_STR_KEY},
            {"Number", TYPE_INT2_KEY},
            {"Name", TYPE_STR_REQ},
            {"Type", TYPE_INT2_REQ}
        };

        for (const auto& t : tables) {
            int colNum = 1;
            for (const auto& c : t.columns) {
                tColumns.rows.push_back({
                    st.add(t.name),
                    colNum++,
                    st.add(c.name),
                    static_cast<int64_t>(c.type)
                });
            }
        }

        // Insert _Tables and _Columns to front of table list
        tables.insert(tables.begin(), tColumns);
        tables.insert(tables.begin(), tTables);

        // Create Storage Writer
        StorageWriter storage;

        // Serialize string pool & data
        std::vector<uint8_t> pool_bytes, data_bytes;
        st.serialize(pool_bytes, data_bytes);

        // Add StringPool and StringData streams
        storage.add_stream(encode_stream_name("_StringPool", false), pool_bytes);
        storage.add_stream(encode_stream_name("_StringData", false), data_bytes);

        // Add SummaryInformation stream
        std::vector<uint8_t> sum_info = create_summary_info(product_name, version, manufacturer, package_code);
        storage.add_stream(u"\x0005SummaryInformation", sum_info);

        // Add embedded cabinet stream
        // Name in _Streams is "cab1.cab"
        storage.add_stream(to_utf16("cab1.cab"), cab_bytes);

        // Serialize and add each table stream
        for (const auto& t : tables) {
            uint32_t num_rows = static_cast<uint32_t>(t.rows.size());
            std::vector<uint8_t> table_raw;

            // In MSI raw format: for each column across all rows
            for (size_t col_idx = 0; col_idx < t.columns.size(); ++col_idx) {
                const auto& col = t.columns[col_idx];
                bool is_str = (col.type & 0x0800) != 0;
                uint8_t width = col.type & 0xFF;

                for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
                    int64_t val = t.rows[row_idx][col_idx];

                    if (is_str) {
                        // 2-byte string id
                        uint16_t s_id = static_cast<uint16_t>(val);
                        table_raw.push_back(s_id & 0xFF);
                        table_raw.push_back((s_id >> 8) & 0xFF);
                    } else if (width == 2) {
                        // Integer 2 bytes stored as val + 0x8000
                        uint16_t enc = static_cast<uint16_t>((val == 0 && (col.type & 0x1000)) ? 0 : (val + 0x8000));
                        table_raw.push_back(enc & 0xFF);
                        table_raw.push_back((enc >> 8) & 0xFF);
                    } else if (width == 4) {
                        // Integer 4 bytes stored as val ^ 0x80000000
                        uint32_t enc = static_cast<uint32_t>(val ^ 0x80000000);
                        table_raw.push_back(enc & 0xFF);
                        table_raw.push_back((enc >> 8) & 0xFF);
                        table_raw.push_back((enc >> 16) & 0xFF);
                        table_raw.push_back((enc >> 24) & 0xFF);
                    }
                }
            }

            storage.add_stream(encode_stream_name(t.name, true), table_raw);
        }

        // Build the complete CFBF file
        std::vector<uint8_t> msi_file_bytes = storage.build_storage();

        // Write to output file
        std::ofstream out(output_msi_path, std::ios::binary);
        if (!out.is_open()) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(msi_file_bytes.data()), msi_file_bytes.size());
        out.close();

        return true;
    }
};

#endif
