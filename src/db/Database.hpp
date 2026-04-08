#pragma once
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <mutex>
#include <string>
#include <vector>

namespace avacli {

class Database {
public:
    static Database& instance();

    void init();
    void close();

    void exec(const std::string& sql);
    nlohmann::json query(const std::string& sql,
                         const std::vector<std::string>& params = {});
    nlohmann::json queryOne(const std::string& sql,
                            const std::vector<std::string>& params = {});
    int64_t execute(const std::string& sql,
                    const std::vector<std::string>& params = {});

    sqlite3* raw() { return db_; }

private:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void migrate();
    int currentVersion();
    void setVersion(int v);

    sqlite3* db_ = nullptr;
    mutable std::mutex mu_;
    bool initialized_ = false;
};

} // namespace avacli
