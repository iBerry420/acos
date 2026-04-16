#include "platform/Hash.hpp"

#include <openssl/evp.h>

#include <array>
#include <cstdio>

namespace avacli {

namespace {

std::string toHex(const unsigned char* data, size_t len) {
    static const char* kDigits = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out[2 * i]     = kDigits[(data[i] >> 4) & 0x0f];
        out[2 * i + 1] = kDigits[data[i] & 0x0f];
    }
    return out;
}

} // namespace

std::string sha256Hex(const std::string& data) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return {};
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    std::string out;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
        EVP_DigestUpdate(ctx, data.data(), data.size()) == 1 &&
        EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1) {
        out = toHex(digest, digestLen);
    }
    EVP_MD_CTX_free(ctx);
    return out;
}

std::string sha256File(const std::string& path) {
    if (path.empty()) return {};
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return {};

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::fclose(f);
        return {};
    }

    std::string out;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        std::fclose(f);
        return {};
    }

    std::array<unsigned char, 65536> buf{};
    size_t n = 0;
    bool ok = true;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
        if (EVP_DigestUpdate(ctx, buf.data(), n) != 1) { ok = false; break; }
    }
    std::fclose(f);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (ok && EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1) {
        out = toHex(digest, digestLen);
    }
    EVP_MD_CTX_free(ctx);
    return out;
}

std::string sha256Files(const std::vector<std::string>& paths) {
    // Stable composition: hash each file, join with '|', hash again.
    std::string joined;
    joined.reserve(paths.size() * 70);
    for (const auto& p : paths) {
        joined += sha256File(p);
        joined += '|';
    }
    return sha256Hex(joined);
}

} // namespace avacli
