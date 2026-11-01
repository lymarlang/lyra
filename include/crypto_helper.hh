#ifndef CRYPTO_HELPER_HH
#define CRYPTO_HELPER_HH

#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <openssl/evp.h>

namespace fs = std::filesystem;

class CryptoHelper {
public:
    static std::string sha256(const std::string& data) {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int lengthOfHash = 0;

        EVP_MD_CTX* context = EVP_MD_CTX_new();
        if (context == nullptr) return "";

        if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) &&
            EVP_DigestUpdate(context, data.c_str(), data.size()) &&
            EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
            EVP_MD_CTX_free(context);
            return to_hex(hash, lengthOfHash);
        }

        EVP_MD_CTX_free(context);
        return "";
    }

    static std::string sha256_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int lengthOfHash = 0;

        EVP_MD_CTX* context = EVP_MD_CTX_new();
        if (context == nullptr) return "";

        if (!EVP_DigestInit_ex(context, EVP_sha256(), nullptr)) {
            EVP_MD_CTX_free(context);
            return "";
        }

        char buffer[4096];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            if (!EVP_DigestUpdate(context, buffer, file.gcount())) {
                EVP_MD_CTX_free(context);
                return "";
            }
        }

        if (EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
            EVP_MD_CTX_free(context);
            return to_hex(hash, lengthOfHash);
        }

        EVP_MD_CTX_free(context);
        return "";
    }

    static std::string sha256_dir(const std::string& dir_path) {
        fs::path root(dir_path);
        if (!fs::exists(root)) return "";

        if (fs::is_regular_file(root)) {
            return sha256_file(dir_path);
        }

        std::vector<std::string> relative_paths;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(root)) {
                if (entry.is_regular_file()) {
                    std::string rel = fs::relative(entry.path(), root).generic_string();
                    if (rel.find(".git/") != 0 && rel != ".git") {
                        relative_paths.push_back(rel);
                    }
                }
            }
        } catch (...) {
            return "";
        }

        std::sort(relative_paths.begin(), relative_paths.end());

        EVP_MD_CTX* context = EVP_MD_CTX_new();
        if (!context) return "";

        if (!EVP_DigestInit_ex(context, EVP_sha256(), nullptr)) {
            EVP_MD_CTX_free(context);
            return "";
        }

        char buffer[4096];
        for (const auto& rel : relative_paths) {
            std::string header = rel + "\n";
            EVP_DigestUpdate(context, header.c_str(), header.size());

            std::ifstream file(root / rel, std::ios::binary);
            if (file.is_open()) {
                while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                    EVP_DigestUpdate(context, buffer, file.gcount());
                }
            }
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int lengthOfHash = 0;
        if (EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
            EVP_MD_CTX_free(context);
            return to_hex(hash, lengthOfHash);
        }

        EVP_MD_CTX_free(context);
        return "";
    }

    static std::string calculate_package_hash(const std::string& path) {
        if (fs::is_regular_file(path)) {
            return sha256_file(path);
        } else if (fs::is_directory(path)) {
            return sha256_dir(path);
        }
        return "";
    }

private:
    static std::string to_hex(const unsigned char* hash, unsigned int length) {
        std::stringstream ss;
        for (unsigned int i = 0; i < length; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
};

#endif
