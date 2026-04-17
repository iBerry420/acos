#include "client/NodeClient.hpp"
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <atomic>
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

// Context used by proxyRawPost's write callback. Needs access to the curl handle so
// it can query status + content-type on the first byte and fire onHeaders exactly once.
struct RawPostCtx {
    CURL* curl;
    NodeClient::HeadersCallback onHeaders;
    NodeClient::SseCallback onChunk;
    bool headersNotified = false;
};

size_t rawPostWriteCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* c = static_cast<RawPostCtx*>(user);
    if (!c->headersNotified) {
        long status = 0;
        curl_easy_getinfo(c->curl, CURLINFO_RESPONSE_CODE, &status);
        char* ct = nullptr;
        curl_easy_getinfo(c->curl, CURLINFO_CONTENT_TYPE, &ct);
        c->onHeaders(status, ct ? ct : "");
        c->headersNotified = true;
    }
    std::string chunk(ptr, size * nmemb);
    c->onChunk(chunk);
    return size * nmemb;
}

} // namespace

NodeClient::NodeClient(const NodeInfo& node) : node_(node) {}

std::string NodeClient::baseUrl() const {
    // Build canonical http(s)://host[:port] URL, tolerating what the user typed:
    //  - host may be a bare hostname/IP, an "http://"/"https://" prefixed URL, or include a trailing slash
    //  - port may be 80 (Apache-fronted → actually HTTPS via redirect), 443 (direct HTTPS), or anything else
    //
    // Scheme picking rules:
    //   1. Explicit http:// or https:// on the host field wins.
    //   2. port 443 → https
    //   3. port 80 with a Apache-style hostname (contains a dot, not an IP) — still start on http;
    //      FOLLOWLOCATION will bounce us to https if needed. We intentionally do NOT auto-upgrade to
    //      https on port 80, because some users genuinely run plain-http avacli on port 80.
    //   4. otherwise http
    //
    // We also strip the default port (`:80` for http, `:443` for https) from the final URL for cleanliness
    // and to avoid some edge cases where proxies care about the exact Host header.

    std::string host = node_.domain.empty() ? node_.ip : node_.domain;
    int port = node_.port;
    std::string scheme;

    if (host.rfind("https://", 0) == 0) {
        scheme = "https";
        host = host.substr(8);
    } else if (host.rfind("http://", 0) == 0) {
        scheme = "http";
        host = host.substr(7);
    }

    // Strip any path/trailing slash that snuck in
    auto slash = host.find('/');
    if (slash != std::string::npos) host = host.substr(0, slash);

    if (scheme.empty()) {
        scheme = (port == 443) ? "https" : "http";
    }

    bool defaultPort = (scheme == "http" && port == 80) || (scheme == "https" && port == 443);
    if (defaultPort) return scheme + "://" + host;
    return scheme + "://" + host + ":" + std::to_string(port);
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
    // Stream POSTs also need to follow http→https redirects (Apache frontends) and stay POST.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_POSTREDIR,
        (long)(CURL_REDIR_POST_301 | CURL_REDIR_POST_302 | CURL_REDIR_POST_303));

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
    // Follow Apache-style 301/302 http→https redirects so nodes sitting behind a reverse proxy
    // on port 80 still report as "connected" instead of "not_avacli". Our Authorization header
    // is attached via CURLOPT_HTTPHEADER which libcurl retransmits on same-host redirects.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

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
    // Same redirect-following rationale as GET. CURLOPT_POSTREDIR forces libcurl to keep the
    // method as POST on 301/302/303 — by default it downgrades POST to GET on 301/302, which
    // would drop our JSON body silently.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_POSTREDIR,
        (long)(CURL_REDIR_POST_301 | CURL_REDIR_POST_302 | CURL_REDIR_POST_303));

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

bool NodeClient::proxyRawPost(const std::string& path,
                              const std::string& body,
                              const std::string& contentType,
                              HeadersCallback onHeaders,
                              SseCallback onChunk,
                              std::atomic<bool>* cancelFlag) {
    std::string url = baseUrl() + path;

    CURL* curl = curl_easy_init();
    if (!curl) {
        onHeaders(0, "");
        return false;
    }

    std::string ct = contentType.empty() ? std::string("application/json") : contentType;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Content-Type: " + ct).c_str());
    headers = curl_slist_append(headers, "Accept: */*");
    auto token = authToken();
    if (!token.empty())
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());

    RawPostCtx ctx{curl, onHeaders, onChunk, false};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L); // cover long agent runs
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, rawPostWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_POSTREDIR,
        (long)(CURL_REDIR_POST_301 | CURL_REDIR_POST_302 | CURL_REDIR_POST_303));

    if (cancelFlag) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
            +[](void* cp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                auto* f = static_cast<std::atomic<bool>*>(cp);
                return f->load() ? 1 : 0;
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancelFlag);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    CURLcode res = curl_easy_perform(curl);

    // If the remote returned 0 bytes (e.g. an error with empty body), the write cb
    // never ran and onHeaders was never fired — do it now so the queue consumer unblocks.
    if (!ctx.headersNotified) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        char* hct = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &hct);
        onHeaders(status, hct ? hct : "");
    }

    if (res != CURLE_OK) {
        spdlog::debug("NodeClient raw POST {} failed: {}", path, curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

} // namespace avacli
