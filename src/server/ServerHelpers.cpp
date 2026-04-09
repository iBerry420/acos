#include "server/ServerHelpers.hpp"
#include "config/ApiKeyStorage.hpp"
#include "config/ServeSettings.hpp"
#include "config/VaultStorage.hpp"
#include "platform/Paths.hpp"
#include "platform/ProcessRunner.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace avacli {

namespace fs = std::filesystem;

std::string findPythonCmd() {
    const char* env = std::getenv("AVACLI_PYTHON");
    if (env && env[0] != '\0') return std::string(env);
#if defined(_WIN32)
    const char* cands[] = {"py -3", "python3", "python"};
#elif defined(__APPLE__)
    const char* cands[] = {"python3", "/usr/local/bin/python3", "python"};
#else
    const char* cands[] = {"python3", "python"};
#endif
    for (auto c : cands) {
        auto pr = runShellCommand("", std::string(c) + " --version", 5);
        if (pr.exitCode == 0 && pr.capturedOutput.find("Python") != std::string::npos) return std::string(c);
    }
    return "python3";
}

static std::string cachedPyCmd;
std::string getPyCmd() {
    if (cachedPyCmd.empty()) cachedPyCmd = findPythonCmd();
    return cachedPyCmd;
}

int findAvailablePort(int startPort, int maxAttempts) {
#ifdef _WIN32
    for (int port = startPort; port < startPort + maxAttempts; ++port) {
        SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return startPort;
        BOOL opt = 1;
        ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&opt), sizeof(opt));
        struct sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = INADDR_ANY;
        bool ok = (::bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);
        ::closesocket(s);
        if (ok) return port;
    }
#else
    for (int port = startPort; port < startPort + maxAttempts; ++port) {
        int s = ::socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return startPort;
        int opt = 1;
        ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = INADDR_ANY;
        bool ok = (::bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);
        ::close(s);
        if (ok) return port;
    }
#endif
    return startPort;
}

std::string usageFilePath() {
    std::string home;
    const char* h = std::getenv("HOME");
    if (h) home = h;
    if (home.empty()) home = ".";
    return (fs::path(home) / ".avacli" / "usage.json").string();
}

void appendUsageRecord(const nlohmann::json& record) {
    auto path = usageFilePath();
    fs::create_directories(fs::path(path).parent_path());

    nlohmann::json arr = nlohmann::json::array();
    if (fs::exists(path)) {
        try {
            std::ifstream f(path);
            arr = nlohmann::json::parse(f);
        } catch (...) {
            arr = nlohmann::json::array();
        }
    }
    arr.push_back(record);

    if (arr.size() > 10000) {
        arr = nlohmann::json(arr.end() - 10000, arr.end());
    }

    std::ofstream f(path);
    f << arr.dump();
}

nlohmann::json loadUsageHistory(int days, int hours) {
    auto path = usageFilePath();
    nlohmann::json arr = nlohmann::json::array();
    if (!fs::exists(path)) return arr;

    try {
        std::ifstream f(path);
        arr = nlohmann::json::parse(f);
    } catch (...) {
        return nlohmann::json::array();
    }

    int totalHours = hours > 0 ? hours : (days > 0 ? days * 24 : 0);
    if (totalHours <= 0) return arr;

    auto now = std::chrono::system_clock::now();
    auto cutoff = std::chrono::duration_cast<std::chrono::seconds>(
        (now - std::chrono::hours(totalHours)).time_since_epoch()).count();

    nlohmann::json filtered = nlohmann::json::array();
    for (const auto& r : arr) {
        if (r.contains("timestamp") && r["timestamp"].is_number() && r["timestamp"].get<long long>() >= cutoff)
            filtered.push_back(r);
    }
    return filtered;
}

