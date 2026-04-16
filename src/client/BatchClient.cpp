#include "client/BatchClient.hpp"

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <sstream>

namespace avacli {

namespace {

size_t writeBodyCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    const size_t total = size * nmemb;
    out->append(ptr, total);
    return total;
}

std::string urlEncode(const std::string& s) {
    CURL* c = curl_easy_init();
    if (!c) return s;
    char* esc = curl_easy_escape(c, s.c_str(), static_cast<int>(s.size()));
    std::string out = esc ? esc : s;
    if (esc) curl_free(esc);
    curl_easy_cleanup(c);
    return out;
}

size_t getSizeField(const nlohmann::json& j, const char* k) {
    if (!j.contains(k)) return 0;
    if (!j[k].is_number_integer() && !j[k].is_number_unsigned()) return 0;
    return j[k].get<size_t>();
}

int64_t getI64Field(const nlohmann::json& j, const char* k) {
    if (!j.contains(k)) return 0;
    if (!j[k].is_number_integer() && !j[k].is_number_unsigned()) return 0;
    return j[k].get<int64_t>();
}

/// Extract token counts from a response envelope. Handles all four variants
/// (chat_get_completion, responses, image_generation, video_generation).
void extractUsage(const nlohmann::json& resp, BatchClient::BatchResult& out) {
    const nlohmann::json* usage = nullptr;
    for (const char* kind : {"chat_get_completion", "responses",
                             "image_generation", "video_generation"}) {
        if (resp.contains(kind) && resp[kind].is_object()
            && resp[kind].contains("usage") && resp[kind]["usage"].is_object()) {
            usage = &resp[kind]["usage"];
            break;
        }
    }
    if (!usage) return;

    out.promptTokens     = getSizeField(*usage, "prompt_tokens");
    if (!out.promptTokens) out.promptTokens = getSizeField(*usage, "input_tokens");
    out.completionTokens = getSizeField(*usage, "completion_tokens");
    if (!out.completionTokens) out.completionTokens = getSizeField(*usage, "output_tokens");

    if (usage->contains("prompt_tokens_details")
        && (*usage)["prompt_tokens_details"].is_object()
        && (*usage)["prompt_tokens_details"].contains("cached_tokens")) {
        out.cachedTokens = getSizeField((*usage)["prompt_tokens_details"], "cached_tokens");
    } else if (usage->contains("input_tokens_details")
               && (*usage)["input_tokens_details"].is_object()
               && (*usage)["input_tokens_details"].contains("cached_tokens")) {
        out.cachedTokens = getSizeField((*usage)["input_tokens_details"], "cached_tokens");
    }

    if (usage->contains("completion_tokens_details")
        && (*usage)["completion_tokens_details"].is_object()
        && (*usage)["completion_tokens_details"].contains("reasoning_tokens")) {
        out.reasoningTokens = getSizeField((*usage)["completion_tokens_details"], "reasoning_tokens");
    } else if (usage->contains("output_tokens_details")
               && (*usage)["output_tokens_details"].is_object()
               && (*usage)["output_tokens_details"].contains("reasoning_tokens")) {
        out.reasoningTokens = getSizeField((*usage)["output_tokens_details"], "reasoning_tokens");
    } else if (usage->contains("reasoning_tokens")) {
        out.reasoningTokens = getSizeField(*usage, "reasoning_tokens");
    }

    out.costUsdTicks = getI64Field(*usage, "cost_in_usd_ticks");
}

} // namespace

BatchClient::BatchClient(const std::string& apiKey, const std::string& baseUrl)
    : apiKey_(apiKey), baseUrl_(baseUrl) {
    while (!baseUrl_.empty() && baseUrl_.back() == '/') baseUrl_.pop_back();
}

