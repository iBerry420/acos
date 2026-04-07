#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace avacli {

struct AccountInfo {
    std::string username;
    bool is_admin = false;
    bool has_password = false;
    long long created_at = 0;
};

struct AuthResult {
    std::string username;
    bool is_admin = false;
};

class MasterKeyManager {
public:
    static std::string accountsFilePath();

    static bool isConfigured();

    /// Migrate legacy master.json into accounts.json (call once at startup)
    static void migrateFromLegacy();

    static bool setCredentials(const std::string& username, const std::string& password);
    static bool setMasterKey(const std::string& key);
    static std::string generateMasterKey();

    static bool verify(const std::string& key);
    static bool verifyCredentials(const std::string& username, const std::string& password);

    static std::optional<AuthResult> authenticateUser(const std::string& username,
                                                       const std::string& password);

    // Account management (all stored in accounts.json)
    static std::vector<AccountInfo> listAccounts();
    static bool createAccount(const std::string& username, const std::string& password, bool isAdmin = false);
    static bool deleteAccount(const std::string& username);
    static bool updateAccountPassword(const std::string& username, const std::string& newPassword);
    static bool updateAccountRole(const std::string& username, bool isAdmin);
    static bool isUserAdmin(const std::string& username);
    static bool accountExists(const std::string& username);
    static bool hasAnyPasswordSet();
    static bool accountHasPassword(const std::string& username);
    static bool renameAccount(const std::string& oldName, const std::string& newName);

    void updateSessionUsername(const std::string& oldName, const std::string& newName);

    // Session management
    std::string createSession(const std::string& username = "admin");
    bool validateSession(const std::string& token) const;
    std::string getSessionUser(const std::string& token) const;
    bool isSessionAdmin(const std::string& token) const;
    void invalidateSession(const std::string& token);
    void invalidateAllSessions();
    void loadSessionsFromDisk();

private:
    static std::string legacyMasterKeyPath();
    void saveSessionsToDisk() const;

    mutable std::mutex mu_;
    std::unordered_map<std::string, std::string> sessions_; // token -> username
};

} // namespace avacli
