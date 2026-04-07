#include "config/ModelRegistry.hpp"
#include "core/Types.hpp"
#include "platform/Paths.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>

namespace avacli {

namespace fs = std::filesystem;

namespace {

const std::vector<ModelConfig> FALLBACK_MODELS = {
    {"grok-4-0709",    131072, 65536, false},
    {"grok-3",         131072, 65536, false},
    {"grok-3-mini",    131072, 65536, false},
    {"grok-code-fast-1", 131072, 65536, false},
};

std::vector<ModelConfig> dynamicModels;
bool fetched = false;

std::string cacheDir() {
    return (fs::path(userHomeDirectoryOrFallback()) / ".avacli").string();
}

std::string cachePath() {
    return (fs::path(cacheDir()) / "models-cache.json").string();
}

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* s = static_cast<std::string*>(ud);
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string httpGetSimple(const std::string& url, const std::string& apiKey) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    struct curl_slist* headers = nullptr;
    if (!apiKey.empty()) {
        std::string auth = "Authorization: Bearer " + apiKey;
        headers = curl_slist_append(headers, auth.c_str());
    }

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? body : "";
}

bool loadCache() {
    std::string path = cachePath();
    if (!fs::exists(path)) return false;

    try {
        auto lastWrite = fs::last_write_time(path);
        auto now = fs::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWrite).count();
        if (age > 24) return false;  // Cache expired

        std::ifstream f(path);
        auto j = nlohmann::json::parse(f);
        if (!j.is_array()) return false;

        dynamicModels.clear();
        for (const auto& m : j) {
            ModelConfig cfg;
            cfg.id = m.value("id", "");
            cfg.contextWindow = m.value("context_window", 131072);
            cfg.defaultMaxTokens = m.value("max_tokens", 65536);
            cfg.reasoning = m.value("reasoning", false);
            cfg.type = m.value("type", "chat");
            if (!cfg.id.empty()) dynamicModels.push_back(cfg);
        }
        if (!dynamicModels.empty()) {
            fetched = true;
            spdlog::debug("ModelRegistry: Loaded {} models from cache", dynamicModels.size());
            return true;
        }
    } catch (...) {}
    return false;
}

void saveCache() {
    try {
        fs::create_directories(cacheDir());
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& m : dynamicModels) {
            arr.push_back({
                {"id", m.id},
                {"context_window", m.contextWindow},
                {"max_tokens", m.defaultMaxTokens},
                {"reasoning", m.reasoning},
                {"type", m.type}
            });
        }
        std::ofstream f(cachePath());
        if (f) f << arr.dump(2);
    } catch (...) {}
}

void parseModelsFromApi(const std::string& body, const std::string& type) {
    try {
        auto j = nlohmann::json::parse(body);

        if (j.contains("error")) {
            std::string errMsg = j["error"].is_string() ? j["error"].get<std::string>()
                : j["error"].is_object() ? j["error"].value("message", "unknown") : "unknown";
            spdlog::warn("ModelRegistry: API error for {}: {}", type, errMsg);
            return;
        }

        nlohmann::json models;
        if (j.contains("data") && j["data"].is_array())
            models = j["data"];
        else if (j.contains("models") && j["models"].is_array())
            models = j["models"];
        else if (j.is_array())
            models = j;
        else
            return;

        size_t added = 0;
        for (const auto& m : models) {
            std::string id = m.value("id", "");
            if (id.empty()) continue;

            bool exists = false;
            for (const auto& existing : dynamicModels)
                if (existing.id == id) { exists = true; break; }
            if (exists) continue;

            ModelConfig cfg;
            cfg.id = id;
            cfg.type = type;
            cfg.contextWindow = m.value("context_window", 131072);
            cfg.defaultMaxTokens = m.value("max_output_tokens",
                m.value("max_tokens", id.find("reasoning") != std::string::npos ? 81920 : 65536));
            bool nameImpliesReasoning = id.find("reasoning") != std::string::npos
                                       && id.find("non-reasoning") == std::string::npos;
            cfg.reasoning = m.value("reasoning", nameImpliesReasoning);

            if (type == "image_gen" || type == "video_gen") {
                cfg.contextWindow = 0;
                cfg.defaultMaxTokens = 0;
            }

            dynamicModels.push_back(cfg);
            ++added;
        }
        spdlog::info("ModelRegistry: Parsed {} new models from {} endpoint (total: {})",
                      added, type, dynamicModels.size());
    } catch (const std::exception& e) {
        spdlog::warn("ModelRegistry: Failed to parse {} response: {}", type, e.what());
    }
}

} // namespace

