#include "auth/AuthManager.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace avacli {

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string AuthManager::authFilePath() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    fs::path dir = fs::path(home) / ".avacli";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "auth.json").string();
}

bool AuthManager::save(const AuthTokens& tokens) {
    const std::string path = authFilePath();
    try {
        json j;
        j["cluster_key"] = tokens.clusterKey;

        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs) {
            spdlog::error("AuthManager: failed to open {} for writing", path);
            return false;
        }
        ofs << j.dump(2);
        ofs.close();

#ifndef _WIN32
        ::chmod(path.c_str(), 0600);
#endif
        spdlog::debug("AuthManager: saved tokens to {}", path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("AuthManager: save failed: {}", e.what());
        return false;
    }
}

std::optional<AuthTokens> AuthManager::load() {
    const std::string path = authFilePath();
    try {
        if (!fs::exists(path)) return std::nullopt;

        std::ifstream ifs(path);
        if (!ifs) return std::nullopt;

        json j = json::parse(ifs);
        AuthTokens t;
        t.clusterKey = j.value("cluster_key", "");

        return t;
    } catch (const std::exception& e) {
        spdlog::error("AuthManager: load failed: {}", e.what());
        return std::nullopt;
    }
}

bool AuthManager::clear() {
    const std::string path = authFilePath();
    std::error_code ec;
    if (fs::remove(path, ec)) {
        spdlog::info("AuthManager: cleared auth file");
        return true;
    }
    if (ec) spdlog::error("AuthManager: clear failed: {}", ec.message());
    return !ec;
}

} // namespace avacli
