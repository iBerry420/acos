#include "config/VaultStorage.hpp"
#include "platform/Paths.hpp"
#include "platform/PortableTime.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace avacli {

namespace fs = std::filesystem;

namespace {

constexpr int SALT_LEN = 16;
constexpr int IV_LEN = 12;
constexpr int TAG_LEN = 16;
constexpr int KEY_LEN = 32;
constexpr int PBKDF2_ITERATIONS = 100000;

std::string base64Encode(const unsigned char* data, size_t len) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len * 4 / 3) + 4);
    for (size_t i = 0; i < len;) {
        uint32_t a = i < len ? data[i++] : 0;
        uint32_t b = i < len ? data[i++] : 0;
        uint32_t c = i < len ? data[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        out += b64[(triple >> 18) & 0x3f];
        out += b64[(triple >> 12) & 0x3f];
        out += (i > len + 1) ? '=' : b64[(triple >> 6) & 0x3f];
        out += (i > len) ? '=' : b64[triple & 0x3f];
    }
    return out;
}

std::string base64Decode(const std::string& encoded) {
    static const int lookup[] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51
    };
    std::string out;
    out.reserve(encoded.size() * 3 / 4);
    uint32_t buf = 0;
    int bits = 0;
    for (char c : encoded) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        int val = (c >= 0 && c < 128) ? lookup[(unsigned char)c] : -1;
        if (val < 0) continue;
        buf = (buf << 6) | val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((buf >> bits) & 0xFF);
        }
    }
    return out;
}

bool deriveKey(const std::string& password, const unsigned char* salt,
               unsigned char* outKey) {
    return PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                              salt, SALT_LEN, PBKDF2_ITERATIONS,
                              EVP_sha256(), KEY_LEN, outKey) == 1;
}

std::string currentTimestampStr() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    utcTmFromTime(t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

nlohmann::json loadJsonFile(const std::string& path) {
    if (!fs::exists(path)) return nlohmann::json::object();
    try {
        std::ifstream f(path);
        return nlohmann::json::parse(f);
    } catch (...) {
        return nlohmann::json::object();
    }
}

void saveJsonFile(const std::string& path, const nlohmann::json& j) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    f << j.dump(2);
#ifndef _WIN32
    chmod(path.c_str(), 0600);
#endif
}

} // namespace

std::string VaultStorage::vaultDir() {
    return (fs::path(userHomeDirectoryOrFallback()) / ".avacli").string();
}

std::string VaultStorage::vaultPath() {
    return (fs::path(vaultDir()) / "vault.json").string();
}

std::string VaultStorage::metaPath() {
    return (fs::path(vaultDir()) / "vault_meta.json").string();
}

std::string VaultStorage::registryPath() {
    return (fs::path(vaultDir()) / "api_registry.json").string();
}

bool VaultStorage::exists() {
    return fs::exists(vaultPath());
}

bool VaultStorage::initialize(const std::string& password) {
    if (exists()) return true;
    fs::create_directories(vaultDir());

    nlohmann::json vault;
    vault["version"] = 1;
    vault["entries"] = nlohmann::json::object();

    // Generate and store salt
    unsigned char salt[SALT_LEN];
    RAND_bytes(salt, SALT_LEN);
    vault["salt"] = base64Encode(salt, SALT_LEN);

    // Store a verification token so we can check the password later
    std::string verifyPlain = "avacli_vault_v1";
    vault["verify"] = encrypt(verifyPlain, password);

    saveJsonFile(vaultPath(), vault);
    spdlog::info("VaultStorage: initialized at {}", vaultPath());
    return true;
}

