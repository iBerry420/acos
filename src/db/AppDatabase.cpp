#include "db/AppDatabase.hpp"
#include "platform/Paths.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace avacli {

namespace fs = std::filesystem;

namespace {

// Slug safety: only lowercase letters, digits, '-', '_'. `slugify()` in
// RoutesApps.cpp enforces this at app-creation time but we re-check here
// so a corrupted row can never navigate us out of ~/.avacli/app_data/.
bool isSafeSlug(const std::string& s) {
    if (s.empty() || s.size() > 128) return false;
    if (s.front() == '.' || s.front() == '-') return false;
    for (char c : s) {
        bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

std::string appDataRoot() {
    return (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "app_data").string();
}

// ── Authorizer ───────────────────────────────────────────────────────────
// Return SQLITE_OK to allow, SQLITE_DENY to refuse.
//
// The sandbox goal is: the app cannot touch any file outside its own DB,
// and cannot change PRAGMAs that affect isolation (journal_mode, foreign
// keys, etc.). Everything else — DDL, DML, transactions, triggers —
// is the app's own business.
int appDbAuthorizer(void* /*userdata*/,
                    int action,
                    const char* arg1,
                    const char* /*arg2*/,
                    const char* /*dbName*/,
                    const char* /*triggerName*/) {
    switch (action) {
        // Hard-deny: any way to reach another DB file.
        case SQLITE_ATTACH:
        case SQLITE_DETACH:
            return SQLITE_DENY;

        // PRAGMA is the classic sandbox escape — whitelist carefully.
        case SQLITE_PRAGMA: {
            static const char* const ALLOWED_PRAGMAS[] = {
                "user_version",     // apps may stamp their own migration version
                "table_info",       // used by schema() below
                "index_info",
                "index_list",
                "foreign_key_list",
                "table_list",
                "page_count",       // size-cap middleware needs this
                "page_size",
                "schema_version",   // read-only view of schema generation
                "application_id",   // apps may set their own id
                "database_list",    // read-only, can only see "main"
            };
            if (!arg1) return SQLITE_DENY;
            std::string p = arg1;
            std::transform(p.begin(), p.end(), p.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            for (const char* ok : ALLOWED_PRAGMAS) {
                if (p == ok) return SQLITE_OK;
            }
            return SQLITE_DENY;
        }

        // Deny the loadable-extension footgun.
        case SQLITE_FUNCTION: {
            if (arg1) {
                std::string fn = arg1;
                std::transform(fn.begin(), fn.end(), fn.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (fn == "load_extension") return SQLITE_DENY;
            }
            return SQLITE_OK;
        }

        // Everything else — SELECT, INSERT, UPDATE, DELETE, CREATE_*, DROP_*,
        // TRANSACTION, READ, ANALYZE, REINDEX, triggers, views — is allowed.
        // Apps own their schema; our job is only to stop sandbox escapes.
        default:
            return SQLITE_OK;
    }
}

void bindJsonParam(sqlite3_stmt* stmt, int idx, const nlohmann::json& v) {
    if (v.is_null()) {
        sqlite3_bind_null(stmt, idx);
    } else if (v.is_boolean()) {
        sqlite3_bind_int(stmt, idx, v.get<bool>() ? 1 : 0);
    } else if (v.is_number_integer()) {
        sqlite3_bind_int64(stmt, idx, v.get<int64_t>());
    } else if (v.is_number_float()) {
        sqlite3_bind_double(stmt, idx, v.get<double>());
    } else if (v.is_string()) {
        auto s = v.get<std::string>();
        sqlite3_bind_text(stmt, idx, s.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        // arrays / objects → serialise to JSON text (apps can use JSON1 funcs)
        auto s = v.dump();
        sqlite3_bind_text(stmt, idx, s.c_str(), -1, SQLITE_TRANSIENT);
    }
}

nlohmann::json columnToJson(sqlite3_stmt* stmt, int c) {
    int type = sqlite3_column_type(stmt, c);
    switch (type) {
        case SQLITE_INTEGER: return sqlite3_column_int64(stmt, c);
        case SQLITE_FLOAT:   return sqlite3_column_double(stmt, c);
        case SQLITE_TEXT: {
            auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
            return p ? std::string(p) : std::string();
        }
        case SQLITE_NULL:    return nullptr;
        case SQLITE_BLOB: {
            int n = sqlite3_column_bytes(stmt, c);
            auto* p = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, c));
            // Apps don't get raw blobs across HTTP today — base64 is the usual
            // answer, but JSON1 inside SQLite already covers most use cases.
            // Return a stable marker so callers can detect blob columns.
            nlohmann::json b;
            b["__blob"] = true;
            b["bytes"] = n;
            (void)p;
            return b;
        }
        default:
            return nullptr;
    }
}

} // namespace

// ── Registry: one AppDatabase per slug, cached for the life of the process.
// Defined outside the anonymous namespace because it's the `friend` named
// in `AppDatabase`'s class body; anonymous-namespace friends are a different
// type from the one the header declared.
class AppDatabaseRegistry {
public:
    static AppDatabaseRegistry& instance() {
        static AppDatabaseRegistry r;
        return r;
    }

    AppDatabase& get(const std::string& slug) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = by_slug_.find(slug);
        if (it != by_slug_.end()) return *it->second;
        std::unique_ptr<AppDatabase> p(new AppDatabase(slug));
        AppDatabase* raw = p.get();
        by_slug_.emplace(slug, std::move(p));
        return *raw;
    }

    void evict(const std::string& slug) {
        std::lock_guard<std::mutex> lk(mu_);
        by_slug_.erase(slug);
    }

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::unique_ptr<AppDatabase>> by_slug_;
};

// ── AppDatabase ──────────────────────────────────────────────────────────

AppDatabase::AppDatabase(const std::string& slug) : slug_(slug) {
    if (!isSafeSlug(slug)) {
        throw std::invalid_argument("AppDatabase: unsafe slug: " + slug);
    }
    fs::path root = appDataRoot();
    fs::create_directories(root);
    path_ = (root / (slug + ".db")).string();
    openOrThrow();
}

AppDatabase::~AppDatabase() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void AppDatabase::openOrThrow() {
    int rc = sqlite3_open(path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "unknown error";
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        throw std::runtime_error("AppDatabase open failed (" + slug_ + "): " + err);
    }

    // Set the authorizer BEFORE any user SQL runs, so no query can see an
    // unsandboxed window even at init time.
    sqlite3_set_authorizer(db_, appDbAuthorizer, nullptr);

    // Set pragmas bypassing the authorizer — these sqlite3_exec calls use
    // internal statements that sqlite3_set_authorizer lets through.
    // (Authorizer callbacks fire on compilation, not on exec of already-prepared
    // statements. Using sqlite3_exec with raw SQL would however re-prepare and
    // trigger SQLITE_PRAGMA — for setup pragmas we temporarily disable the
    // authorizer by passing nullptr for these init writes.)
    sqlite3_set_authorizer(db_, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    // Reinstall authorizer for all subsequent user queries.
    sqlite3_set_authorizer(db_, appDbAuthorizer, nullptr);

    spdlog::debug("AppDatabase opened: slug={} path={}", slug_, path_);
}

sqlite3_stmt* AppDatabase::prepareBound(const std::string& sql,
                                        const std::vector<nlohmann::json>& params) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        if (stmt) sqlite3_finalize(stmt);
        throw std::runtime_error("SQL prepare: " + err);
    }
    for (int i = 0; i < static_cast<int>(params.size()); i++) {
        bindJsonParam(stmt, i + 1, params[i]);
    }
    return stmt;
}

nlohmann::json AppDatabase::query(const std::string& sql,
                                  const std::vector<nlohmann::json>& params) {
    std::lock_guard<std::mutex> lk(mu_);
    sqlite3_stmt* stmt = prepareBound(sql, params);

    nlohmann::json rows = nlohmann::json::array();
    int cols = sqlite3_column_count(stmt);
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        nlohmann::json row = nlohmann::json::object();
        for (int c = 0; c < cols; c++) {
            const char* name = sqlite3_column_name(stmt, c);
            row[name ? name : ("col" + std::to_string(c))] = columnToJson(stmt, c);
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQL step: ") + sqlite3_errmsg(db_));
    }
    return rows;
}

nlohmann::json AppDatabase::execute(const std::string& sql,
                                    const std::vector<nlohmann::json>& params) {
    std::lock_guard<std::mutex> lk(mu_);
    sqlite3_stmt* stmt = prepareBound(sql, params);

    int rc = sqlite3_step(stmt);
    int cols = sqlite3_column_count(stmt);
    // If the statement happens to return rows (e.g. RETURNING clause),
    // drain them into `rows` so clients that use INSERT ... RETURNING
    // can get the generated ids back.
    nlohmann::json rows = nlohmann::json::array();
    while (rc == SQLITE_ROW) {
        nlohmann::json row = nlohmann::json::object();
        for (int c = 0; c < cols; c++) {
            const char* name = sqlite3_column_name(stmt, c);
            row[name ? name : ("col" + std::to_string(c))] = columnToJson(stmt, c);
        }
        rows.push_back(std::move(row));
        rc = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(std::string("SQL exec: ") + sqlite3_errmsg(db_));
    }

    nlohmann::json out;
    out["changes"] = sqlite3_changes(db_);
    out["last_insert_rowid"] = static_cast<int64_t>(sqlite3_last_insert_rowid(db_));
    if (!rows.empty()) out["rows"] = std::move(rows);
    return out;
}

nlohmann::json AppDatabase::schema() {
    std::lock_guard<std::mutex> lk(mu_);
    nlohmann::json out;
    out["tables"] = nlohmann::json::array();

    // List all user tables (exclude SQLite internal `sqlite_*` schema).
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_,
        "SELECT name FROM sqlite_master WHERE type='table' "
        "AND name NOT LIKE 'sqlite_%' ORDER BY name",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        throw std::runtime_error(std::string("schema list: ") + sqlite3_errmsg(db_));
    }

