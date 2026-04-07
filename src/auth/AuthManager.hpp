#pragma once

#include <optional>
#include <string>

namespace avacli {

struct AuthTokens {
    std::string clusterKey;
};

class AuthManager {
public:
    static std::string authFilePath();
    static bool save(const AuthTokens& tokens);
    static std::optional<AuthTokens> load();
    static bool clear();
};

} // namespace avacli
