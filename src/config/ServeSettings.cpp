#include "config/ServeSettings.hpp"
#include "platform/Paths.hpp"
#include <filesystem>
#include <fstream>

namespace avacli {

std::string serveSettingsFilePath() {
    std::string home = userHomeDirectoryOrFallback();
    return (std::filesystem::path(home) / ".avacli" / "settings.json").string();
}

nlohmann::json loadServeSettingsJson() {
    nlohmann::json j = nlohmann::json::object();
    try {
        std::ifstream f(serveSettingsFilePath());
        if (!f)
            return j;
        auto parsed = nlohmann::json::parse(f, nullptr, false);
        if (parsed.is_object())
            return parsed;
    } catch (...) {}
    return j;
}

} // namespace avacli
