#include "auth/MasterKeyManager.hpp"
#include "platform/Paths.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdlib>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace avacli {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

constexpr int SALT_LEN = 32;
constexpr int HASH_LEN = 32;
constexpr int PBKDF2_ITERATIONS = 100000;
constexpr int SESSION_TOKEN_LEN = 32;
constexpr int MASTER_KEY_GEN_LEN = 32;

std::string hexEncode(const unsigned char* data, int len) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (int i = 0; i < len; ++i) {
        out += hex[(data[i] >> 4) & 0xF];
        out += hex[data[i] & 0xF];
    }
    return out;
}

std::string hexDecode(const std::string& hex) {
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned char hi = 0, lo = 0;
        if (hex[i] >= '0' && hex[i] <= '9') hi = hex[i] - '0';
        else if (hex[i] >= 'a' && hex[i] <= 'f') hi = hex[i] - 'a' + 10;
        else if (hex[i] >= 'A' && hex[i] <= 'F') hi = hex[i] - 'A' + 10;
        if (hex[i+1] >= '0' && hex[i+1] <= '9') lo = hex[i+1] - '0';
        else if (hex[i+1] >= 'a' && hex[i+1] <= 'f') lo = hex[i+1] - 'a' + 10;
        else if (hex[i+1] >= 'A' && hex[i+1] <= 'F') lo = hex[i+1] - 'A' + 10;
        out += static_cast<char>((hi << 4) | lo);
    }
    return out;
}

bool pbkdf2Hash(const std::string& key, const unsigned char* salt,
                unsigned char* outHash) {
    return PKCS5_PBKDF2_HMAC(key.c_str(), static_cast<int>(key.size()),
                              salt, SALT_LEN, PBKDF2_ITERATIONS,
                              EVP_sha256(), HASH_LEN, outHash) == 1;
}

bool verifyAgainstEntry(const json& entry, const std::string& password) {
    std::string saltHex = entry.value("salt", "");
    std::string storedHashHex = entry.value("hash", "");
    if (saltHex.empty() || storedHashHex.empty()) return false;

    std::string saltRaw = hexDecode(saltHex);
    if (static_cast<int>(saltRaw.size()) != SALT_LEN) return false;

    unsigned char hash[HASH_LEN];
    if (!pbkdf2Hash(password, reinterpret_cast<const unsigned char*>(saltRaw.data()), hash))
        return false;

    return hexEncode(hash, HASH_LEN) == storedHashHex;
}

std::string avacliDir() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return (fs::path(home) / ".avacli").string();
}

json readAccountsFile() {
    auto path = MasterKeyManager::accountsFilePath();
    if (!fs::exists(path)) return json::array();
    try {
        std::ifstream f(path);
        auto arr = json::parse(f);
        if (arr.is_array()) return arr;
    } catch (...) {}
    return json::array();
}

bool writeAccountsFile(const json& arr) {
    auto path = MasterKeyManager::accountsFilePath();
    fs::create_directories(fs::path(path).parent_path());
    try {
        std::ofstream f(path, std::ios::trunc);
        if (!f) return false;
        f << arr.dump(2);
        f.close();
#ifndef _WIN32
        ::chmod(path.c_str(), 0600);
#endif
        return true;
    } catch (...) {
        return false;
    }
}

long long nowEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

json hashPassword(const std::string& password) {
    unsigned char salt[SALT_LEN];
    RAND_bytes(salt, SALT_LEN);
    unsigned char hash[HASH_LEN];
    if (!pbkdf2Hash(password, salt, hash)) return json();
    json j;
    j["salt"] = hexEncode(salt, SALT_LEN);
    j["hash"] = hexEncode(hash, HASH_LEN);
    return j;
}

} // anonymous namespace

// ─── File paths ──────────────────────────────────────────

std::string MasterKeyManager::accountsFilePath() {
    return (fs::path(avacliDir()) / "accounts.json").string();
}

