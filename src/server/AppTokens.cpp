#include "server/AppTokens.hpp"
#include "db/Database.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/rand.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace avacli {

namespace AppTokens {

std::string generateToken() {
    unsigned char buf[32];
    if (RAND_bytes(buf, sizeof(buf)) != 1) {
        // Should not fail on any sane platform; if it does, refuse rather than
        // weaken the token.
        throw std::runtime_error("RAND_bytes failed");
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char b : buf) oss << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

std::string ensureForAppId(const std::string& appId) {
    auto& db = Database::instance();
    auto row = db.queryOne("SELECT agent_token FROM apps WHERE id = ?1", {appId});
    if (row.is_null()) throw std::runtime_error("app not found: " + appId);
    std::string existing = row.value("agent_token", "");
    if (!existing.empty()) return existing;

    std::string tok = generateToken();
    db.execute("UPDATE apps SET agent_token = ?1 WHERE id = ?2", {tok, appId});
    spdlog::info("Minted agent_token for app id={}", appId);
    return tok;
}

nlohmann::json findAppByToken(const std::string& token) {
    if (token.empty()) return nullptr;
    auto& db = Database::instance();
    auto row = db.queryOne(
        "SELECT id, slug, name, db_enabled, ai_enabled, db_size_cap_mb "
        "FROM apps WHERE agent_token = ?1 AND status = 'active'",
        {token});
    return row;
}

bool secureEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); i++) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

} // namespace AppTokens

// ── Request auth ─────────────────────────────────────────────────────────

namespace {

// Extract the bearer token after `Authorization: Bearer `. Empty on miss.
std::string bearerToken(const httplib::Request& req) {
    auto h = req.get_header_value("Authorization");
    if (h.size() > 7 && h.substr(0, 7) == "Bearer ") return h.substr(7);
    return "";
}

// Parse the path component of a Referer. Returns empty if header missing
// or URL unparseable. Only the path matters — hostname checks would need
// knowledge of the public domain which we don't have here; we trust that
// the browser sets Referer honestly for same-origin requests and rely on
// the bearer token as the primary credential.
std::string refererPath(const httplib::Request& req) {
    auto ref = req.get_header_value("Referer");
    if (ref.empty()) return "";
    auto schemeEnd = ref.find("://");
    size_t pathStart = 0;
    if (schemeEnd != std::string::npos) {
        auto slash = ref.find('/', schemeEnd + 3);
        if (slash == std::string::npos) return "/";
        pathStart = slash;
    }
    auto q = ref.find_first_of("?#", pathStart);
    return q == std::string::npos
        ? ref.substr(pathStart)
        : ref.substr(pathStart, q - pathStart);
}

} // namespace

AppAuthContext authenticateAppRequest(const httplib::Request& req,
                                      const std::string& slug) {
    AppAuthContext ctx;

    std::string token = bearerToken(req);
    if (token.empty()) {
        ctx.failStatus = 401;
        // Raw string with a custom JSON delimiter so `)"` inside the body
        // doesn't close the literal early.
        ctx.failBody = R"JSON({"error":"agent_token required (Authorization: Bearer ...)"})JSON";
        return ctx;
    }

    auto app = AppTokens::findAppByToken(token);
    if (app.is_null()) {
        ctx.failStatus = 401;
        ctx.failBody = R"({"error":"invalid agent_token"})";
        return ctx;
    }

    std::string actualSlug = app.value("slug", "");
    if (!AppTokens::secureEquals(actualSlug, slug)) {
        // Token valid but belongs to a different app — never leak which.
        ctx.failStatus = 403;
        ctx.failBody = R"({"error":"token does not match app"})";
        return ctx;
    }

    // Referer check: if present, must be rooted at /apps/<slug>/. Absent
    // Referer is fine (same-origin fetch, server-to-server tool calls).
    auto refPath = refererPath(req);
    if (!refPath.empty()) {
        std::string expected = "/apps/" + slug + "/";
        if (refPath.rfind(expected, 0) != 0) {
            ctx.failStatus = 403;
            ctx.failBody = R"({"error":"cross-origin request denied"})";
            return ctx;
        }
    }

    ctx.appId        = app.value("id", "");
    ctx.slug         = actualSlug;
    ctx.dbEnabled    = app.value("db_enabled", 1) != 0;
    ctx.aiEnabled    = app.value("ai_enabled", 1) != 0;
    ctx.dbSizeCapMb  = app.value("db_size_cap_mb", 0);
    ctx.ok           = true;
    return ctx;
}

} // namespace avacli