    std::vector<std::string> tables;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (p) tables.emplace_back(p);
    }
    sqlite3_finalize(stmt);

    for (const auto& t : tables) {
        nlohmann::json table;
        table["name"] = t;
        table["columns"] = nlohmann::json::array();

        std::string pragma = "PRAGMA table_info(" + t + ")";
        sqlite3_stmt* ps = nullptr;
        rc = sqlite3_prepare_v2(db_, pragma.c_str(), -1, &ps, nullptr);
        if (rc != SQLITE_OK) {
            if (ps) sqlite3_finalize(ps);
            continue;
        }
        while (sqlite3_step(ps) == SQLITE_ROW) {
            nlohmann::json col;
            auto* cname = reinterpret_cast<const char*>(sqlite3_column_text(ps, 1));
            auto* ctype = reinterpret_cast<const char*>(sqlite3_column_text(ps, 2));
            col["name"] = cname ? cname : "";
            col["type"] = ctype ? ctype : "";
            col["notnull"] = sqlite3_column_int(ps, 3);
            col["pk"] = sqlite3_column_int(ps, 5);
            table["columns"].push_back(std::move(col));
        }
        sqlite3_finalize(ps);
        out["tables"].push_back(std::move(table));
    }
    return out;
}

int64_t AppDatabase::diskBytes() {
    std::lock_guard<std::mutex> lk(mu_);
    int64_t pageCount = 0, pageSize = 0;
    sqlite3_stmt* s = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA page_count", -1, &s, nullptr) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW) pageCount = sqlite3_column_int64(s, 0);
    }
    if (s) sqlite3_finalize(s);
    s = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA page_size", -1, &s, nullptr) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW) pageSize = sqlite3_column_int64(s, 0);
    }
    if (s) sqlite3_finalize(s);
    return pageCount * pageSize;
}