std::string MasterKeyManager::legacyMasterKeyPath() {
    return (fs::path(avacliDir()) / "master.json").string();
}

bool MasterKeyManager::isConfigured() {
    auto accounts = readAccountsFile();
    return !accounts.empty();
}

// ─── Legacy migration ────────────────────────────────────

void MasterKeyManager::migrateFromLegacy() {
    std::string legacyPath = legacyMasterKeyPath();
    if (!fs::exists(legacyPath)) return;

    try {
        std::ifstream ifs(legacyPath);
        json master = json::parse(ifs);
        ifs.close();

        std::string username = master.value("username", "admin");
        std::string salt = master.value("salt", "");
        std::string hash = master.value("hash", "");
        long long createdAt = master.value("created_at", 0LL);

        if (salt.empty() || hash.empty()) {
            spdlog::warn("Legacy master.json has no credentials, skipping migration");
            return;
        }

        auto arr = readAccountsFile();
        for (const auto& a : arr) {
            if (a.value("username", "") == username) {
                spdlog::info("Account '{}' already exists in accounts.json, skipping migration", username);
                std::string bakPath = legacyPath + ".migrated";
                fs::rename(legacyPath, bakPath);
                return;
            }
        }

        json entry;
        entry["username"] = username;
        entry["salt"] = salt;
        entry["hash"] = hash;
        entry["is_admin"] = true;
        entry["created_at"] = createdAt > 0 ? createdAt : nowEpoch();

        arr.insert(arr.begin(), entry);
        if (writeAccountsFile(arr)) {
            std::string bakPath = legacyPath + ".migrated";
            fs::rename(legacyPath, bakPath);
            spdlog::info("Migrated legacy master account '{}' into accounts.json (old file renamed to master.json.migrated)", username);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to migrate legacy master.json: {}", e.what());
    }
}

// ─── Credential management ───────────────────────────────

bool MasterKeyManager::setCredentials(const std::string& username, const std::string& password) {
    if (password.size() < 8) {
        spdlog::error("MasterKeyManager: password must be at least 8 characters");
        return false;
    }
    if (username.empty()) {
        spdlog::error("MasterKeyManager: username required");
        return false;
    }

    if (accountExists(username)) {
        return updateAccountPassword(username, password);
    }

    return createAccount(username, password, true);
}

bool MasterKeyManager::setMasterKey(const std::string& key) {
    return setCredentials("admin", key);
}

std::string MasterKeyManager::generateMasterKey() {
    unsigned char raw[MASTER_KEY_GEN_LEN];
    RAND_bytes(raw, MASTER_KEY_GEN_LEN);
    std::string key = hexEncode(raw, MASTER_KEY_GEN_LEN);
    if (!setMasterKey(key)) return "";
    return key;
}

bool MasterKeyManager::verify(const std::string& key) {
    return verifyCredentials("", key);
}

// ─── Authentication ──────────────────────────────────────

bool MasterKeyManager::verifyCredentials(const std::string& username, const std::string& password) {
    return authenticateUser(username, password).has_value();
}

std::optional<AuthResult> MasterKeyManager::authenticateUser(
        const std::string& username, const std::string& password) {
    auto accounts = readAccountsFile();
    for (const auto& acct : accounts) {
        std::string acctUser = acct.value("username", "");
        if (acctUser.empty()) continue;
        if (!username.empty() && acctUser != username) continue;

        std::string storedHash = acct.value("hash", "");
        if (storedHash.empty()) {
            return AuthResult{acctUser, acct.value("is_admin", false)};
        }

        if (verifyAgainstEntry(acct, password)) {
            return AuthResult{acctUser, acct.value("is_admin", false)};
        }
        if (!username.empty()) return std::nullopt;
    }
    return std::nullopt;
}

// ─── Account management ─────────────────────────────────

std::vector<AccountInfo> MasterKeyManager::listAccounts() {
    std::vector<AccountInfo> result;
    auto arr = readAccountsFile();
    for (const auto& a : arr) {
        AccountInfo info;
        info.username = a.value("username", "");
        info.is_admin = a.value("is_admin", false);
        info.has_password = !a.value("hash", "").empty();
        info.created_at = a.value("created_at", 0LL);
        if (!info.username.empty()) result.push_back(info);
    }
    return result;
}

bool MasterKeyManager::createAccount(const std::string& username,
                                      const std::string& password, bool isAdmin) {
    if (username.empty()) {
        spdlog::error("MasterKeyManager: username required");
        return false;
    }
    if (!password.empty() && password.size() < 8) {
        spdlog::error("MasterKeyManager: password min 8 chars");
        return false;
    }

    auto arr = readAccountsFile();
    for (const auto& a : arr) {
        if (a.value("username", "") == username) {
            spdlog::error("MasterKeyManager: account '{}' already exists", username);
            return false;
        }
    }

    json entry;
    entry["username"] = username;
    entry["is_admin"] = isAdmin;
    entry["created_at"] = nowEpoch();

    if (password.empty()) {
        entry["salt"] = "";
        entry["hash"] = "";
    } else {
        auto hashed = hashPassword(password);
        if (hashed.empty()) return false;
        entry["salt"] = hashed["salt"];
        entry["hash"] = hashed["hash"];
    }

    arr.push_back(entry);
    if (!writeAccountsFile(arr)) return false;

    spdlog::info("MasterKeyManager: account '{}' created (admin={}, has_password={})",
                 username, isAdmin, !password.empty());
    return true;
}

bool MasterKeyManager::deleteAccount(const std::string& username) {
    auto arr = readAccountsFile();
    json out = json::array();
    bool found = false;
    for (const auto& a : arr) {
        if (a.value("username", "") == username) {
            found = true;
        } else {
            out.push_back(a);
        }
    }
    if (!found) return true;
    if (!writeAccountsFile(out)) return false;
    spdlog::info("MasterKeyManager: account '{}' deleted", username);
    return true;
}

bool MasterKeyManager::updateAccountPassword(const std::string& username,
                                              const std::string& newPassword) {
    if (newPassword.size() < 8) {
        spdlog::error("MasterKeyManager: password min 8 chars");
        return false;
    }

    auto arr = readAccountsFile();
    bool found = false;
    for (auto& a : arr) {
        if (a.value("username", "") == username) {
            auto hashed = hashPassword(newPassword);
            if (hashed.empty()) return false;
            a["salt"] = hashed["salt"];
            a["hash"] = hashed["hash"];
            found = true;
            break;
        }
    }
    if (!found) return false;
    if (!writeAccountsFile(arr)) return false;
    spdlog::info("MasterKeyManager: password updated for '{}'", username);
    return true;
}

bool MasterKeyManager::updateAccountRole(const std::string& username, bool isAdmin) {
    auto arr = readAccountsFile();
    bool found = false;
    for (auto& a : arr) {
        if (a.value("username", "") == username) {
            a["is_admin"] = isAdmin;
            found = true;
            break;
        }
    }
    if (!found) return false;
    if (!writeAccountsFile(arr)) return false;
    spdlog::info("MasterKeyManager: role updated for '{}' (admin={})", username, isAdmin);
    return true;
}

bool MasterKeyManager::isUserAdmin(const std::string& username) {
    if (username.empty()) return false;
    auto arr = readAccountsFile();
    for (const auto& a : arr) {
        if (a.value("username", "") == username)
            return a.value("is_admin", false);
    }
    return false;
}

bool MasterKeyManager::accountExists(const std::string& username) {
    auto arr = readAccountsFile();
    for (const auto& a : arr) {
        if (a.value("username", "") == username) return true;
    }
    return false;
}

bool MasterKeyManager::hasAnyPasswordSet() {
    auto arr = readAccountsFile();
    for (const auto& a : arr) {
        if (!a.value("hash", "").empty()) return true;
    }
    return false;
}

bool MasterKeyManager::accountHasPassword(const std::string& username) {
    auto arr = readAccountsFile();
    for (const auto& a : arr) {
        if (a.value("username", "") == username)
            return !a.value("hash", "").empty();
    }
    return false;
}

bool MasterKeyManager::renameAccount(const std::string& oldName, const std::string& newName) {
    if (oldName.empty() || newName.empty()) return false;
    if (oldName == newName) return true;
    auto arr = readAccountsFile();
    bool foundOld = false;
    for (const auto& a : arr) {
        if (a.value("username", "") == newName) {
            spdlog::error("MasterKeyManager: rename target '{}' already exists", newName);
            return false;
        }
        if (a.value("username", "") == oldName) foundOld = true;
    }
    if (!foundOld) return false;
    for (auto& a : arr) {
        if (a.value("username", "") == oldName) {
            a["username"] = newName;
            break;
        }
    }
    if (!writeAccountsFile(arr)) return false;
    spdlog::info("MasterKeyManager: renamed '{}' -> '{}'", oldName, newName);
    return true;
}

void MasterKeyManager::updateSessionUsername(const std::string& oldName, const std::string& newName) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& [tok, user] : sessions_) {
        if (user == oldName) user = newName;
    }
    saveSessionsToDisk();
}

