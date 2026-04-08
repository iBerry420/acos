#include "db/Database.hpp"
#include "platform/Paths.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
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
}

} // namespace avacli
