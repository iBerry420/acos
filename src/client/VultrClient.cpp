#include "client/VultrClient.hpp"
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <cstdint>

namespace avacli {

namespace {

std::string base64Encode(const unsigned char* data, size_t len) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len * 4 / 3) + 4);
    for (size_t i = 0; i < len;) {
        uint32_t a = i < len ? static_cast<uint8_t>(data[i++]) : 0;
        uint32_t b = i < len ? static_cast<uint8_t>(data[i++]) : 0;
        uint32_t c = i < len ? static_cast<uint8_t>(data[i++]) : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        out += b64[(triple >> 18) & 0x3f];
        out += b64[(triple >> 12) & 0x3f];
        out += (i > len + 1) ? '=' : b64[(triple >> 6) & 0x3f];
        out += (i > len) ? '=' : b64[triple & 0x3f];
    }
    return out;
}

size_t curlWriteCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

} // namespace

VultrClient::VultrClient(const std::string& apiKey)
    : apiKey_(apiKey) {}

VultrClient::HttpResult VultrClient::get(const std::string& path) {
    HttpResult result;
    const std::string url = std::string(BASE_URL) + path;

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey_).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    else
        spdlog::error("VultrClient GET {}: {}", path, curl_easy_strerror(res));

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

VultrClient::HttpResult VultrClient::post(const std::string& path, const nlohmann::json& body) {
    HttpResult result;
    const std::string url = std::string(BASE_URL) + path;
    const std::string payload = body.dump();

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey_).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    else
        spdlog::error("VultrClient POST {}: {}", path, curl_easy_strerror(res));

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

VultrClient::HttpResult VultrClient::del(const std::string& path) {
    HttpResult result;
    const std::string url = std::string(BASE_URL) + path;

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey_).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    else
        spdlog::error("VultrClient DELETE {}: {}", path, curl_easy_strerror(res));

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

nlohmann::json VultrClient::listPlans() {
    auto resp = get("/plans");
    if (!resp.ok()) return nlohmann::json::array();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::array(); }
}

nlohmann::json VultrClient::listRegions() {
    auto resp = get("/regions");
    if (!resp.ok()) return nlohmann::json::array();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::array(); }
}

nlohmann::json VultrClient::listInstances() {
    auto resp = get("/instances");
    if (!resp.ok()) return nlohmann::json::object();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::object(); }
}

nlohmann::json VultrClient::getInstance(const std::string& instanceId) {
    auto resp = get("/instances/" + instanceId);
    if (!resp.ok()) return nlohmann::json::object();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::object(); }
}

nlohmann::json VultrClient::createInstance(const std::string& region, const std::string& plan,
                                            const std::string& osId, const std::string& label,
                                            const std::string& startupScriptId,
                                            const std::string& userDataPlain) {
    nlohmann::json body;
    body["region"] = region;
    body["plan"] = plan;
    body["os_id"] = std::stoi(osId);
    if (!label.empty()) body["label"] = label;
    if (!startupScriptId.empty()) body["script_id"] = startupScriptId;
    if (!userDataPlain.empty()) {
        body["user_data"] = base64Encode(reinterpret_cast<const unsigned char*>(userDataPlain.data()),
                                         userDataPlain.size());
    }

    auto resp = post("/instances", body);
    if (!resp.ok()) {
        spdlog::error("VultrClient: createInstance failed HTTP {}", resp.status);
        nlohmann::json err;
        err["error"] = "create failed";
        err["status"] = resp.status;
        try { err["detail"] = nlohmann::json::parse(resp.body); } catch (...) {}
        return err;
    }
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::object(); }
}

bool VultrClient::destroyInstance(const std::string& instanceId) {
    auto resp = del("/instances/" + instanceId);
    return resp.status == 204 || resp.ok();
}

nlohmann::json VultrClient::listOSImages() {
    auto resp = get("/os");
    if (!resp.ok()) return nlohmann::json::array();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::array(); }
}

nlohmann::json VultrClient::getAccount() {
    auto resp = get("/account");
    if (!resp.ok()) return nlohmann::json::object();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::object(); }
}

nlohmann::json VultrClient::getAccountBandwidth() {
    auto resp = get("/account/bandwidth");
    if (!resp.ok()) return nlohmann::json::object();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::object(); }
}

nlohmann::json VultrClient::listPendingCharges() {
    auto resp = get("/billing/pending-charges");
    if (!resp.ok()) return nlohmann::json::object();
    try { return nlohmann::json::parse(resp.body); }
    catch (...) { return nlohmann::json::object(); }
}

} // namespace avacli
