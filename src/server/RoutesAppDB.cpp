#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/AppTokens.hpp"
#include "server/LogBuffer.hpp"
#include "db/Database.hpp"
#include "db/AppDatabase.hpp"
#include "config/PlatformSettings.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace avacli {

namespace fs = std::filesystem;

namespace {

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Record a usage entry for the 24h rolling quota accounting. We don't
// enforce call-count quotas in phase 3 — this is the seam for that —
// but we log writes + reads for observability today.
void recordUsage(const std::string& appId, const std::string& kind, int64_t bytes) {
    try {
        Database::instance().execute(
            "INSERT INTO app_usage (app_id, timestamp, kind, bytes) VALUES (?1, ?2, ?3, ?4)",
            {appId, std::to_string(nowMs()), kind, std::to_string(bytes)});
    } catch (...) {
        // Usage recording must never fail the actual query.
    }
}

// Parse the `params` field from the POST body. Accept `null`, missing,
// array, or object (object is passed through as the named-param case below).
// Returns an array of JSON values aligned to positional ?1, ?2, ... binding.
std::vector<nlohmann::json> parsePositionalParams(const nlohmann::json& body) {
    std::vector<nlohmann::json> out;
    if (!body.contains("params")) return out;
    const auto& p = body["params"];
    if (p.is_null()) return out;
    if (p.is_array()) {
        for (const auto& v : p) out.push_back(v);
    }
    // Named params are not supported in phase 3; callers can switch to
    // positional `?1/?2/?3` binding. Trying to bind a named map would need
    // SQL-parsing on our side to map :name -> index, which isn't worth it.
    return out;
}

int resolveSizeCapMb(const AppAuthContext& auth) {
    if (auth.dbSizeCapMb > 0) return auth.dbSizeCapMb;
    return platformSettings::appsDbSizeCapMb();
}

bool looksLikeWrite(const std::string& sql) {
    // Cheap prefix check — the authorizer is the real guardrail, this is
    // just to decide whether we need to enforce the size cap BEFORE exec.
    std::string head;
    for (char c : sql) {
        if (std::isspace(static_cast<unsigned char>(c)) && head.empty()) continue;
        if (head.size() >= 16) break;
        head += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (head.rfind("INSERT", 0) == 0) return true;
    if (head.rfind("UPDATE", 0) == 0) return true;
    if (head.rfind("DELETE", 0) == 0) return true;
    if (head.rfind("CREATE", 0) == 0) return true;
    if (head.rfind("DROP",   0) == 0) return true;
    if (head.rfind("ALTER",  0) == 0) return true;
    if (head.rfind("REPLACE",0) == 0) return true;
    return false;
}

// ── Whitelisted main-DB views (2.6) ──────────────────────────────────────
// Apps never send raw SQL against avacli.db. Instead they call a view by
// name; we map the name to a prepared SQL string and bind positional
// params exactly as received. Everything here is strict-SELECT, no writes.
//
// Adding a new view is a one-line change here — design views to be
// privacy-safe (no emails, no tokens, no secrets).
using ViewQuery = std::string;
const std::unordered_map<std::string, ViewQuery>& mainDbViews() {
    static const std::unordered_map<std::string, ViewQuery> V = {
        {"articles_public",
            "SELECT id, title, summary, tags, created_at "
            "FROM articles ORDER BY created_at DESC LIMIT 500"},
        {"apps_directory",
            "SELECT slug, name, description, icon_url "
            "FROM apps WHERE status = 'active' ORDER BY updated_at DESC LIMIT 500"},
        {"my_app",
            "SELECT id, slug, name, description, icon_url, status, created_at, updated_at "
            "FROM apps WHERE slug = ?1 LIMIT 1"},
    };
    return V;
}

} // namespace

void registerAppDBRoutes(httplib::Server& svr, ServerContext /*ctx*/) {

    // POST /api/apps/:slug/db/query  body {sql, params?} -> {rows, count}
    svr.Post(R"(/api/apps/([^/]+)/db/query)",
        [](const httplib::Request& req, httplib::Response& res) {
            std::string slug = req.matches[1];

            auto auth = authenticateAppRequest(req, slug);
            if (!auth.ok) {
                res.status = auth.failStatus;
                res.set_content(auth.failBody, "application/json");
                return;
            }
            if (!auth.dbEnabled) {
                res.status = 403;
                res.set_content(R"({"error":"db disabled for this app"})", "application/json");
                return;
            }

            nlohmann::json body;
            try { body = nlohmann::json::parse(req.body); } catch (...) {
                res.status = 400;
                res.set_content(R"({"error":"invalid JSON"})", "application/json");
                return;
            }
            std::string sql = body.value("sql", "");
            if (sql.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"sql required"})", "application/json");
                return;
            }
            auto params = parsePositionalParams(body);

            try {
                auto& db = AppDatabase::forSlug(slug);
                auto rows = db.query(sql, params);
                nlohmann::json out;
                out["rows"] = rows;
                out["count"] = rows.size();
                res.set_content(out.dump(), "application/json");
                recordUsage(auth.appId, "db_read", static_cast<int64_t>(out.dump().size()));
            } catch (const std::exception& e) {
                // SQLITE_AUTH errors bubble up here as runtime_error — render
                // a more specific code so apps can detect sandbox rejection.
                std::string what = e.what();
                bool denied = what.find("not authorized") != std::string::npos
                           || what.find("authorized") != std::string::npos;
                res.status = denied ? 403 : 500;
                nlohmann::json err;
                err["error"] = what;
                if (denied) err["code"] = "sandbox_denied";
                res.set_content(err.dump(), "application/json");
            }
        });

    // POST /api/apps/:slug/db/execute  body {sql, params?} -> {changes, last_insert_rowid}
    svr.Post(R"(/api/apps/([^/]+)/db/execute)",
        [](const httplib::Request& req, httplib::Response& res) {
            std::string slug = req.matches[1];

            auto auth = authenticateAppRequest(req, slug);
            if (!auth.ok) {
                res.status = auth.failStatus;
                res.set_content(auth.failBody, "application/json");
                return;
            }
            if (!auth.dbEnabled) {
                res.status = 403;
                res.set_content(R"({"error":"db disabled for this app"})", "application/json");
                return;
            }

            nlohmann::json body;
            try { body = nlohmann::json::parse(req.body); } catch (...) {
                res.status = 400;
                res.set_content(R"({"error":"invalid JSON"})", "application/json");
                return;
            }
            std::string sql = body.value("sql", "");
            if (sql.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"sql required"})", "application/json");
                return;
            }
            auto params = parsePositionalParams(body);

            try {
                auto& db = AppDatabase::forSlug(slug);

                // Enforce size cap natively via SQLite's PRAGMA max_page_count.
                // This is WAL-aware — SQLite returns SQLITE_FULL when a write
                // would grow the db past the limit, and the statement rolls
                // back atomically. Caveat: a single row bigger than one page
                // can still push past the cap by up to one overflow-chain
                // allocation; the cap is a guard-rail, not an exact quota.
                int capMb = resolveSizeCapMb(auth);
                if (looksLikeWrite(sql)) {
                    db.setPageLimitForCap(capMb);
                }

                auto out = db.execute(sql, params);
                res.set_content(out.dump(), "application/json");
                recordUsage(auth.appId, "db_write", static_cast<int64_t>(sql.size()));
            } catch (const std::exception& e) {
                std::string what = e.what();
                // SQLITE_FULL bubbles up as "database or disk is full".
                if (what.find("database or disk is full") != std::string::npos) {
                    int capMb = resolveSizeCapMb(auth);
                    res.status = 507; // Insufficient Storage
                    nlohmann::json err;
                    err["error"] = "db_quota_exceeded";
                    err["code"] = "quota_exceeded";
                    err["limit_mb"] = capMb;
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                bool denied = what.find("not authorized") != std::string::npos;
                res.status = denied ? 403 : 500;
                nlohmann::json err;
                err["error"] = what;
                if (denied) err["code"] = "sandbox_denied";
                res.set_content(err.dump(), "application/json");
            }
        });

    // GET /api/apps/:slug/db/schema
    svr.Get(R"(/api/apps/([^/]+)/db/schema)",
        [](const httplib::Request& req, httplib::Response& res) {
            std::string slug = req.matches[1];

            auto auth = authenticateAppRequest(req, slug);
            if (!auth.ok) {
                res.status = auth.failStatus;
                res.set_content(auth.failBody, "application/json");
                return;
            }
            if (!auth.dbEnabled) {
                res.status = 403;
                res.set_content(R"({"error":"db disabled for this app"})", "application/json");
                return;
            }
            try {
                auto& db = AppDatabase::forSlug(slug);
                auto s = db.schema();
                int capMb = resolveSizeCapMb(auth);
                s["size_bytes"] = db.diskBytes();
                s["size_cap_mb"] = capMb;
                res.set_content(s.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
            }
        });

    // GET /api/apps/:slug/db/export -> raw SQLite file download
    svr.Get(R"(/api/apps/([^/]+)/db/export)",
        [](const httplib::Request& req, httplib::Response& res) {
            std::string slug = req.matches[1];

            auto auth = authenticateAppRequest(req, slug);
            if (!auth.ok) {
                res.status = auth.failStatus;
                res.set_content(auth.failBody, "application/json");
                return;
            }
            if (!auth.dbEnabled) {
                res.status = 403;
                res.set_content(R"({"error":"db disabled for this app"})", "application/json");
                return;
            }

            try {
                auto& db = AppDatabase::forSlug(slug);
                std::string path = db.filePath();
                if (!fs::exists(path)) {
                    res.status = 404;
                    res.set_content(R"({"error":"no db file yet"})", "application/json");
                    return;
                }
                std::ifstream ifs(path, std::ios::binary);
                std::string buf((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
                res.set_header("Content-Disposition",
                    "attachment; filename=\"" + slug + ".db\"");
                res.set_content(std::move(buf), "application/x-sqlite3");
                recordUsage(auth.appId, "db_read", static_cast<int64_t>(buf.size()));
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
            }
        });

    // POST /api/apps/:slug/main/query  body {view, params?}
    // Whitelisted read-only access to avacli.db views.
    svr.Post(R"(/api/apps/([^/]+)/main/query)",
        [](const httplib::Request& req, httplib::Response& res) {
            std::string slug = req.matches[1];

            auto auth = authenticateAppRequest(req, slug);
            if (!auth.ok) {
                res.status = auth.failStatus;
                res.set_content(auth.failBody, "application/json");
                return;
            }

            nlohmann::json body;
            try { body = nlohmann::json::parse(req.body); } catch (...) {
                res.status = 400;
                res.set_content(R"({"error":"invalid JSON"})", "application/json");
                return;
            }
            std::string view = body.value("view", "");
            const auto& views = mainDbViews();
            auto it = views.find(view);
            if (it == views.end()) {
                res.status = 400;
                nlohmann::json err;
                err["error"] = "unknown view";
                err["available"] = nlohmann::json::array();
                for (const auto& kv : views) err["available"].push_back(kv.first);
                res.set_content(err.dump(), "application/json");
                return;
            }

            auto params = parsePositionalParams(body);
            // `my_app` is scoped to the calling app — inject the slug as
            // the first bound param regardless of what the caller sent,
            // so an app can't peek at another app's row.
            if (view == "my_app") {
                params.clear();
                params.push_back(slug);
            }

            // Bind through Database::query which already handles string
            // params — convert json positional args to strings.
            try {
                std::vector<std::string> sparams;
                for (const auto& v : params) {
                    sparams.push_back(v.is_string() ? v.get<std::string>() : v.dump());
                }
                auto rows = Database::instance().query(it->second, sparams);
                nlohmann::json out;
                out["rows"] = rows;
                out["count"] = rows.size();
                out["view"] = view;
                res.set_content(out.dump(), "application/json");
                recordUsage(auth.appId, "main_read", 0);
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
            }
        });

    // ── SDK script served to the app's browser ──────────────────────────
    // GET /apps/:slug/_sdk.js — templated JS with the slug + token baked in.
    // No auth required on this route itself; the token IS the auth.
    // Cache-Control: no-store so token rotation takes immediate effect.
    //
    // This is served alongside the app bundle under `/apps/:slug/*`, NOT
    // under `/api/`, so it's unaffected by the master-key middleware.
    svr.Get(R"(/apps/([^/]+)/_sdk\.js)",
        [](const httplib::Request& req, httplib::Response& res) {
            std::string slug = req.matches[1];

            auto app = Database::instance().queryOne(
                "SELECT id, agent_token, db_enabled, ai_enabled "
                "FROM apps WHERE slug = ?1 AND status = 'active'",
                {slug});
            if (app.is_null()) {
                res.status = 404;
                res.set_content("// avacli sdk: app not found\n", "application/javascript");
                return;
            }

            // Mint a token on first SDK fetch if the app doesn't have one yet.
            std::string token = app.value("agent_token", "");
            if (token.empty()) {
                try {
                    token = AppTokens::ensureForAppId(app["id"].get<std::string>());
                } catch (const std::exception& e) {
                    res.status = 500;
                    res.set_content(std::string("// sdk error: ") + e.what() + "\n",
                                    "application/javascript");
                    return;
                }
            }

            bool dbEnabled = app.value("db_enabled", 1) != 0;
            bool aiEnabled = app.value("ai_enabled", 1) != 0;

            std::string js;
            js.reserve(4096);
            js += "// avacli SDK — auto-generated, do not edit.\n";
            js += "// Docs: /platform/apps/sdk.html\n";
            js += "(function(){\n";
            js += "  var SLUG = '" + slug + "';\n";
            js += "  var TOKEN = '" + token + "';\n";
            js += "  var BASE = '/api/apps/' + SLUG;\n";
            js += "  function hdrs(){return {\n";
            js += "    'Content-Type':'application/json',\n";
            js += "    'Authorization':'Bearer '+TOKEN\n";
            js += "  };}\n";
            js += "  async function jfetch(path, init){\n";
            js += "    var r = await fetch(BASE+path, init||{});\n";
            js += "    var t = await r.text();\n";
            js += "    var body = t ? JSON.parse(t) : {};\n";
            js += "    if(!r.ok){ var e = new Error(body.error||('HTTP '+r.status));\n";
            js += "      e.status=r.status; e.body=body; throw e; }\n";
            js += "    return body;\n";
            js += "  }\n";
            js += "  var db = {\n";
            js += "    query: function(sql, params){\n";
            js += "      return jfetch('/db/query', {method:'POST',headers:hdrs(),\n";
            js += "        body:JSON.stringify({sql:sql, params:params||[]})});\n";
            js += "    },\n";
            js += "    execute: function(sql, params){\n";
            js += "      return jfetch('/db/execute', {method:'POST',headers:hdrs(),\n";
            js += "        body:JSON.stringify({sql:sql, params:params||[]})});\n";
            js += "    },\n";
            js += "    schema: function(){\n";
            js += "      return jfetch('/db/schema', {headers:hdrs()});\n";
            js += "    }\n";
            js += "  };\n";
            js += "  var main = {\n";
            js += "    query: function(view, params){\n";
            js += "      return jfetch('/main/query', {method:'POST',headers:hdrs(),\n";
            js += "        body:JSON.stringify({view:view, params:params||[]})});\n";
            js += "    }\n";
            js += "  };\n";
            js += "  var ai = {\n";
            // AI proxy is not yet wired (lands in phase 4+); expose a
            // placeholder that returns a structured error so app code can
            // detect it cleanly rather than hitting a 404.
            js += "    chat: async function(){ throw new Error('avacli.ai not enabled yet'); },\n";
            js += "    stream: async function(){ throw new Error('avacli.ai not enabled yet'); }\n";
            js += "  };\n";
            js += "  window.avacli = {\n";
            js += "    slug: SLUG,\n";
            js += "    db: " + std::string(dbEnabled ? "db" : "null") + ",\n";
            js += "    main: main,\n";
            js += "    ai: " + std::string(aiEnabled ? "ai" : "null") + "\n";
            js += "  };\n";
            js += "})();\n";

            res.set_header("Cache-Control", "no-store");
            res.set_content(js, "application/javascript");
        });
}

} // namespace avacli
