#include "services/RelayClient.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace avacli {

namespace {

std::string localHostLabel() {
#ifdef _WIN32
    char buf[256] = {};
    DWORD n = sizeof(buf);
    if (GetComputerNameA(buf, &n) && n > 0)
        return std::string(buf, static_cast<std::size_t>(n));
    return "relay-client";
#else
    char buf[256] = {};
    if (gethostname(buf, sizeof(buf)) == 0)
        return std::string(buf);
    return "relay-client";
#endif
}

struct CurlResponse {
    long status = 0;
    std::string body;
};

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

CurlResponse httpPost(const std::string& url, const std::string& body,
                       const std::string& authToken, int timeoutSec = 10) {
    CurlResponse result;
    CURL* curl = curl_easy_init();
    if (!curl) return result;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!authToken.empty())
        headers = curl_slist_append(headers, ("Authorization: Bearer " + authToken).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSec));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    else
        spdlog::debug("RelayClient POST {} failed: {}", url, curl_easy_strerror(res));

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

CurlResponse httpGet(const std::string& url, const std::string& authToken, int timeoutSec = 30) {
    CurlResponse result;
    CURL* curl = curl_easy_init();
    if (!curl) return result;

    struct curl_slist* headers = nullptr;
    if (!authToken.empty())
        headers = curl_slist_append(headers, ("Authorization: Bearer " + authToken).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSec));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    else
        spdlog::debug("RelayClient GET {} failed: {}", url, curl_easy_strerror(res));

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

// Stream a local chat response back to the server chunk by chunk
struct StreamCtx {
    std::string serverUrl;
    std::string authToken;
    std::string requestId;
};

size_t localStreamCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* ctx = static_cast<StreamCtx*>(user);
    std::string chunk(ptr, size * nmemb);
    std::string url = ctx->serverUrl + "/api/relay/chunk?request_id=" +
                      ctx->requestId;
    httpPost(url, chunk, ctx->authToken, 5);
    return size * nmemb;
}

} // namespace

RelayClient& RelayClient::instance() {
    static RelayClient rc;
    return rc;
}

RelayClient::~RelayClient() {
    stop();
}

void RelayClient::start(const std::string& serverUrl, const std::string& authToken, int localPort) {
    if (running_) return;

    serverUrl_ = serverUrl;
    // Strip trailing slash
    while (!serverUrl_.empty() && serverUrl_.back() == '/')
        serverUrl_.pop_back();
    authToken_ = authToken;
    localPort_ = localPort;
    stopFlag_ = false;
    running_ = true;
    thread_ = std::thread(&RelayClient::runLoop, this);
    spdlog::info("RelayClient started → {}", serverUrl_);
}

void RelayClient::stop() {
    if (!running_) return;
    stopFlag_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
    connected_ = false;

    if (!clientId_.empty() && !serverUrl_.empty()) {
        nlohmann::json body = {{"client_id", clientId_}};
        httpPost(serverUrl_ + "/api/relay/disconnect", body.dump(), authToken_, 3);
    }
    spdlog::info("RelayClient stopped");
}

void RelayClient::runLoop() {
    // Initial delay to let the local server start
    for (int i = 0; i < 5 && !stopFlag_; i++)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    int backoff = 1;
    while (!stopFlag_) {
        if (clientId_.empty()) {
            if (registerWithServer()) {
                backoff = 1;
            } else {
                connected_ = false;
                spdlog::warn("RelayClient: registration failed, retrying in {}s", backoff);
                for (int i = 0; i < backoff && !stopFlag_; i++)
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                backoff = std::min(backoff * 2, 60);
                continue;
            }
        }

        pollAndProcess();
    }
}

bool RelayClient::registerWithServer() {
    const std::string label = localHostLabel();

    nlohmann::json body;
    body["label"] = label;
    body["version"] = "2.3.0";
    body["workspace"] = "";

    auto resp = httpPost(serverUrl_ + "/api/relay/register", body.dump(), authToken_);
    if (resp.status != 200) {
        spdlog::warn("RelayClient: register failed (HTTP {})", resp.status);
        return false;
    }

    try {
        auto j = nlohmann::json::parse(resp.body);
        clientId_ = j.value("client_id", "");
        if (clientId_.empty()) return false;
        connected_ = true;
        spdlog::info("RelayClient: registered as {} (id={})", label, clientId_);
        return true;
    } catch (...) {
        return false;
    }
}

void RelayClient::pollAndProcess() {
    std::string url = serverUrl_ + "/api/relay/poll?client_id=" + clientId_;
    auto resp = httpGet(url, authToken_, 30);

    if (resp.status == 0) {
        connected_ = false;
        spdlog::warn("RelayClient: poll failed (server unreachable)");
        for (int i = 0; i < 5 && !stopFlag_; i++)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        return;
    }

    if (resp.status == 401) {
        spdlog::error("RelayClient: auth rejected, clearing client ID");
        clientId_.clear();
        connected_ = false;
        return;
    }

    if (resp.status != 200) {
        spdlog::warn("RelayClient: poll returned HTTP {}", resp.status);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return;
    }

    connected_ = true;

    try {
        auto j = nlohmann::json::parse(resp.body);
        std::string type = j.value("type", "");

        if (type == "heartbeat") return;

        if (type == "request") {
            std::string requestId = j.value("request_id", "");
            std::string requestType = j.value("request_type", "");
            auto payload = j.value("payload", nlohmann::json::object());

            if (requestType == "chat") {
                processChat(requestId,
                            payload.value("message", ""),
                            payload.value("model", ""),
                            payload.value("session", ""));
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("RelayClient: poll parse error: {}", e.what());
    }
}

void RelayClient::processChat(const std::string& requestId, const std::string& message,
                                const std::string& model, const std::string& session) {
    spdlog::info("RelayClient: processing chat request {}", requestId);

    std::string localUrl = "http://127.0.0.1:" + std::to_string(localPort_) + "/api/chat";

    nlohmann::json chatBody;
    chatBody["message"] = message;
    if (!model.empty()) chatBody["model"] = model;
    if (!session.empty()) chatBody["session"] = session;
    std::string payload = chatBody.dump();

    // Stream the local chat response back to the server
    StreamCtx streamCtx{serverUrl_, authToken_, requestId};

    CURL* curl = curl_easy_init();
    if (!curl) {
        nlohmann::json done = {{"request_id", requestId}};
        httpPost(serverUrl_ + "/api/relay/done", done.dump(), authToken_, 5);
        return;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream");

    curl_easy_setopt(curl, CURLOPT_URL, localUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, localStreamCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &streamCtx);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    nlohmann::json done = {{"request_id", requestId}};
    httpPost(serverUrl_ + "/api/relay/done", done.dump(), authToken_, 5);

    spdlog::info("RelayClient: chat request {} completed", requestId);
}

} // namespace avacli
