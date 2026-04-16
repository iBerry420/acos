#pragma once

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace avacli {

/// Per-app SQLite database, sandboxed via sqlite3_set_authorizer.
///
/// Each slug gets its own file at `~/.avacli/app_data/<slug>.db`. One
/// sqlite3* is cached per slug (WAL mode, FK on) behind a shared mutex;
/// parallel queries within a single app serialise on that mutex — fine for
/// the expected workload (LLM-generated UIs making a handful of calls per
/// user action).
///
/// The authorizer rejects `ATTACH` / `DETACH` and any pragma outside a
/// tiny whitelist so an app cannot reach out of its sandbox even given a
/// SQL-injection hole in the app itself. Within the sandbox, CREATE/DROP
/// TABLE, CREATE/DROP INDEX, and SELECT/INSERT/UPDATE/DELETE are all
/// allowed — apps manage their own schema however they like.
///
/// Size-cap enforcement lives in the caller (`RoutesAppDB.cpp`) via
/// `pageCount()` / `pageSize()` — keeping it out of the hot per-query path
/// here means reads never fail with 507.
class AppDatabase {
public:
    /// Returns the shared AppDatabase handle for `slug`. Lazily creates
    /// the SQLite file + applies the authorizer on first access.
    static AppDatabase& forSlug(const std::string& slug);

    /// Run any statement the authorizer permits. Rows come back as
    /// [{col: val, ...}, ...]. `params` supports string/int/double/null
    /// via the nlohmann::json type — bind order matches positional `?1/?2/...`.
    nlohmann::json query(const std::string& sql,
                         const std::vector<nlohmann::json>& params = {});

    /// For INSERT/UPDATE/DELETE/CREATE/DROP. Returns
    /// `{"changes": <affected rows>, "last_insert_rowid": <rowid>}`.
    nlohmann::json execute(const std::string& sql,
                           const std::vector<nlohmann::json>& params = {});

    /// List tables + their columns. Shape:
    /// {"tables":[{"name":"t","columns":[{"name":"c","type":"TEXT","notnull":0,"pk":0}]}]}
    nlohmann::json schema();

    /// On-disk size in bytes (`page_count * page_size`). NOTE: only counts
    /// the materialised main file — any pages still in WAL are NOT included,
    /// so this is observability-grade, not enforcement-grade.
    int64_t diskBytes();

    /// Configure the connection so SQLite itself refuses to grow the
    /// database past `capMb` megabytes. WAL-aware — when a write would
    /// exceed the cap, SQLite returns SQLITE_FULL and the statement is
    /// rolled back atomically. Callers in `RoutesAppDB.cpp` detect this
    /// by the "database or disk is full" error string and return HTTP 507.
    ///
    /// `capMb <= 0` removes the page limit (unbounded).
    void setPageLimitForCap(int capMb);

    /// Absolute path of the underlying SQLite file. Used by the `/export`
    /// endpoint to stream the DB to the caller.
    std::string filePath() const;

    /// Drop the cached sqlite3* for a slug. Safe to call from the `DELETE
    /// /api/apps/:id` path so the on-disk file can be unlinked afterwards.
    static void evict(const std::string& slug);

    // Non-copyable; the singleton registry owns these. Ctor/dtor are public
    // only so `std::unique_ptr<AppDatabase>` in the registry can default-
    // delete them. All construction should go through `forSlug()` — direct
    // construction is undefined behaviour vs the registry cache.
    AppDatabase(const AppDatabase&) = delete;
    AppDatabase& operator=(const AppDatabase&) = delete;
    explicit AppDatabase(const std::string& slug);
    ~AppDatabase();

private:
    void openOrThrow();
    sqlite3_stmt* prepareBound(const std::string& sql,
                               const std::vector<nlohmann::json>& params);

    std::string slug_;
    std::string path_;
    sqlite3* db_{nullptr};
    mutable std::mutex mu_;
};

} // namespace avacli
