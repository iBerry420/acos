#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"

#include "client/BatchClient.hpp"
#include "db/Database.hpp"
#include "services/BatchPoller.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <string>

namespace avacli {

namespace {

int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}

/// Strip the trailing `/chat/completions` from the configured chat URL so the
/// BatchClient can form its own `/v1/batches` URLs against the same root.
std::string deriveBaseUrl(const std::string& chatUrl) {
    std::string base = chatUrl.empty()
        ? std::string("https://api.x.ai/v1/chat/completions")
        : chatUrl;
    auto pos = base.find("/chat/completions");
    if (pos != std::string::npos) base.erase(pos);
    while (!base.empty() && base.back() == '/') base.pop_back();
    if (base.empty()) base = "https://api.x.ai/v1";
    return base;
}

/// Build a xAI batch_request envelope from a caller-supplied JSON item.
/// The UI posts arbitrary items; we normalise them into xAI's wire shape:
///   { batch_request_id, batch_request: { <kind>: { ... } } }
///
/// Accepted input shapes:
///   1. Fully-formed envelope — passed through as-is.
///   2. { "chat": { model, messages, ... } }  -> wraps chat_get_completion.
///   3. { "responses": { model, input, ... } } -> wraps responses.
///   4. Bare chat body (has `messages` or `model`) -> wraps chat_get_completion.
///
/// Returns true on success; on failure fills `err` and leaves `out` untouched.
bool normaliseRequest(const nlohmann::json& in, size_t idx,
                      nlohmann::json& out, std::string& err) {
    if (!in.is_object()) {
        err = "request #" + std::to_string(idx) + " must be an object";
        return false;
    }

    std::string rid = in.value("batch_request_id",
                               std::string("req_") + std::to_string(idx));

    // Case 1: already a full envelope.
    if (in.contains("batch_request") && in["batch_request"].is_object()) {
        out = in;
        out["batch_request_id"] = rid;
        return true;
    }

    nlohmann::json body;
    if (in.contains("chat") && in["chat"].is_object()) {
        body["chat_get_completion"] = in["chat"];
    } else if (in.contains("responses") && in["responses"].is_object()) {
        body["responses"] = in["responses"];
    } else if (in.contains("chat_get_completion") && in["chat_get_completion"].is_object()) {
        body["chat_get_completion"] = in["chat_get_completion"];
    } else if (in.contains("image_generation") && in["image_generation"].is_object()) {
        body["image_generation"] = in["image_generation"];
    } else if (in.contains("video_generation") && in["video_generation"].is_object()) {
        body["video_generation"] = in["video_generation"];
    } else if (in.contains("messages") || in.contains("model")) {
        // Bare chat body (a la /v1/chat/completions payload).
        nlohmann::json chat = in;
        chat.erase("batch_request_id");
        body["chat_get_completion"] = chat;
    } else {
        err = "request #" + std::to_string(idx)
            + " must contain one of: chat / responses / batch_request / messages";
        return false;
    }

    out["batch_request_id"] = rid;
    out["batch_request"]    = body;
    return true;
}

nlohmann::json rowToJson(const nlohmann::json& row, bool includeResults) {
    nlohmann::json j;
    j["id"]             = row.value("id", "");
    j["name"]           = row.value("name", "");
    j["owner"]          = row.value("owner", "");
    j["kind"]           = row.value("kind", "user");
    j["state"]          = row.value("state", "processing");
    j["num_requests"]   = row.value("num_requests", 0);
    j["num_pending"]    = row.value("num_pending", 0);
    j["num_success"]    = row.value("num_success", 0);
    j["num_error"]      = row.value("num_error", 0);
    j["num_cancelled"]  = row.value("num_cancelled", 0);
    j["cost_usd_ticks"] = row.value("cost_usd_ticks", 0);
    j["cost_usd"]       = static_cast<double>(row.value("cost_usd_ticks", 0)) / 1e10;
    j["created_at"]     = row.value("created_at", 0);
    j["updated_at"]     = row.value("updated_at", 0);
    j["expires_at"]     = row.value("expires_at", 0);
    j["last_polled"]    = row.value("last_polled", 0);

    if (row.contains("request_ids_json") && row["request_ids_json"].is_string()) {
        try { j["request_ids"] = nlohmann::json::parse(row["request_ids_json"].get<std::string>()); }
        catch (...) { j["request_ids"] = nlohmann::json::array(); }
    }

    if (includeResults
        && row.contains("results_json") && row["results_json"].is_string()
        && !row["results_json"].get<std::string>().empty()) {
        try { j["results"] = nlohmann::json::parse(row["results_json"].get<std::string>()); }
        catch (...) { j["results"] = nlohmann::json::array(); }
    }
    return j;
}

} // namespace

