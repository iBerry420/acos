#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "services/RelayManager.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>

namespace avacli {

static int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static bool validateRelayAuth(const httplib::Request& req, httplib::Response& res) {
    std::string auth = req.get_header_value("Authorization");
    std::string token;
    if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ")
        token = auth.substr(7);

    if (!RelayManager::instance().validateToken(token)) {
        res.status = 401;
        res.set_content(R"({"error":"invalid relay token"})", "application/json");
        return false;
    }
    return true;
}

void registerRelayRoutes(httplib::Server& svr, ServerContext ctx) {

    // Client registers with the server
    svr.Post("/api/relay/register", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (ctx.config->nodeRole != "server") {
            res.status = 403;
            res.set_content(R"({"error":"this node is not in server mode"})", "application/json");
            return;
        }
        if (!validateRelayAuth(req, res)) return;

        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string label = body.value("label", "unknown");
        std::string version = body.value("version", "");
        std::string workspace = body.value("workspace", "");

        auto& rm = RelayManager::instance();
        std::string clientId = rm.registerClient(label, version, workspace, "");

        LogBuffer::instance().info("relay", "Client registered: " + label,
                                   {{"client_id", clientId}});

        nlohmann::json j;
        j["client_id"] = clientId;
        j["registered"] = true;
        res.set_content(j.dump(), "application/json");
    });

    // Client heartbeat + long-poll for pending requests
    svr.Get("/api/relay/poll", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (ctx.config->nodeRole != "server") {
            res.status = 403;
            res.set_content(R"({"error":"this node is not in server mode"})", "application/json");
            return;
        }
        if (!validateRelayAuth(req, res)) return;

        auto it = req.params.find("client_id");
        if (it == req.params.end()) {
            res.status = 400;
            res.set_content(R"({"error":"client_id required"})", "application/json");
            return;
        }
        std::string clientId = it->second;

        auto& rm = RelayManager::instance();
        rm.heartbeat(clientId);

        auto request = rm.pollRequest(clientId, 25);
        if (!request) {
            nlohmann::json j;
            j["type"] = "heartbeat";
            j["timestamp"] = nowSec();
            res.set_content(j.dump(), "application/json");
            return;
        }

        nlohmann::json j;
        j["type"] = "request";
        j["request_id"] = request->requestId;
        j["request_type"] = request->type;
        j["payload"] = request->payload;
        res.set_content(j.dump(), "application/json");
    });

    // Client pushes a response chunk for a pending request
    svr.Post("/api/relay/chunk", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (ctx.config->nodeRole != "server") {
            res.status = 403;
            res.set_content(R"({"error":"this node is not in server mode"})", "application/json");
            return;
        }
        if (!validateRelayAuth(req, res)) return;

        auto it = req.params.find("request_id");
        if (it == req.params.end()) {
            res.status = 400;
            res.set_content(R"({"error":"request_id required"})", "application/json");
            return;
        }

        RelayManager::instance().pushResponseChunk(it->second, req.body);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // Client signals that the response is complete
    svr.Post("/api/relay/done", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (ctx.config->nodeRole != "server") {
            res.status = 403;
            res.set_content(R"({"error":"this node is not in server mode"})", "application/json");
            return;
        }
        if (!validateRelayAuth(req, res)) return;

        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string requestId = body.value("request_id", "");
        if (requestId.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"request_id required"})", "application/json");
            return;
        }

        RelayManager::instance().finishResponse(requestId);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // Client disconnects
    svr.Post("/api/relay/disconnect", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (ctx.config->nodeRole != "server") {
            res.status = 403;
            res.set_content(R"({"error":"this node is not in server mode"})", "application/json");
            return;
        }
        if (!validateRelayAuth(req, res)) return;

        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string clientId = body.value("client_id", "");
        if (!clientId.empty()) {
            RelayManager::instance().disconnectClient(clientId);
            LogBuffer::instance().info("relay", "Client disconnected", {{"client_id", clientId}});
        }
        res.set_content(R"({"ok":true})", "application/json");
    });

    // Server-side: list connected relay clients
    svr.Get("/api/relay/clients", [ctx](const httplib::Request&, httplib::Response& res) {
        auto clients = RelayManager::instance().getClients();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : clients) {
            arr.push_back({
                {"id", c.id},
                {"label", c.label},
                {"version", c.version},
                {"workspace", c.workspace},
                {"connected", c.connected},
                {"connected_at", c.connectedAt},
                {"last_seen", c.lastSeen}
            });
        }
        nlohmann::json j;
        j["clients"] = arr;
        res.set_content(j.dump(), "application/json");
    });

    // Server-side: initiate a chat with a relay client (called by node chat proxy)
    // This SSE endpoint waits for the relay client to process and stream back the response
    svr.Post(R"(/api/relay/chat/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string clientId = req.matches[1];

        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string message = body.value("message", "");
        std::string model = body.value("model", "");
        std::string session = body.value("session", "");

        auto& rm = RelayManager::instance();
        auto relayReq = rm.queueChatRequest(clientId, message, model, session);
        if (!relayReq) {
            res.status = 502;
            res.set_content(R"({"error":"relay client not connected"})", "application/json");
            return;
        }

        auto rq = relayReq->responseQueue;

        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider(
            "text/event-stream",
            [rq](size_t, httplib::DataSink& sink) -> bool {
                std::unique_lock<std::mutex> lock(rq->mu);
                rq->cv.wait_for(lock, std::chrono::seconds(60),
                    [&] { return !rq->chunks.empty() || rq->done; });

                while (!rq->chunks.empty()) {
                    std::string chunk = std::move(rq->chunks.front());
                    rq->chunks.pop();
                    lock.unlock();
                    if (!sink.write(chunk.data(), chunk.size())) return false;
                    lock.lock();
                }

                if (rq->done && rq->chunks.empty()) {
                    sink.done();
                    return false;
                }
                return true;
            });
    });
}

} // namespace avacli