long BatchClient::httpRequest(const std::string& method,
                              const std::string& url,
                              const std::string& body,
                              std::string& out) {
    lastError_.clear();
    out.clear();
    lastHttpBody_.clear();

    if (apiKey_.empty()) {
        lastError_ = "XAI_API_KEY not set";
        return 0;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        lastError_ = "curl_easy_init failed";
        return 0;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    {
        std::string auth = "Authorization: Bearer " + apiKey_;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBodyCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode rc = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    lastHttpBody_ = out;

    if (rc != CURLE_OK) {
        lastError_ = std::string("curl: ") + curl_easy_strerror(rc);
        return 0;
    }
    if (httpCode >= 400) {
        std::ostringstream oss;
        oss << "HTTP " << httpCode;
        if (!out.empty()) {
            try {
                auto j = nlohmann::json::parse(out);
                if (j.contains("error")) {
                    if (j["error"].is_object() && j["error"].contains("message"))
                        oss << ": " << j["error"]["message"].get<std::string>();
                    else if (j["error"].is_string())
                        oss << ": " << j["error"].get<std::string>();
                }
            } catch (...) {
                oss << ": " << out.substr(0, 256);
            }
        }
        lastError_ = oss.str();
    }
    return httpCode;
}

std::string BatchClient::create(const std::string& name) {
    nlohmann::json req;
    req["name"] = name;

    std::string respBody;
    long status = httpRequest("POST", baseUrl_ + "/batches", req.dump(), respBody);
    if (status < 200 || status >= 300) {
        spdlog::warn("[batch] create failed: {}", lastError_);
        return "";
    }

    try {
        auto j = nlohmann::json::parse(respBody);
        if (j.contains("batch_id") && j["batch_id"].is_string()) {
            std::string id = j["batch_id"].get<std::string>();
            spdlog::info("[batch] created '{}' -> {}", name, id);
            return id;
        }
        if (j.contains("id") && j["id"].is_string()) {
            return j["id"].get<std::string>();
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("parse create response: ") + e.what();
    }
    return "";
}

bool BatchClient::addRequests(const std::string& batchId,
                              const std::vector<nlohmann::json>& requests) {
    if (requests.empty()) {
        lastError_ = "addRequests: empty list";
        return false;
    }

    nlohmann::json body;
    body["batch_requests"] = requests;

    std::string respBody;
    long status = httpRequest("POST",
                              baseUrl_ + "/batches/" + urlEncode(batchId) + "/requests",
                              body.dump(),
                              respBody);
    if (status < 200 || status >= 300) {
        spdlog::warn("[batch] addRequests({}) failed: {}", batchId, lastError_);
        return false;
    }
    spdlog::info("[batch] added {} request(s) to {}", requests.size(), batchId);
    return true;
}

std::optional<BatchClient::BatchState> BatchClient::get(const std::string& batchId) {
    std::string respBody;
    long status = httpRequest("GET",
                              baseUrl_ + "/batches/" + urlEncode(batchId),
                              "",
                              respBody);
    if (status < 200 || status >= 300) return std::nullopt;

    try {
        auto j = nlohmann::json::parse(respBody);
        BatchState bs;
        bs.raw = j;
        bs.batchId = j.value("batch_id", j.value("id", ""));
        bs.name    = j.value("name", "");

        if (j.contains("state") && j["state"].is_object()) {
            const auto& s = j["state"];
            bs.numRequests  = static_cast<int>(getSizeField(s, "num_requests"));
            bs.numPending   = static_cast<int>(getSizeField(s, "num_pending"));
            bs.numSuccess   = static_cast<int>(getSizeField(s, "num_success"));
            bs.numError     = static_cast<int>(getSizeField(s, "num_error"));
            bs.numCancelled = static_cast<int>(getSizeField(s, "num_cancelled"));
        } else if (j.contains("state") && j["state"].is_string()) {
            bs.state = j["state"].get<std::string>();
        }

        // Derive aggregate state when the server returns only counters.
        if (bs.state.empty()) {
            if (bs.numRequests > 0 && bs.numPending == 0) {
                if (bs.numCancelled > 0 && bs.numSuccess == 0)
                    bs.state = "cancelled";
                else
                    bs.state = "complete";
            } else {
                bs.state = "processing";
            }
        }

        if (j.contains("cost_breakdown") && j["cost_breakdown"].is_object()) {
            bs.costUsdTicks = getI64Field(j["cost_breakdown"], "total_cost_usd_ticks");
        } else {
            bs.costUsdTicks = getI64Field(j, "total_cost_usd_ticks");
        }

        bs.expiresAt = getI64Field(j, "expires_at");

        return bs;
    } catch (const std::exception& e) {
        lastError_ = std::string("parse get response: ") + e.what();
        return std::nullopt;
    }
}

BatchClient::ResultsPage BatchClient::listResults(const std::string& batchId,
                                                  int limit,
                                                  const std::string& pageToken) {
    ResultsPage page;
    std::ostringstream url;
    url << baseUrl_ << "/batches/" << urlEncode(batchId) << "/results?limit=" << limit;
    if (!pageToken.empty()) url << "&pagination_token=" << urlEncode(pageToken);

    std::string respBody;
    long status = httpRequest("GET", url.str(), "", respBody);
    if (status < 200 || status >= 300) return page;

    try {
        auto j = nlohmann::json::parse(respBody);
        if (j.contains("pagination_token") && j["pagination_token"].is_string())
            page.nextPageToken = j["pagination_token"].get<std::string>();

        if (j.contains("results") && j["results"].is_array()) {
            for (const auto& r : j["results"]) {
                BatchResult br;
                br.batchRequestId = r.value("batch_request_id", "");

                // Errors can live at top-level or inside batch_result.
                if (r.contains("error_message") && r["error_message"].is_string())
                    br.errorMessage = r["error_message"].get<std::string>();

                if (r.contains("batch_result") && r["batch_result"].is_object()) {
                    const auto& br_j = r["batch_result"];
                    if (br_j.contains("error_message") && br_j["error_message"].is_string())
                        br.errorMessage = br_j["error_message"].get<std::string>();
                    if (br_j.contains("response") && br_j["response"].is_object()) {
                        br.response = br_j["response"];
                        extractUsage(br.response, br);
                    }
                }

                br.succeeded = br.errorMessage.empty() && !br.response.is_null()
                               && !br.response.empty();
                page.results.push_back(std::move(br));
            }
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("parse results: ") + e.what();
    }
    return page;
}

bool BatchClient::cancel(const std::string& batchId) {
    // xAI's cancel endpoint uses the Google-style colon suffix.
    std::string url = baseUrl_ + "/batches/" + urlEncode(batchId) + ":cancel";
    std::string respBody;
    long status = httpRequest("POST", url, "", respBody);
    if (status < 200 || status >= 300) {
        spdlog::warn("[batch] cancel({}) failed: {}", batchId, lastError_);
        return false;
    }
    spdlog::info("[batch] cancelled {}", batchId);
    return true;
}

BatchClient::ListPage BatchClient::list(int limit, const std::string& pageToken) {
    ListPage out;
    std::ostringstream url;
    url << baseUrl_ << "/batches?limit=" << limit;
    if (!pageToken.empty()) url << "&pagination_token=" << urlEncode(pageToken);

    std::string respBody;
    long status = httpRequest("GET", url.str(), "", respBody);
    if (status < 200 || status >= 300) return out;

    try {
        auto j = nlohmann::json::parse(respBody);
        if (j.contains("pagination_token") && j["pagination_token"].is_string())
            out.nextPageToken = j["pagination_token"].get<std::string>();

        if (j.contains("batches") && j["batches"].is_array()) {
            for (const auto& b : j["batches"]) {
                BatchSummary s;
                s.batchId = b.value("batch_id", b.value("id", ""));
                s.name    = b.value("name", "");
                s.createdAt = getI64Field(b, "created_at");
                s.expiresAt = getI64Field(b, "expires_at");
                if (b.contains("state") && b["state"].is_object()) {
                    s.numRequests = static_cast<int>(getSizeField(b["state"], "num_requests"));
                    s.numPending  = static_cast<int>(getSizeField(b["state"], "num_pending"));
                    s.state = (s.numRequests > 0 && s.numPending == 0) ? "complete" : "processing";
                }
                out.batches.push_back(std::move(s));
            }
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("parse list: ") + e.what();
    }
    return out;
}

nlohmann::json BatchClient::makeChatRequest(const std::string& batchRequestId,
                                            const nlohmann::json& chatBody) {
    nlohmann::json envelope;
    envelope["batch_request_id"] = batchRequestId;
    envelope["batch_request"] = { {"chat_get_completion", chatBody} };
    return envelope;
}

nlohmann::json BatchClient::makeResponsesRequest(const std::string& batchRequestId,
                                                 const nlohmann::json& responsesBody) {
    nlohmann::json envelope;
    envelope["batch_request_id"] = batchRequestId;
    envelope["batch_request"] = { {"responses", responsesBody} };
    return envelope;
}

} // namespace avacli