void AppDatabase::setPageLimitForCap(int capMb) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!db_) return;

    int64_t pageSize = 0;
    sqlite3_stmt* s = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA page_size", -1, &s, nullptr) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW) pageSize = sqlite3_column_int64(s, 0);
    }
    if (s) sqlite3_finalize(s);
    if (pageSize <= 0) pageSize = 4096;

    int64_t maxPages = 0; // 0 == unlimited in SQLite
    if (capMb > 0) {
        int64_t capBytes = static_cast<int64_t>(capMb) * 1024LL * 1024LL;
        maxPages = capBytes / pageSize;
        if (maxPages < 1) maxPages = 1;
    }

    // PRAGMA max_page_count is write-through (it persists in the db header).
    // Running this repeatedly with the same value is effectively free.
    // Apps must pass through the authorizer for all statements, so we
    // temporarily lift it to set the pragma (max_page_count is NOT on the
    // whitelist by design — only avacli itself is allowed to move it).
    sqlite3_set_authorizer(db_, nullptr, nullptr);
    std::string sql = "PRAGMA max_page_count=" + std::to_string(maxPages);
    sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
    // Re-install the authorizer for the caller's subsequent query.
    sqlite3_set_authorizer(db_, appDbAuthorizer, nullptr);
}

std::string AppDatabase::filePath() const {
    return path_;
}

AppDatabase& AppDatabase::forSlug(const std::string& slug) {
    return AppDatabaseRegistry::instance().get(slug);
}

void AppDatabase::evict(const std::string& slug) {
    AppDatabaseRegistry::instance().evict(slug);
}

} // namespace avacli
