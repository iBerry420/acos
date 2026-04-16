#include "db/Database.hpp"
#include "platform/Paths.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace avacli {

namespace fs = std::filesystem;

Database& Database::instance() {
    static Database db;
    return db;
}

Database::~Database() {
    close();
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        initialized_ = false;
    }
}

void Database::init() {
    std::lock_guard<std::mutex> lock(mu_);
    if (initialized_) return;

    auto dir = fs::path(userHomeDirectoryOrFallback()) / ".avacli";
    fs::create_directories(dir);
    auto dbPath = (dir / "avacli.db").string();

    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "unknown error";
        spdlog::error("Failed to open database: {}", err);
        throw std::runtime_error("Failed to open database: " + err);
    }

    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);

    initialized_ = true;
    migrate();
    spdlog::info("Database initialized at {}", dbPath);
}

void Database::exec(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mu_);
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        spdlog::error("SQL exec error: {}", msg);
        throw std::runtime_error("SQL error: " + msg);
    }
}

nlohmann::json Database::query(const std::string& sql,
                               const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mu_);
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("SQL prepare: ") + sqlite3_errmsg(db_));
    }

    for (int i = 0; i < static_cast<int>(params.size()); i++) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    nlohmann::json rows = nlohmann::json::array();
    int cols = sqlite3_column_count(stmt);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        nlohmann::json row;
        for (int c = 0; c < cols; c++) {
            const char* colName = sqlite3_column_name(stmt, c);
            int type = sqlite3_column_type(stmt, c);
            switch (type) {
                case SQLITE_INTEGER:
                    row[colName] = sqlite3_column_int64(stmt, c);
                    break;
                case SQLITE_FLOAT:
                    row[colName] = sqlite3_column_double(stmt, c);
                    break;
                case SQLITE_TEXT:
                    row[colName] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
                    break;
                case SQLITE_NULL:
                    row[colName] = nullptr;
                    break;
                default:
                    row[colName] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
                    break;
            }
        }
        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQL step: ") + sqlite3_errmsg(db_));
    }
    return rows;
}

nlohmann::json Database::queryOne(const std::string& sql,
                                  const std::vector<std::string>& params) {
    auto rows = query(sql, params);
    if (rows.empty()) return nullptr;
    return rows[0];
}

int64_t Database::execute(const std::string& sql,
                          const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mu_);
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("SQL prepare: ") + sqlite3_errmsg(db_));
    }

    for (int i = 0; i < static_cast<int>(params.size()); i++) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQL exec: ") + sqlite3_errmsg(db_));
    }
    return sqlite3_changes(db_);
}

int Database::currentVersion() {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_,
        "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return 0;
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return version;
}

