#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>

namespace avacli {

struct VaultEntry {
    std::string name;
    std::string service;
    std::string encrypted_value;
    std::string created_at;
    std::string updated_at;
};

struct ApiRegistryEntry {
    std::string slug;
    std::string name;
    std::string base_url;
    std::string auth_type;       // bearer, api_key_header, api_key_query, basic, custom
    nlohmann::json auth_config;  // {header_name, query_param, token_prefix, ...}
    std::string vault_key_name;
    nlohmann::json endpoints;    // [{method, path, params, description}]
    nlohmann::json rate_limit;
    std::string documentation_url;
    std::string research_notes;
    std::string status;          // discovered, configured, tested, active
    std::string created_at;
    std::string updated_at;
};

/// AES-256-GCM encrypted vault for API keys and secrets.
/// Storage: ~/.avacli/vault.enc (binary) + ~/.avacli/vault.meta.json (non-secret metadata)
/// Encryption key: PBKDF2(user_password, salt) or machine-derived key.
class VaultStorage {
public:
    static std::string vaultDir();
    static std::string vaultPath();
    static std::string metaPath();
    static std::string registryPath();

    /// Initialize the vault with a password. Creates the vault file if it doesn't exist.
    static bool initialize(const std::string& password);

    /// Check if the vault exists.
    static bool exists();

    /// Store a key-value pair. Encrypts the value with AES-256-GCM.
    static bool store(const std::string& password, const std::string& name,
                      const std::string& service, const std::string& value);

    /// Retrieve a decrypted value by name.
    static std::optional<std::string> retrieve(const std::string& password, const std::string& name);

    /// Remove a vault entry.
    static bool remove(const std::string& password, const std::string& name);

    /// List all vault entry names (no secrets).
    static std::vector<VaultEntry> listEntries();

    /// Encrypt a single value (returns base64-encoded IV+tag+ciphertext).
    static std::string encrypt(const std::string& plaintext, const std::string& password);

    /// Decrypt a single value.
    static std::optional<std::string> decrypt(const std::string& ciphertext, const std::string& password);

    // ── API Registry ──

    static bool saveApiRegistry(const std::string& slug, const ApiRegistryEntry& entry);
    static std::optional<ApiRegistryEntry> getApiRegistry(const std::string& slug);
    static bool removeApiRegistry(const std::string& slug);
    static std::vector<ApiRegistryEntry> listApiRegistry();

    /// Resolve an API key for a registered API (decrypt from vault using the registry's vault_key_name).
    static std::optional<std::string> resolveApiKey(const std::string& password,
                                                     const std::string& registrySlug);
};

} // namespace avacli
