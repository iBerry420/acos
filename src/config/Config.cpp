#include "config/Config.hpp"
#include "config/ApiKeyStorage.hpp"
#include <cstdlib>
#include <filesystem>

namespace avacli {

std::string Config::xaiApiKey() const {
    const char* envKey = std::getenv("XAI_API_KEY");
    if (envKey && envKey[0] != '\0')
        return std::string(envKey);
    return ApiKeyStorage::load();
}

bool Config::validate() const {
    return !xaiApiKey().empty();
}

bool Config::validateServe() const {
    return servePort > 0 && servePort < 65536;
}

void Config::resolveWorkspace() {
    namespace fs = std::filesystem;
    try {
        fs::path p(workspace);
        if (fs::exists(p))
            workspace = fs::canonical(p).string();
        else
            workspace = fs::absolute(p).string();
    } catch (...) {
        workspace = fs::absolute(workspace).string();
    }
}

} // namespace avacli