std::string VaultStorage::encrypt(const std::string& plaintext, const std::string& password) {
    unsigned char salt[SALT_LEN];
    RAND_bytes(salt, SALT_LEN);

    unsigned char key[KEY_LEN];
    if (!deriveKey(password, salt, key)) return "";

    unsigned char iv[IV_LEN];
    RAND_bytes(iv, IV_LEN);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";

    std::string result;
    int len = 0;
    std::vector<unsigned char> ciphertext(plaintext.size() + 16);
    unsigned char tag[TAG_LEN];

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv) != 1 ||
        EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.c_str()),
                          static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    int cipherLen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    cipherLen += len;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag);
    EVP_CIPHER_CTX_free(ctx);

    // Format: salt(16) + iv(12) + tag(16) + ciphertext
    std::vector<unsigned char> packed;
    packed.insert(packed.end(), salt, salt + SALT_LEN);
    packed.insert(packed.end(), iv, iv + IV_LEN);
    packed.insert(packed.end(), tag, tag + TAG_LEN);
    packed.insert(packed.end(), ciphertext.begin(), ciphertext.begin() + cipherLen);

    return base64Encode(packed.data(), packed.size());
}

std::optional<std::string> VaultStorage::decrypt(const std::string& ciphertext, const std::string& password) {
    std::string raw = base64Decode(ciphertext);
    if (raw.size() < SALT_LEN + IV_LEN + TAG_LEN + 1) return std::nullopt;

    const auto* data = reinterpret_cast<const unsigned char*>(raw.data());
    const unsigned char* salt = data;
    const unsigned char* iv = data + SALT_LEN;
    const unsigned char* tag = data + SALT_LEN + IV_LEN;
    const unsigned char* enc = data + SALT_LEN + IV_LEN + TAG_LEN;
    int encLen = static_cast<int>(raw.size()) - SALT_LEN - IV_LEN - TAG_LEN;

    unsigned char key[KEY_LEN];
    if (!deriveKey(password, salt, key)) return std::nullopt;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return std::nullopt;

    std::vector<unsigned char> plaintext(encLen + 16);
    int len = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv) != 1 ||
        EVP_DecryptUpdate(ctx, plaintext.data(), &len, enc, encLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    int plainLen = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN,
                             const_cast<unsigned char*>(tag)) != 1 ||
        EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    plainLen += len;
    EVP_CIPHER_CTX_free(ctx);

    return std::string(reinterpret_cast<char*>(plaintext.data()), plainLen);
}

bool VaultStorage::store(const std::string& password, const std::string& name,
                         const std::string& service, const std::string& value) {
    if (!exists()) initialize(password);

    auto vault = loadJsonFile(vaultPath());
    std::string encrypted = encrypt(value, password);
    if (encrypted.empty()) return false;

    std::string now = currentTimestampStr();
    vault["entries"][name] = {
        {"service", service},
        {"encrypted_value", encrypted},
        {"created_at", vault["entries"].contains(name) ?
            vault["entries"][name].value("created_at", now) : now},
        {"updated_at", now}
    };

    saveJsonFile(vaultPath(), vault);

    // Update metadata (no secrets)
    auto meta = loadJsonFile(metaPath());
    meta[name] = {{"service", service}, {"updated_at", now}};
    saveJsonFile(metaPath(), meta);

    spdlog::info("VaultStorage: stored key '{}'", name);
    return true;
}

std::optional<std::string> VaultStorage::retrieve(const std::string& password, const std::string& name) {
    auto vault = loadJsonFile(vaultPath());
    if (!vault.contains("entries") || !vault["entries"].contains(name))
        return std::nullopt;

    std::string encrypted = vault["entries"][name].value("encrypted_value", "");
    if (encrypted.empty()) return std::nullopt;

    return decrypt(encrypted, password);
}

bool VaultStorage::remove(const std::string& password, const std::string& name) {
    auto vault = loadJsonFile(vaultPath());
    if (!vault.contains("entries") || !vault["entries"].contains(name))
        return false;

    vault["entries"].erase(name);
    saveJsonFile(vaultPath(), vault);

    auto meta = loadJsonFile(metaPath());
    if (meta.contains(name)) meta.erase(name);
    saveJsonFile(metaPath(), meta);

    spdlog::info("VaultStorage: removed key '{}'", name);
    return true;
}