void Database::setVersion(int v) {
    char* err = nullptr;
    auto sql = "INSERT INTO schema_version (version) VALUES (" + std::to_string(v) + ")";
    sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

void Database::migrate() {
    char* err = nullptr;
    sqlite3_exec(db_,
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version INTEGER PRIMARY KEY,"
        "  applied_at TEXT DEFAULT (datetime('now'))"
        ")", nullptr, nullptr, &err);
    if (err) { sqlite3_free(err); err = nullptr; }

    int ver = currentVersion();

    if (ver < 1) {
        sqlite3_exec(db_, R"(
            CREATE TABLE IF NOT EXISTS articles (
                id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                content TEXT NOT NULL DEFAULT '',
                summary TEXT DEFAULT '',
                tags TEXT DEFAULT '[]',
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL,
                created_by TEXT DEFAULT '',
                source_session TEXT DEFAULT ''
            );
            CREATE INDEX IF NOT EXISTS idx_articles_created ON articles(created_at);
            CREATE INDEX IF NOT EXISTS idx_articles_title ON articles(title);

            CREATE TABLE IF NOT EXISTS apps (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT DEFAULT '',
                slug TEXT UNIQUE NOT NULL,
                status TEXT DEFAULT 'active',
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL,
                created_by TEXT DEFAULT ''
            );
            CREATE UNIQUE INDEX IF NOT EXISTS idx_apps_slug ON apps(slug);

            CREATE TABLE IF NOT EXISTS app_files (
                id TEXT PRIMARY KEY,
                app_id TEXT NOT NULL REFERENCES apps(id) ON DELETE CASCADE,
                filename TEXT NOT NULL,
                content TEXT NOT NULL DEFAULT '',
                mime_type TEXT DEFAULT 'text/plain',
                updated_at INTEGER NOT NULL,
                UNIQUE(app_id, filename)
            );
            CREATE INDEX IF NOT EXISTS idx_app_files_app ON app_files(app_id);

            CREATE TABLE IF NOT EXISTS services (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT DEFAULT '',
                type TEXT NOT NULL DEFAULT 'custom',
                config TEXT DEFAULT '{}',
                status TEXT DEFAULT 'stopped',
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL,
                last_run INTEGER DEFAULT 0,
                next_run INTEGER DEFAULT 0,
                created_by TEXT DEFAULT ''
            );

            CREATE TABLE IF NOT EXISTS service_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                service_id TEXT NOT NULL REFERENCES services(id) ON DELETE CASCADE,
                timestamp INTEGER NOT NULL,
                level TEXT NOT NULL DEFAULT 'info',
                message TEXT NOT NULL DEFAULT ''
            );
            CREATE INDEX IF NOT EXISTS idx_svc_logs_svc ON service_logs(service_id, timestamp);

            CREATE TABLE IF NOT EXISTS event_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER NOT NULL,
                level TEXT NOT NULL DEFAULT 'info',
                category TEXT NOT NULL DEFAULT 'system',
                message TEXT NOT NULL DEFAULT '',
                details TEXT DEFAULT '{}'
            );
            CREATE INDEX IF NOT EXISTS idx_event_log_ts ON event_log(timestamp);
            CREATE INDEX IF NOT EXISTS idx_event_log_cat ON event_log(category);
        )", nullptr, nullptr, &err);
        if (err) { spdlog::error("Migration v1 error: {}", err); sqlite3_free(err); err = nullptr; }
        setVersion(1);
        spdlog::info("Database migrated to v1");
    }

    if (ver < 2) {
        sqlite3_exec(db_, "ALTER TABLE apps ADD COLUMN icon_url TEXT DEFAULT ''",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }
        setVersion(2);
        spdlog::info("Database migrated to v2 (apps.icon_url)");
    }

    if (ver < 3) {
        sqlite3_exec(db_, R"(
            CREATE TABLE IF NOT EXISTS nodes (
                id TEXT PRIMARY KEY,
                ip TEXT NOT NULL,
                port INTEGER DEFAULT 8080,
                domain TEXT DEFAULT '',
                label TEXT DEFAULT '',
                username TEXT DEFAULT '',
                auth_token TEXT DEFAULT '',
                status TEXT DEFAULT 'unknown',
                latency_ms INTEGER DEFAULT -1,
                version TEXT DEFAULT '',
                workspace TEXT DEFAULT '',
                vultr_instance_id TEXT DEFAULT '',
                added_at INTEGER NOT NULL,
                last_seen INTEGER DEFAULT 0,
                last_sync INTEGER DEFAULT 0
            );
            CREATE INDEX IF NOT EXISTS idx_nodes_status ON nodes(status);

            CREATE TABLE IF NOT EXISTS mesh_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                event_type TEXT NOT NULL,
                node_id TEXT DEFAULT '',
                source_node_id TEXT DEFAULT '',
                payload TEXT DEFAULT '{}',
                created_at INTEGER NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_mesh_events_ts ON mesh_events(created_at);
            CREATE INDEX IF NOT EXISTS idx_mesh_events_type ON mesh_events(event_type);
        )", nullptr, nullptr, &err);
        if (err) { spdlog::error("Migration v3 error: {}", err); sqlite3_free(err); err = nullptr; }

        // Migrate existing nodes.json into SQLite
        auto home = std::string(std::getenv("HOME") ? std::getenv("HOME") : ".");
        auto jsonPath = (fs::path(home) / ".avacli" / "nodes.json").string();
        if (fs::exists(jsonPath)) {
            try {
                std::ifstream f(jsonPath);
                auto arr = nlohmann::json::parse(f);
                for (const auto& n : arr) {
                    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::string id = n.value("id", std::to_string(now));
                    std::string ip = n.value("ip", "");
                    int port = n.value("port", 8080);
                    std::string label = n.value("label", ip);
                    std::string domain = n.value("domain", "");
                    std::string username = n.value("username", "");
                    int64_t addedAt = n.value("added_at", now);

                    auto sql = "INSERT OR IGNORE INTO nodes (id, ip, port, domain, label, username, added_at) "
                               "VALUES ('" + id + "', '" + ip + "', " + std::to_string(port) +
                               ", '" + domain + "', '" + label + "', '" + username + "', " +
                               std::to_string(addedAt) + ")";
                    sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
                }
                spdlog::info("Migrated {} nodes from nodes.json to SQLite", arr.size());
            } catch (const std::exception& ex) {
                spdlog::warn("Could not migrate nodes.json: {}", ex.what());
            }
        }

        setVersion(3);
        spdlog::info("Database migrated to v3 (nodes + mesh_events)");
    }

    if (ver < 4) {
        // Phase 1 platform capabilities: supervisor state for `type:"process"`
        // services. Columns default to 0 so older rows of tick-type services
        // (rss_feed, scheduled_prompt, custom_script) are unaffected — they
        // simply don't read these fields.
        sqlite3_exec(db_,
            "ALTER TABLE services ADD COLUMN pid INTEGER DEFAULT 0",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }
        sqlite3_exec(db_,
            "ALTER TABLE services ADD COLUMN started_at INTEGER DEFAULT 0",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }
        sqlite3_exec(db_,
            "ALTER TABLE services ADD COLUMN restart_count INTEGER DEFAULT 0",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }
        sqlite3_exec(db_,
            "ALTER TABLE services ADD COLUMN last_exit_code INTEGER DEFAULT 0",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }

        // Existing idx_svc_logs_svc already covers (service_id, timestamp).
        // Add a DESC-ordered index for tail-N queries.
        sqlite3_exec(db_,
            "CREATE INDEX IF NOT EXISTS idx_svc_logs_svc_ts_desc "
            "ON service_logs(service_id, timestamp DESC)",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }

        setVersion(4);
        spdlog::info("Database migrated to v4 (services supervisor state)");
    }

    if (ver < 5) {
        // Phase 3 platform capabilities: per-app SQLite access + feature
        // flags + usage accounting. Defaults keep older rows behaving
        // identically (db_enabled=1, ai_enabled=1, db_size_cap_mb=0 inherits
        // settings). agent_token is filled lazily on first GET/POST that
        // needs it (see AppTokens::ensureForAppId) — backfilling every
        // existing app at migration time would require us to call OpenSSL
        // inside migrate() which adds a dependency ordering constraint we
        // don't need.
        sqlite3_exec(db_,
            "ALTER TABLE apps ADD COLUMN agent_token TEXT DEFAULT ''",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }
        sqlite3_exec(db_,
            "ALTER TABLE apps ADD COLUMN db_enabled INTEGER DEFAULT 1",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }
        sqlite3_exec(db_,
            "ALTER TABLE apps ADD COLUMN ai_enabled INTEGER DEFAULT 1",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }
        sqlite3_exec(db_,
            "ALTER TABLE apps ADD COLUMN db_size_cap_mb INTEGER DEFAULT 0",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }

        // Unique on non-empty agent_token only — empty strings are allowed
        // to coexist (rows pre-token). SQLite's partial-index syntax lets
        // us express this without a trigger.
        sqlite3_exec(db_,
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_apps_agent_token "
            "ON apps(agent_token) WHERE agent_token != ''",
            nullptr, nullptr, &err);
        if (err) { sqlite3_free(err); err = nullptr; }

        sqlite3_exec(db_, R"(
            CREATE TABLE IF NOT EXISTS app_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                app_id TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                kind TEXT NOT NULL,
                tokens INTEGER DEFAULT 0,
                bytes INTEGER DEFAULT 0
            );
            CREATE INDEX IF NOT EXISTS idx_app_usage_app_ts ON app_usage(app_id, timestamp);
            CREATE INDEX IF NOT EXISTS idx_app_usage_kind ON app_usage(kind, timestamp);
        )", nullptr, nullptr, &err);
        if (err) { spdlog::error("Migration v5 error: {}", err); sqlite3_free(err); err = nullptr; }

        setVersion(5);
        spdlog::info("Database migrated to v5 (apps DB access + usage)");
    }

    if (ver < 6) {
        // Phase 4 platform capabilities: sub-agents with scoped writes +
        // lease-based coordination. xAI-only (no provider column — the rest
        // of the stack already hard-assumes xAI). Depth is checked against
        // `subagents_max_depth` setting at spawn time; 0 disables entirely.
        //
        // agent_leases.resource_key is UNIQUE so exactly one task can hold
        // a given resource (typically a file path). `expires_at` lets a
        // background sweep reap stale entries when a task crashes without
        // releasing.
        sqlite3_exec(db_, R"(
            CREATE TABLE IF NOT EXISTS agent_tasks (
                id TEXT PRIMARY KEY,
                parent_task_id TEXT DEFAULT '',
                session_id TEXT DEFAULT '',
                goal TEXT NOT NULL,
                allowed_paths TEXT DEFAULT '[]',
                allowed_tools TEXT DEFAULT '[]',
                depth INTEGER NOT NULL DEFAULT 0,
                status TEXT NOT NULL DEFAULT 'pending',
                result TEXT DEFAULT '',
                error TEXT DEFAULT '',
                prompt_tokens INTEGER DEFAULT 0,
                completion_tokens INTEGER DEFAULT 0,
                created_at INTEGER NOT NULL,
                started_at INTEGER DEFAULT 0,
                finished_at INTEGER DEFAULT 0,
                cancel_requested INTEGER DEFAULT 0
            );
            CREATE INDEX IF NOT EXISTS idx_agent_tasks_parent ON agent_tasks(parent_task_id);
            CREATE INDEX IF NOT EXISTS idx_agent_tasks_status ON agent_tasks(status);
            CREATE INDEX IF NOT EXISTS idx_agent_tasks_session ON agent_tasks(session_id);

            CREATE TABLE IF NOT EXISTS agent_leases (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                task_id TEXT NOT NULL,
                resource_key TEXT NOT NULL,
                acquired_at INTEGER NOT NULL,
                expires_at INTEGER NOT NULL
            );
            CREATE UNIQUE INDEX IF NOT EXISTS idx_agent_leases_key ON agent_leases(resource_key);
            CREATE INDEX IF NOT EXISTS idx_agent_leases_task ON agent_leases(task_id);
            CREATE INDEX IF NOT EXISTS idx_agent_leases_expires ON agent_leases(expires_at);
        )", nullptr, nullptr, &err);
        if (err) { spdlog::error("Migration v6 error: {}", err); sqlite3_free(err); err = nullptr; }

        setVersion(6);
        spdlog::info("Database migrated to v6 (agent_tasks + agent_leases)");
    }
}

} // namespace avacli
