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
#include <openssl/pem.h>

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

    // Keypair generation (Ed25519) - Uses in-memory BIOs for cross-platform OpenSSL compatibility
    static bool generate_keypair(const std::string& privkey_path, const std::string& pubkey_path) {
        EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
        if (!pctx) return false;
        if (EVP_PKEY_keygen_init(pctx) <= 0) {
            EVP_PKEY_CTX_free(pctx);
            return false;
        }
        EVP_PKEY* pkey = nullptr;
        if (EVP_PKEY_keygen(pctx, &pkey) <= 0 || !pkey) {
            EVP_PKEY_CTX_free(pctx);
            return false;
        }
        EVP_PKEY_CTX_free(pctx);

        BIO* priv_bio = BIO_new(BIO_s_mem());
        if (!priv_bio) {
            EVP_PKEY_free(pkey);
            return false;
        }
        PEM_write_bio_PrivateKey(priv_bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
        char* priv_data = nullptr;
        long priv_len = BIO_get_mem_data(priv_bio, &priv_data);
        std::ofstream priv_file(privkey_path, std::ios::binary);
        if (!priv_file.is_open()) {
            BIO_free(priv_bio);
            EVP_PKEY_free(pkey);
            return false;
        }
        priv_file.write(priv_data, priv_len);
        priv_file.close();
        BIO_free(priv_bio);

        BIO* pub_bio = BIO_new(BIO_s_mem());
        if (!pub_bio) {
            EVP_PKEY_free(pkey);
            return false;
        }
        PEM_write_bio_PUBKEY(pub_bio, pkey);
        char* pub_data = nullptr;
        long pub_len = BIO_get_mem_data(pub_bio, &pub_data);
        std::ofstream pub_file(pubkey_path, std::ios::binary);
        if (!pub_file.is_open()) {
            BIO_free(pub_bio);
            EVP_PKEY_free(pkey);
            return false;
        }
        pub_file.write(pub_data, pub_len);
        pub_file.close();
        BIO_free(pub_bio);

        EVP_PKEY_free(pkey);
        return true;
    }

    // Sign a file using Ed25519 private key - Uses in-memory BIO
    static std::string sign_file(const std::string& file_path, const std::string& privkey_path) {
        std::ifstream kf(privkey_path, std::ios::binary);
        if (!kf.is_open()) return "";
        std::string kstr((std::istreambuf_iterator<char>(kf)), std::istreambuf_iterator<char>());
        kf.close();
        BIO* kbio = BIO_new_mem_buf(kstr.data(), (int)kstr.size());
        if (!kbio) return "";
        EVP_PKEY* pkey = PEM_read_bio_PrivateKey(kbio, nullptr, nullptr, nullptr);
        BIO_free(kbio);
        if (!pkey) return "";

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            EVP_PKEY_free(pkey);
            return "";
        }
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
        file.close();

        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
        if (!md_ctx) {
            EVP_PKEY_free(pkey);
            return "";
        }

        if (EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, pkey) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            EVP_PKEY_free(pkey);
            return "";
        }

        size_t sig_len = 0;
        if (EVP_DigestSign(md_ctx, nullptr, &sig_len, data.data(), data.size()) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            EVP_PKEY_free(pkey);
            return "";
        }

        std::vector<unsigned char> sig(sig_len);
        if (EVP_DigestSign(md_ctx, sig.data(), &sig_len, data.data(), data.size()) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            EVP_PKEY_free(pkey);
            return "";
        }

        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);

        return to_hex(sig.data(), sig_len);
    }

    // Verify file signature using Ed25519 public key - Uses in-memory BIO
    static bool verify_file_signature(const std::string& file_path, const std::string& hex_signature, const std::string& pubkey_path) {
        std::ifstream pf(pubkey_path, std::ios::binary);
        if (!pf.is_open()) return false;
        std::string pstr((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
        pf.close();
        BIO* pbio = BIO_new_mem_buf(pstr.data(), (int)pstr.size());
        if (!pbio) return false;
        EVP_PKEY* pkey = PEM_read_bio_PUBKEY(pbio, nullptr, nullptr, nullptr);
        BIO_free(pbio);
        if (!pkey) return false;

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            EVP_PKEY_free(pkey);
            return false;
        }
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
        file.close();

        // Convert hex to bytes
        std::vector<unsigned char> sig;
        for (size_t i = 0; i + 1 < hex_signature.size(); i += 2) {
            std::string byte_str = hex_signature.substr(i, 2);
            unsigned char b = static_cast<unsigned char>(std::stoul(byte_str, nullptr, 16));
            sig.push_back(b);
        }

        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
        if (!md_ctx) {
            EVP_PKEY_free(pkey);
            return false;
        }

        if (EVP_DigestVerifyInit(md_ctx, nullptr, nullptr, nullptr, pkey) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            EVP_PKEY_free(pkey);
            return false;
        }

        int ret = EVP_DigestVerify(md_ctx, sig.data(), sig.size(), data.data(), data.size());
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);

        return ret == 1;
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
