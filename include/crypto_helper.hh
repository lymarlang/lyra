#ifndef CRYPTO_HELPER_HH
#define CRYPTO_HELPER_HH

#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace Lyra {

class CryptoHelper {
public:
    static std::string sha256_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";

        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        const EVP_MD* md = EVP_sha256();
        unsigned char hash[SHA256_DIGEST_LENGTH];
        unsigned int hash_len;

        EVP_DigestInit_ex(mdctx, md, nullptr);

        char buffer[4096];
        while (file.read(buffer, sizeof(buffer))) {
            EVP_DigestUpdate(mdctx, buffer, file.gcount());
        }
        EVP_DigestUpdate(mdctx, buffer, file.gcount());

        EVP_DigestFinal_ex(mdctx, hash, &hash_len);
        EVP_MD_CTX_free(mdctx);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
};

} // namespace Lyra

#endif
