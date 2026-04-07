#include "config/ApiKeyStorage.hpp"
#include "platform/Paths.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace avacli {

namespace fs = std::filesystem;

std::string ApiKeyStorage::configPath() {
    std::string home = userHomeDirectory();
    if (home.empty())
        return (fs::path(".avacli") / "config.json").string();
    return (fs::path(home) / ".avacli" / "config.json").string();
}

std::string ApiKeyStorage::load() {
    try {
        std::string path = configPath();
        if (!fs::exists(path))
            return "";

        std::ifstream f(path);
        if (!f)
            return "";

        auto j = nlohmann::json::parse(f);
        if (j.contains("api_key") && j["api_key"].is_string())
            return j["api_key"].get<std::string>();
    } catch (const std::exception& e) {
        spdlog::debug("ApiKeyStorage: load failed: {}", e.what());
    }
    return "";
}

bool ApiKeyStorage::save(const std::string& apiKey) {
    try {
        std::string path = configPath();
        std::string dir = fs::path(path).parent_path().string();

        fs::create_directories(dir);

        nlohmann::json j;
        j["api_key"] = apiKey;

        std::ofstream f(path);
        if (!f) {
            spdlog::error("ApiKeyStorage: Cannot open config for writing: {}", path);
            return false;
        }
        f << j.dump(2);

#ifndef _WIN32
        chmod(path.c_str(), 0600);
#endif
        spdlog::info("API key saved to {}", path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ApiKeyStorage: save failed: {}", e.what());
        return false;
    }
}

} // namespace avacli