// ─── Session management ──────────────────────────────────

static std::string sessionsFilePath() {
    return (fs::path(avacliDir()) / "sessions.json").string();
}

void MasterKeyManager::loadSessionsFromDisk() {
    std::lock_guard<std::mutex> lock(mu_);
    std::string path = sessionsFilePath();
    if (!fs::exists(path)) return;
    try {
        std::ifstream ifs(path);
        json j = json::parse(ifs);
        if (j.is_object()) {
            for (auto& [tok, user] : j.items()) {
                sessions_[tok] = user.get<std::string>();
            }
        } else if (j.is_array()) {
            for (auto& tok : j) {
                if (tok.is_string())
                    sessions_[tok.get<std::string>()] = "admin";
            }
        }
        spdlog::info("MasterKeyManager: loaded {} sessions from disk", sessions_.size());
    } catch (const std::exception& e) {
        spdlog::warn("MasterKeyManager: failed to load sessions: {}", e.what());
    }
}

void MasterKeyManager::saveSessionsToDisk() const {
    std::string path = sessionsFilePath();
    try {
        json obj = json::object();
        for (auto& [tok, user] : sessions_) obj[tok] = user;
        std::ofstream ofs(path, std::ios::trunc);
        ofs << obj.dump();
        ofs.close();
#ifndef _WIN32
        ::chmod(path.c_str(), 0600);
#endif
    } catch (const std::exception& e) {
        spdlog::warn("MasterKeyManager: failed to save sessions: {}", e.what());
    }
}

std::string MasterKeyManager::createSession(const std::string& username) {
    unsigned char raw[SESSION_TOKEN_LEN];
    RAND_bytes(raw, SESSION_TOKEN_LEN);
    std::string token = hexEncode(raw, SESSION_TOKEN_LEN);

    std::lock_guard<std::mutex> lock(mu_);
    sessions_[token] = username;
    saveSessionsToDisk();
    return token;
}

bool MasterKeyManager::validateSession(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mu_);
    return sessions_.count(token) > 0;
}

std::string MasterKeyManager::getSessionUser(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = sessions_.find(token);
    return it != sessions_.end() ? it->second : "";
}

bool MasterKeyManager::isSessionAdmin(const std::string& token) const {
    std::string user = getSessionUser(token);
    if (user.empty()) return false;
    return isUserAdmin(user);
}

void MasterKeyManager::invalidateSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mu_);
    sessions_.erase(token);
    saveSessionsToDisk();
}

void MasterKeyManager::invalidateAllSessions() {
    std::lock_guard<std::mutex> lock(mu_);
    sessions_.clear();
    saveSessionsToDisk();
}

} // namespace avacli