void ModelRegistry::fetchFromApi(const std::string& apiKey) {
    if (fetched) return;
    if (loadCache()) return;

    if (apiKey.empty()) return;

    spdlog::info("ModelRegistry: Fetching models from xAI API...");

    std::string resp1 = httpGetSimple("https://api.x.ai/v1/language-models", apiKey);
    if (!resp1.empty()) parseModelsFromApi(resp1, "chat");

    std::string resp2 = httpGetSimple("https://api.x.ai/v1/image-generation-models", apiKey);
    if (!resp2.empty()) parseModelsFromApi(resp2, "image_gen");

    std::string resp3 = httpGetSimple("https://api.x.ai/v1/video-generation-models", apiKey);
    if (!resp3.empty()) parseModelsFromApi(resp3, "video_gen");

    std::string resp4 = httpGetSimple("https://api.x.ai/v1/models", apiKey);
    if (!resp4.empty()) parseModelsFromApi(resp4, "chat");

    if (!dynamicModels.empty()) {
        saveCache();
        fetched = true;
        spdlog::info("ModelRegistry: Fetched {} models from API", dynamicModels.size());
    } else {
        fetched = true;
        spdlog::debug("ModelRegistry: API returned no models, using fallback list");
    }
}

void ModelRegistry::refreshModels(const std::string& apiKey) {
    if (apiKey.empty()) return;
    fetched = false;
    dynamicModels.clear();
    try { fs::remove(cachePath()); } catch (...) {}
    fetchFromApi(apiKey);
}

ModelConfig ModelRegistry::get(const std::string& id) {
    // Check dynamic models first
    if (!dynamicModels.empty()) {
        auto it = std::find_if(dynamicModels.begin(), dynamicModels.end(),
            [&id](const ModelConfig& m) { return m.id == id; });
        if (it != dynamicModels.end()) return *it;
    }

    // Check fallback
    auto it = std::find_if(FALLBACK_MODELS.begin(), FALLBACK_MODELS.end(),
        [&id](const ModelConfig& m) { return m.id == id; });
    if (it != FALLBACK_MODELS.end()) return *it;

    bool isReasoning = id.find("reasoning") != std::string::npos
                       && id.find("non-reasoning") == std::string::npos;
    size_t maxTok = isReasoning ? 81920 : 65536;
    return {id, 131072, maxTok, isReasoning};
}

std::vector<std::string> ModelRegistry::listIds() {
    const auto& all = listAll();
    std::vector<std::string> ids;
    ids.reserve(all.size());
    for (const auto& m : all) ids.push_back(m.id);
    return ids;
}

const std::vector<ModelConfig>& ModelRegistry::listAll() {
    if (!dynamicModels.empty()) return dynamicModels;
    return FALLBACK_MODELS;
}

std::string ModelRegistry::listModelsJson() {
    const auto& all = listAll();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& m : all) {
        arr.push_back({
            {"id", m.id},
            {"context_window", m.contextWindow},
            {"max_output_tokens", m.defaultMaxTokens},
            {"reasoning", m.reasoning},
            {"type", m.type}
        });
    }
    return arr.dump(2);
}

} // namespace avacli