std::vector<std::string> autoDetectCapabilities(const std::string& workspace) {
    std::vector<std::string> caps = {"chat", "file_ops"};

    try {
        auto wsPath = fs::canonical(workspace);
        for (const auto& entry : fs::directory_iterator(wsPath, fs::directory_options::skip_permission_denied)) {
            std::string name = entry.path().filename().string();
            std::string nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            if (nameLower.find("polymarket") != std::string::npos)
                caps.push_back("polymarket");
            else if (nameLower == "package.json" || nameLower == "node_modules")
                caps.push_back("nodejs");
            else if (nameLower == "requirements.txt" || nameLower == "setup.py" || nameLower == "pyproject.toml")
                caps.push_back("python");
            else if (nameLower == "docker-compose.yml" || nameLower == "dockerfile")
                caps.push_back("docker");
            else if (nameLower == "cargo.toml")
                caps.push_back("rust");
            else if (nameLower == "composer.json")
                caps.push_back("php");
        }
    } catch (...) {}

    std::sort(caps.begin(), caps.end());
    caps.erase(std::unique(caps.begin(), caps.end()), caps.end());
    return caps;
}

std::string generateChatSessionName() {
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    static const char* hx = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    std::string id = "chat_";
    id += std::to_string(sec);
    id += '_';
    for (int i = 0; i < 6; ++i) id += hx[dist(gen)];
    return id;
}

void mergeServeDiskIntoConfig(ServeConfig& sc) {
    auto j = loadServeSettingsJson();
    if (j.contains("model") && j["model"].is_string())
        sc.model = j["model"].get<std::string>();
    if (j.contains("workspace") && j["workspace"].is_string())
        sc.workspace = j["workspace"].get<std::string>();
    if (j.contains("xai_api_key") && j["xai_api_key"].is_string()) {
        std::string k = j["xai_api_key"].get<std::string>();
        if (!k.empty()) sc.apiKey = k;
    }
    if (j.contains("extra_model") && j["extra_model"].is_string())
        sc.extraModel = j["extra_model"].get<std::string>();
    if (sc.uiTheme.empty() && j.contains("ui_theme") && j["ui_theme"].is_string())
        sc.uiTheme = j["ui_theme"].get<std::string>();
}

void resolveXaiAuth(const ServeConfig& sc, std::string& apiKey, std::string& chatUrl) {
    apiKey = sc.apiKey;
    if (apiKey.empty()) {
        const char* env = std::getenv("XAI_API_KEY");
        if (env && env[0] != '\0') apiKey = env;
        else apiKey = ApiKeyStorage::load();
    }
    chatUrl = sc.chatEndpointUrl.empty() ? std::string("https://api.x.ai/v1/chat/completions")
                                          : sc.chatEndpointUrl;
}

std::string getVultrApiKey() {
    const char* env = std::getenv("VULTR_API_KEY");
    if (env && env[0] != '\0') return env;

    auto settingsPath = serveSettingsFilePath();
    if (fs::exists(settingsPath)) {
        try {
            std::ifstream f(settingsPath);
            auto j = nlohmann::json::parse(f);
            if (j.contains("vultr_api_key") && j["vultr_api_key"].is_string()) {
                std::string k = j["vultr_api_key"].get<std::string>();
                if (!k.empty()) return k;
            }
        } catch (...) {}
    }

    auto vault = VaultStorage::listEntries();
    for (const auto& e : vault) {
        if (e.name == "vultr_api_key") {
            auto val = VaultStorage::retrieve("avacli_default", "vultr_api_key");
            if (val) return *val;
        }
    }
    return "";
}

std::string getRequestUser(const httplib::Request& req, MasterKeyManager* mgr) {
    if (!mgr) return "";
    std::string auth = req.get_header_value("Authorization");
    if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ")
        return mgr->getSessionUser(auth.substr(7));
    return "";
}

bool isAdminRequest(const httplib::Request& req, MasterKeyManager* mgr) {
    if (!mgr) return true;
    if (!MasterKeyManager::isConfigured()) return true;
    std::string auth = req.get_header_value("Authorization");
    if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ")
        return mgr->isSessionAdmin(auth.substr(7));
    return false;
}

} // namespace avacli