void registerBatchRoutes(httplib::Server& svr, ServerContext ctx) {
    // List batches — filter by owner / kind / state.
    svr.Get("/api/batches", [](const httplib::Request& req, httplib::Response& res) {
        std::string owner = req.has_param("owner") ? req.get_param_value("owner") : "";
        std::string kind  = req.has_param("kind")  ? req.get_param_value("kind")  : "";
        std::string state = req.has_param("state") ? req.get_param_value("state") : "";
        int limit = 100;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
        }
        if (limit < 1) limit = 1;
        if (limit > 1000) limit = 1000;

        std::string sql = "SELECT * FROM batches WHERE 1=1";
        std::vector<std::string> params;
        if (!owner.empty()) { sql += " AND owner = ?" + std::to_string(params.size() + 1); params.push_back(owner); }
        if (!kind.empty())  { sql += " AND kind = ?"  + std::to_string(params.size() + 1); params.push_back(kind); }
        if (!state.empty()) { sql += " AND state = ?" + std::to_string(params.size() + 1); params.push_back(state); }
        sql += " ORDER BY created_at DESC LIMIT " + std::to_string(limit);

        try {
            auto rows = Database::instance().query(sql, params);
            nlohmann::json out = nlohmann::json::array();
            for (const auto& r : rows) out.push_back(rowToJson(r, false));
            res.set_content(nlohmann::json{{"batches", out},
                                           {"count", out.size()}}.dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Get one batch (with results if complete).
    svr.Get(R"(/api/batches/([A-Za-z0-9_\-:.]+))",
            [](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto row = Database::instance().queryOne(
                "SELECT * FROM batches WHERE id = ?1", {id});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"not_found"})", "application/json");
                return;
            }
            res.set_content(rowToJson(row, true).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Get results — paginated. Uses our cached results_json when available,
    // falls back to a live fetch from xAI when the batch is still processing
    // or the snapshot isn't ready yet.
    svr.Get(R"(/api/batches/([A-Za-z0-9_\-:.]+)/results)",
            [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        int limit = 100;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
        }
        if (limit < 1) limit = 1;
        if (limit > 500) limit = 500;
        std::string token = req.has_param("pagination_token")
            ? req.get_param_value("pagination_token") : "";

        try {
            auto row = Database::instance().queryOne(
                "SELECT results_json, state FROM batches WHERE id = ?1", {id});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"not_found"})", "application/json");
                return;
            }

            std::string cached = row.value("results_json", "");
            if (!cached.empty() && token.empty()) {
                try {
                    auto j = nlohmann::json::parse(cached);
                    res.set_content(nlohmann::json{
                        {"results", j},
                        {"count", j.size()},
                        {"source", "cache"}
                    }.dump(), "application/json");
                    return;
                } catch (...) {}
            }
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
            return;
        }

        std::string apiKey, chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"xai_api_key_missing"})", "application/json");
            return;
        }
        BatchClient client(apiKey, deriveBaseUrl(chatUrl));
        auto page = client.listResults(id, limit, token);

        nlohmann::json results = nlohmann::json::array();
        for (const auto& r : page.results) {
            nlohmann::json j;
            j["batch_request_id"]  = r.batchRequestId;
            j["succeeded"]         = r.succeeded;
            if (!r.errorMessage.empty())
                j["error_message"] = r.errorMessage;
            j["response"]          = r.response;
            j["prompt_tokens"]     = r.promptTokens;
            j["completion_tokens"] = r.completionTokens;
            j["cached_tokens"]     = r.cachedTokens;
            j["reasoning_tokens"]  = r.reasoningTokens;
            j["cost_usd_ticks"]    = r.costUsdTicks;
            results.push_back(j);
        }

        res.set_content(nlohmann::json{
            {"results", results},
            {"count", results.size()},
            {"pagination_token", page.nextPageToken},
            {"source", "live"}
        }.dump(), "application/json");
    });

    // Create a batch — optionally adds requests in the same call.
    svr.Post("/api/batches", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_json"})", "application/json");
            return;
        }

        std::string name = body.value("name", "");
        if (name.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"name_required"})", "application/json");
            return;
        }
        std::string kind  = body.value("kind", "user");
        std::string owner = body.value("owner", "");
        std::string metadata = body.contains("metadata") ? body["metadata"].dump() : "{}";

        std::vector<nlohmann::json> normalised;
        std::vector<std::string> rids;
        if (body.contains("requests") && body["requests"].is_array()) {
            size_t idx = 0;
            for (const auto& r : body["requests"]) {
                nlohmann::json norm;
                std::string err;
                if (!normaliseRequest(r, idx++, norm, err)) {
                    res.status = 400;
                    res.set_content(nlohmann::json{{"error", "invalid_request"},
                                                   {"message", err}}.dump(),
                                    "application/json");
                    return;
                }
                rids.push_back(norm["batch_request_id"].get<std::string>());
                normalised.push_back(std::move(norm));
            }
        }

        std::string apiKey, chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"xai_api_key_missing"})", "application/json");
            return;
        }

        BatchClient client(apiKey, deriveBaseUrl(chatUrl));
        std::string batchId = client.create(name);
        if (batchId.empty()) {
            res.status = 502;
            res.set_content(nlohmann::json{{"error", "xai_create_failed"},
                                           {"message", client.lastError()}}.dump(),
                            "application/json");
            return;
        }

        if (!normalised.empty()) {
            if (!client.addRequests(batchId, normalised)) {
                res.status = 502;
                res.set_content(nlohmann::json{{"error", "xai_add_requests_failed"},
                                               {"message", client.lastError()},
                                               {"batch_id", batchId}}.dump(),
                                "application/json");
                return;
            }
        }

        nlohmann::json ridsJson = rids;
        int64_t now = nowSec();
        try {
            Database::instance().execute(
                "INSERT INTO batches (id, name, owner, kind, created_at, updated_at, "
                "state, num_requests, num_pending, request_ids_json, metadata) "
                "VALUES (?1, ?2, ?3, ?4, ?5, ?5, 'processing', ?6, ?6, ?7, ?8)",
                {batchId, name, owner, kind, std::to_string(now),
                 std::to_string(normalised.size()), ridsJson.dump(), metadata});
        } catch (const std::exception& e) {
            spdlog::warn("[batch] insert row: {}", e.what());
        }

        LogBuffer::instance().info("batch", "Batch created: " + name,
            {{"id", batchId}, {"kind", kind}, {"num_requests", normalised.size()}});

        // Kick the poller so the UI sees progress quickly.
        BatchPoller::instance().pollNow();

        res.status = 201;
        res.set_content(nlohmann::json{
            {"batch_id",      batchId},
            {"id",            batchId},
            {"name",          name},
            {"kind",          kind},
            {"owner",         owner},
            {"num_requests",  normalised.size()},
            {"state",         "processing"},
            {"request_ids",   rids},
            {"created_at",    now}
        }.dump(), "application/json");
    });

    // Add more requests to an existing batch.
    svr.Post(R"(/api/batches/([A-Za-z0-9_\-:.]+)/requests)",
             [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_json"})", "application/json");
            return;
        }
        if (!body.contains("requests") || !body["requests"].is_array() || body["requests"].empty()) {
            res.status = 400;
            res.set_content(R"({"error":"requests_required"})", "application/json");
            return;
        }

        std::vector<nlohmann::json> normalised;
        std::vector<std::string> rids;
        size_t idx = 0;
        for (const auto& r : body["requests"]) {
            nlohmann::json norm;
            std::string err;
            if (!normaliseRequest(r, idx++, norm, err)) {
                res.status = 400;
                res.set_content(nlohmann::json{{"error","invalid_request"},
                                               {"message",err}}.dump(),
                                "application/json");
                return;
            }
            rids.push_back(norm["batch_request_id"].get<std::string>());
            normalised.push_back(std::move(norm));
        }

        std::string apiKey, chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"xai_api_key_missing"})", "application/json");
            return;
        }
        BatchClient client(apiKey, deriveBaseUrl(chatUrl));
        if (!client.addRequests(id, normalised)) {
            res.status = 502;
            res.set_content(nlohmann::json{{"error","xai_add_requests_failed"},
                                           {"message",client.lastError()}}.dump(),
                            "application/json");
            return;
        }

        try {
            auto row = Database::instance().queryOne(
                "SELECT num_requests, request_ids_json FROM batches WHERE id = ?1", {id});
            if (!row.is_null()) {
                nlohmann::json existing = nlohmann::json::array();
                try { existing = nlohmann::json::parse(row.value("request_ids_json", "[]")); }
                catch (...) {}
                for (const auto& r : rids) existing.push_back(r);
                int newTotal = row.value("num_requests", 0) + static_cast<int>(rids.size());
                int64_t now = nowSec();
                Database::instance().execute(
                    "UPDATE batches SET num_requests = ?1, num_pending = num_pending + ?2, "
                    "request_ids_json = ?3, updated_at = ?4 WHERE id = ?5",
                    {std::to_string(newTotal), std::to_string(rids.size()),
                     existing.dump(), std::to_string(now), id});
            }
        } catch (const std::exception& e) {
            spdlog::warn("[batch] add-requests update: {}", e.what());
        }

        res.set_content(nlohmann::json{
            {"added", rids.size()},
            {"request_ids", rids}
        }.dump(), "application/json");
    });

    // Cancel.
    svr.Post(R"(/api/batches/([A-Za-z0-9_\-:.]+)/cancel)",
             [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        std::string apiKey, chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"xai_api_key_missing"})", "application/json");
            return;
        }
        BatchClient client(apiKey, deriveBaseUrl(chatUrl));
        bool ok = client.cancel(id);
        if (!ok) {
            res.status = 502;
            res.set_content(nlohmann::json{{"error","xai_cancel_failed"},
                                           {"message",client.lastError()}}.dump(),
                            "application/json");
            return;
        }

        try {
            Database::instance().execute(
                "UPDATE batches SET state = 'cancelled', updated_at = ?1 WHERE id = ?2",
                {std::to_string(nowSec()), id});
        } catch (...) {}

        // One last poll to persist any results that completed before cancel landed.
        BatchPoller::instance().pollNow();
        res.set_content(nlohmann::json{{"cancelled", true}, {"id", id}}.dump(),
                        "application/json");
    });

    // Force a poll tick for a single batch.
    svr.Post(R"(/api/batches/([A-Za-z0-9_\-:.]+)/refresh)",
             [](const httplib::Request&, httplib::Response& res) {
        BatchPoller::instance().pollNow();
        res.set_content(R"({"polling": true})", "application/json");
    });

    // Force a global poll tick.
    svr.Post("/api/batches/refresh", [](const httplib::Request&, httplib::Response& res) {
        BatchPoller::instance().pollNow();
        res.set_content(R"({"polling": true})", "application/json");
    });

    // Delete a local row. Does NOT cancel the upstream batch — cancel first
    // if that's what the caller meant.
    svr.Delete(R"(/api/batches/([A-Za-z0-9_\-:.]+))",
               [](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            Database::instance().execute("DELETE FROM batches WHERE id = ?1", {id});
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
            return;
        }
        res.set_content(nlohmann::json{{"deleted", true}, {"id", id}}.dump(),
                        "application/json");
    });
}

} // namespace avacli
