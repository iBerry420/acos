#include "client/NodeClient.hpp"
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <chrono>

namespace avacli {

namespace {

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t streamCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* cb = static_cast<NodeClient::SseCallback*>(user);
    std::string chunk(ptr, size * nmemb);
    (*cb)(chunk);
    return size * nmemb;
}

} // namespace

NodeClient::NodeClient(const NodeInfo& node) : node_(node) {}

std::string NodeClient::baseUrl() const {
    std::string scheme = "http";
    std::string host = node_.domain.empty() ? node_.ip : node_.domain;
    if (host.find("https://") == 0 || host.find("http://") == 0)
        return host + ":" + std::to_string(node_.port);
    return scheme + "://" + host + ":" + std::to_string(node_.port);
}

std::string NodeClient::authToken() const {
    if (node_.password.empty()) return "";
    return node_.password;
}

NodeHealthResult NodeClient::checkHealth() {
    NodeHealthResult result;
    auto start = std::chrono::steady_clock::now();

    auto resp = get("/api/health", 5);
    auto end = std::chrono::steady_clock::now();
    result.latencyMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    if (resp.status == 0) {
        result.error = "unreachable";
        return result;
    }

    result.reachable = true;

    if (resp.status == 401) {
        result.authOk = false;
        result.error = "auth_failed";
        return result;
    }

    if (!resp.ok()) {
        result.error = "http_" + std::to_string(resp.status);
        return result;
    }

    try {
        auto j = nlohmann::json::parse(resp.body);
        if (j.contains("status") && j["status"] == "ok") {
            result.isAvacli = true;
            result.version = j.value("version", "");
            result.workspace = j.value("workspace", "");
        } else {
            result.error = "not_avacli";
        }
    } catch (...) {
        result.error = "invalid_response";
    }

    return result;
}

nlohmann::json NodeClient::proxyGet(const std::string& path) {
    auto resp = get(path);
    if (!resp.ok()) return {{"error", "HTTP " + std::to_string(resp.status)}};
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return {{"error", "invalid JSON"}}; }
}

nlohmann::json NodeClient::proxyPost(const std::string& path, const nlohmann::json& body) {
    auto resp = post(path, body.dump());
    if (!resp.ok()) return {{"error", "HTTP " + std::to_string(resp.status)}};
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return {{"error", "invalid JSON"}}; }
}

bool NodeClient::proxyChatStream(const std::string& message, const std::string& model,
                                  const std::string& session, SseCallback onData,
                                  std::atomic<bool>* cancelFlag) {
    std::string url = baseUrl() + "/api/chat";
    nlohmann::json body;
    body["message"] = message;
    body["model"] = model;
    if (!session.empty()) body["session"] = session;
    std::string payload = body.dump();

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream");
    auto token = authToken();
    if (!token.empty())
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streamCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &onData);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (cancelFlag) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
            +[](void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                auto* flag = static_cast<std::atomic<bool>*>(clientp);
                return flag->load() ? 1 : 0;
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancelFlag);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return res == CURLE_OK && status >= 200 && status < 300;
}

std::string NodeClient::proxyRawGet(const std::string& path, std::string& contentType) {
    auto resp = get(path);
    contentType = resp.contentType;
    return resp.body;
}

NodeClient::HttpResult NodeClient::get(const std::string& path, int timeoutSec) {
    HttpResult result;
    std::string url = baseUrl() + path;

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    struct curl_slist* headers = nullptr;
    auto token = authToken();
    if (!token.empty())
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSec));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
        char* ct = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
        if (ct) result.contentType = ct;
    } else {
        spdlog::debug("NodeClient GET {} failed: {}", path, curl_easy_strerror(res));
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

NodeClient::HttpResult NodeClient::post(const std::string& path, const std::string& body, int timeoutSec) {
    HttpResult result;
    std::string url = baseUrl() + path;

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    auto token = authToken();
    if (!token.empty())
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSec));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
        char* ct = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
        if (ct) result.contentType = ct;
    } else {
        spdlog::debug("NodeClient POST {} failed: {}", path, curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

} // namespace avacli