std::vector<VaultEntry> VaultStorage::listEntries() {
    std::vector<VaultEntry> result;
    auto vault = loadJsonFile(vaultPath());
    if (!vault.contains("entries")) return result;

    for (auto& [key, val] : vault["entries"].items()) {
        VaultEntry e;
        e.name = key;
        e.service = val.value("service", "");
        e.created_at = val.value("created_at", "");
        e.updated_at = val.value("updated_at", "");
        result.push_back(std::move(e));
    }
    return result;
}

// ── API Registry ──

bool VaultStorage::saveApiRegistry(const std::string& slug, const ApiRegistryEntry& entry) {
    auto reg = loadJsonFile(registryPath());
    std::string now = currentTimestampStr();

    reg[slug] = {
        {"name", entry.name},
        {"base_url", entry.base_url},
        {"auth_type", entry.auth_type},
        {"auth_config", entry.auth_config},
        {"vault_key_name", entry.vault_key_name},
        {"endpoints", entry.endpoints},
        {"rate_limit", entry.rate_limit},
        {"documentation_url", entry.documentation_url},
        {"research_notes", entry.research_notes},
        {"status", entry.status.empty() ? "configured" : entry.status},
        {"created_at", reg.contains(slug) ? reg[slug].value("created_at", now) : now},
        {"updated_at", now}
    };

    saveJsonFile(registryPath(), reg);
    spdlog::info("VaultStorage: saved API registry entry '{}'", slug);
    return true;
}

std::optional<ApiRegistryEntry> VaultStorage::getApiRegistry(const std::string& slug) {
    auto reg = loadJsonFile(registryPath());
    if (!reg.contains(slug)) return std::nullopt;

    auto& j = reg[slug];
    ApiRegistryEntry e;
    e.slug = slug;
    e.name = j.value("name", "");
    e.base_url = j.value("base_url", "");
    e.auth_type = j.value("auth_type", "");
    e.auth_config = j.value("auth_config", nlohmann::json::object());
    e.vault_key_name = j.value("vault_key_name", "");
    e.endpoints = j.value("endpoints", nlohmann::json::array());
    e.rate_limit = j.value("rate_limit", nlohmann::json::object());
    e.documentation_url = j.value("documentation_url", "");
    e.research_notes = j.value("research_notes", "");
    e.status = j.value("status", "configured");
    e.created_at = j.value("created_at", "");
    e.updated_at = j.value("updated_at", "");
    return e;
}

bool VaultStorage::removeApiRegistry(const std::string& slug) {
    auto reg = loadJsonFile(registryPath());
    if (!reg.contains(slug)) return false;
    reg.erase(slug);
    saveJsonFile(registryPath(), reg);
    return true;
}

std::vector<ApiRegistryEntry> VaultStorage::listApiRegistry() {
    std::vector<ApiRegistryEntry> result;
    auto reg = loadJsonFile(registryPath());
    for (auto& [slug, j] : reg.items()) {
        ApiRegistryEntry e;
        e.slug = slug;
        e.name = j.value("name", "");
        e.base_url = j.value("base_url", "");
        e.auth_type = j.value("auth_type", "");
        e.auth_config = j.value("auth_config", nlohmann::json::object());
        e.vault_key_name = j.value("vault_key_name", "");
        e.endpoints = j.value("endpoints", nlohmann::json::array());
        e.rate_limit = j.value("rate_limit", nlohmann::json::object());
        e.documentation_url = j.value("documentation_url", "");
        e.research_notes = j.value("research_notes", "");
        e.status = j.value("status", "configured");
        e.created_at = j.value("created_at", "");
        e.updated_at = j.value("updated_at", "");
        result.push_back(std::move(e));
    }
    return result;
}

std::optional<std::string> VaultStorage::resolveApiKey(const std::string& password,
                                                        const std::string& registrySlug) {
    auto entry = getApiRegistry(registrySlug);
    if (!entry || entry->vault_key_name.empty()) return std::nullopt;
    return retrieve(password, entry->vault_key_name);
}

} // namespace avacli
