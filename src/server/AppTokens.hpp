#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace avacli {

/// Helpers for per-app agent_token management (phase 3).
///
/// The token is a 32-byte hex string stored in `apps.agent_token`. It's
/// generated lazily on first access rather than at migration time so the
/// migration stays pure-SQL (no OpenSSL dep in `Database.cpp`).
namespace AppTokens {

/// Ensure the app has an agent_token; generate + persist one if missing.
/// Returns the (now-guaranteed-non-empty) token.
std::string ensureForAppId(const std::string& appId);

/// Resolve `agent_token` -> app row (id, slug, db_enabled, ai_enabled,
/// db_size_cap_mb). Returns nullptr on no match.
nlohmann::json findAppByToken(const std::string& token);

/// Constant-time string compare to keep token validation off timing side
/// channels — not strictly required given DB latency, but cheap.
bool secureEquals(const std::string& a, const std::string& b);

/// Generate a fresh random hex token (64 chars = 32 bytes).
std::string generateToken();

} // namespace AppTokens

/// App DB request context derived from a `Bearer <token>` header. Returned
/// from `authenticateAppRequest()` below; callers pass this to DB handlers.
struct AppAuthContext {
    std::string appId;
    std::string slug;
    bool dbEnabled{true};
    bool aiEnabled{true};
    int dbSizeCapMb{0};  // 0 = inherit global setting
    bool ok{false};
    int failStatus{401};
    std::string failBody;  // JSON body to write on failure
};

/// Validate `Authorization: Bearer <agent_token>` against `apps.agent_token`
/// for the given slug. Sets `out.ok=false` with `failStatus`/`failBody`
/// populated on any failure mode (missing, wrong slug, disabled, etc.).
///
/// Also enforces the same-origin-ish Referer check: the request must either
/// have no Referer (e.g. agent-side tool calls) or a Referer whose path
/// begins with `/apps/<slug>/`. This blocks cross-origin browser fetches
/// that happen to have gotten hold of the token.
AppAuthContext authenticateAppRequest(const httplib::Request& req,
                                      const std::string& slug);

} // namespace avacli
