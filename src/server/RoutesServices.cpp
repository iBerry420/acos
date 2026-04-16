#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "db/Database.hpp"
#include "services/ServiceManager.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace avacli {

namespace {
std::string generateId() {
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    static int seq = 0;
    return std::to_string(now) + "-" + std::to_string(seq++);
}
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
}

void registerServiceRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Get("/api/services", [ctx](const httplib::Request&, httplib::Response& res) {
        try {
            auto rows = Database::instance().query(
                "SELECT id, name, description, type, config, status, created_at, updated_at, last_run, next_run "
                "FROM services ORDER BY updated_at DESC");
            for (auto& r : rows) {
                if (r.contains("config") && r["config"].is_string()) {
                    try { r["config"] = nlohmann::json::parse(r["config"].get<std::string>()); } catch (...) {}
                }
                bool running = ServiceManager::instance().isRunning(r["id"].get<std::string>());
                r["live_status"] = running ? "running" : r["status"].get<std::string>();
            }
            nlohmann::json out;
            out["services"] = rows;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post("/api/services", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string name = body.value("name", "");
        if (name.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"name required"})", "application/json");
            return;
        }

        std::string id = generateId();
        auto now = nowMs();
        std::string type = body.value("type", "custom");
        std::string desc = body.value("description", "");
        std::string config = body.contains("config") ? body["config"].dump() : "{}";
        std::string user = getRequestUser(req, ctx.masterKeyMgr);

        try {
            Database::instance().execute(
                "INSERT INTO services (id, name, description, type, config, status, created_at, updated_at, created_by) "
                "VALUES (?1, ?2, ?3, ?4, ?5, 'stopped', ?6, ?6, ?7)",
                {id, name, desc, type, config, std::to_string(now), user});

            LogBuffer::instance().info("services", "Service created: " + name, {{"id", id}, {"type", type}});
            nlohmann::json out;
            out["id"] = id;
            out["created"] = true;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get(R"(/api/services/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto row = Database::instance().queryOne("SELECT * FROM services WHERE id = ?1", {id});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"service not found"})", "application/json");
                return;
            }
            if (row.contains("config") && row["config"].is_string()) {
                try { row["config"] = nlohmann::json::parse(row["config"].get<std::string>()); } catch (...) {}
            }
            row["live_status"] = ServiceManager::instance().isRunning(id) ? "running" : row["status"].get<std::string>();
            res.set_content(row.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Put(R"(/api/services/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        auto now = std::to_string(nowMs());
        try {
            auto& db = Database::instance();
            if (body.contains("name"))
                db.execute("UPDATE services SET name = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["name"].get<std::string>(), now, id});
            if (body.contains("description"))
                db.execute("UPDATE services SET description = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["description"].get<std::string>(), now, id});
            if (body.contains("config"))
                db.execute("UPDATE services SET config = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["config"].dump(), now, id});
            if (body.contains("type"))
                db.execute("UPDATE services SET type = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["type"].get<std::string>(), now, id});
            res.set_content(R"({"updated":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Delete(R"(/api/services/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            ServiceManager::instance().stopService(id);
            Database::instance().execute("DELETE FROM service_logs WHERE service_id = ?1", {id});
            Database::instance().execute("DELETE FROM services WHERE id = ?1", {id});
            LogBuffer::instance().info("services", "Service deleted", {{"id", id}});
            res.set_content(R"({"deleted":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post(R"(/api/services/([^/]+)/start)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto& db = Database::instance();
            auto svc = db.queryOne("SELECT type, config FROM services WHERE id = ?1", {id});
            if (svc.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"service not found"})", "application/json");
                return;
            }
            std::string type = svc["type"].get<std::string>();
            nlohmann::json config;
            std::string cfgStr = svc.value("config", std::string("{}"));
            try { config = nlohmann::json::parse(cfgStr); } catch (...) {}

            if (type == "scheduled_prompt" && config.value("prompt", "").empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Cannot start: scheduled_prompt requires 'prompt' in config"})", "application/json");
                return;
            }
            if (type == "rss_feed" && config.value("url", "").empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Cannot start: rss_feed requires 'url' in config"})", "application/json");
                return;
            }

            ServiceManager::instance().startService(id);
            db.execute("UPDATE services SET status = 'running', updated_at = ?1 WHERE id = ?2",
                {std::to_string(nowMs()), id});
            LogBuffer::instance().info("services", "Service started", {{"id", id}});
            res.set_content(R"({"started":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post(R"(/api/services/([^/]+)/stop)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            ServiceManager::instance().stopService(id);
            Database::instance().execute("UPDATE services SET status = 'stopped', updated_at = ?1 WHERE id = ?2",
                {std::to_string(nowMs()), id});
            LogBuffer::instance().info("services", "Service stopped", {{"id", id}});
            res.set_content(R"({"stopped":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post(R"(/api/services/([^/]+)/restart)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            ServiceManager::instance().stopService(id);
            ServiceManager::instance().startService(id);
            Database::instance().execute("UPDATE services SET status = 'running', updated_at = ?1 WHERE id = ?2",
                {std::to_string(nowMs()), id});
            LogBuffer::instance().info("services", "Service restarted", {{"id", id}});
            res.set_content(R"({"restarted":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get(R"(/api/services/([^/]+)/logs)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        int limit = 100;
        try { auto l = req.get_param_value("limit"); if (!l.empty()) limit = std::stoi(l); } catch (...) {}

        try {
            auto rows = Database::instance().query(
                "SELECT id, timestamp, level, message FROM service_logs "
                "WHERE service_id = ?1 ORDER BY timestamp DESC LIMIT ?2",
                {id, std::to_string(limit)});
            nlohmann::json out;
            out["logs"] = rows;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    // ── Process service supervisor status ───────────────────────────
    svr.Get(R"(/api/services/([^/]+)/status)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto row = Database::instance().queryOne(
                "SELECT type, status, pid, started_at, restart_count, last_exit_code "
                "FROM services WHERE id = ?1", {id});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"service not found"})", "application/json");
                return;
            }
            nlohmann::json out;
            out["id"]             = id;
            out["type"]           = row["type"];
            out["status"]         = row["status"];
            out["pid"]            = row.value("pid", (long long)0);
            out["started_at"]     = row.value("started_at", (long long)0);
            out["restart_count"]  = row.value("restart_count", (long long)0);
            out["last_exit_code"] = row.value("last_exit_code", (long long)0);
            out["live"]           = ServiceManager::instance().isRunning(id);

            // Live process details (only meaningful for type="process" while running).
            auto live = ServiceManager::instance().processStatus(id);
            if (!live.is_null()) out["process"] = live;

            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    // ── SSE tail of a service's logs ───────────────────────────────
    // Emits an initial backfill (last ?tail= rows, default 100, max 500)
    // then streams new rows as they're appended to service_logs. Closes
    // automatically if the service is deleted (row disappears).
    //
    // Protocol: `data: <json>\n\n` where json has {id, timestamp, level, message}.
    // A heartbeat comment `: ping\n\n` is sent every 20s so proxies don't
    // idle the connection.
    svr.Get(R"(/api/services/([^/]+)/logs/stream)",
            [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        int tailLines = 100;
        try {
            auto t = req.get_param_value("tail");
            if (!t.empty()) tailLines = std::stoi(t);
        } catch (...) {}
        if (tailLines < 0) tailLines = 0;
        if (tailLines > 500) tailLines = 500;

        // Fast-fail if the service doesn't exist so clients don't hang on a
        // 200 SSE stream that will never emit anything.
        try {
            auto row = Database::instance().queryOne(
                "SELECT 1 FROM services WHERE id = ?1", {id});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"service not found"})", "application/json");
                return;
            }
        } catch (...) {
            res.status = 500;
            res.set_content(R"({"error":"db error"})", "application/json");
            return;
        }

        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider(
            "text/event-stream",
            [id, tailLines](size_t, httplib::DataSink& sink) -> bool {
                long long lastId = 0;

                // Initial backfill: fetch the newest N rows, emit oldest-first.
                try {
                    auto rows = Database::instance().query(
                        "SELECT id, timestamp, level, message FROM service_logs "
                        "WHERE service_id = ?1 ORDER BY id DESC LIMIT ?2",
                        {id, std::to_string(tailLines)});
                    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
                        long long rid = 0;
                        if (it->contains("id") && (*it)["id"].is_number_integer())
                            rid = (*it)["id"].get<long long>();
                        if (rid > lastId) lastId = rid;
                        std::string frame = "data: " + it->dump() + "\n\n";
                        if (!sink.write(frame.data(), frame.size())) return false;
                    }
                } catch (...) {
                    std::string frame = "data: {\"error\":\"db read failed\"}\n\n";
                    sink.write(frame.data(), frame.size());
                    sink.done();
                    return false;
                }

                // Announce the backfill boundary so clients can discriminate
                // historical rows from live ones if they care.
                {
                    std::string boundary = "data: {\"type\":\"live\"}\n\n";
                    if (!sink.write(boundary.data(), boundary.size())) return false;
                }

                using clock = std::chrono::steady_clock;
                auto lastPing = clock::now();

                // Poll loop. 500 ms granularity is plenty for human-readable
                // log tailing and is cheap with the idx_svc_logs_svc_ts_desc
                // / auto-increment id index migration v4 added.
                while (true) {
                    // Existence check: if the service was deleted, end the stream.
                    try {
                        auto chk = Database::instance().queryOne(
                            "SELECT 1 FROM services WHERE id = ?1", {id});
                        if (chk.is_null()) {
                            std::string bye = "data: {\"type\":\"gone\"}\n\n";
                            sink.write(bye.data(), bye.size());
                            sink.done();
                            return false;
                        }
                    } catch (...) { /* transient -- keep trying */ }

                    // Pull any rows with id > lastId in ascending order.
                    try {
                        auto rows = Database::instance().query(
                            "SELECT id, timestamp, level, message FROM service_logs "
                            "WHERE service_id = ?1 AND id > ?2 ORDER BY id ASC LIMIT 200",
                            {id, std::to_string(lastId)});
                        for (auto& r : rows) {
                            long long rid = 0;
                            if (r.contains("id") && r["id"].is_number_integer())
                                rid = r["id"].get<long long>();
                            if (rid > lastId) lastId = rid;
                            std::string frame = "data: " + r.dump() + "\n\n";
                            if (!sink.write(frame.data(), frame.size())) return false;
                        }
                    } catch (...) { /* ignore, retry next tick */ }

                    // Heartbeat every 20s to keep proxies happy.
                    auto now = clock::now();
                    if (now - lastPing > std::chrono::seconds(20)) {
                        std::string ping = ": ping\n\n";
                        if (!sink.write(ping.data(), ping.size())) return false;
                        lastPing = now;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            });
    });

    svr.Post(R"(/api/services/([^/]+)/signal)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string signal = body.value("signal", "");
        if (signal != "term" && signal != "kill" && signal != "restart") {
            res.status = 400;
            res.set_content(R"({"error":"signal must be one of: term, kill, restart"})", "application/json");
            return;
        }
        bool delivered = ServiceManager::instance().signalProcess(id, signal);
        if (!delivered) {
            res.status = 409;
            res.set_content(R"({"error":"service is not a running process-type service"})", "application/json");
            return;
        }
        LogBuffer::instance().info("services", "Signal " + signal + " sent", {{"id", id}});
        nlohmann::json out;
        out["delivered"] = true;
        out["signal"] = signal;
        res.set_content(out.dump(), "application/json");
    });
}

} // namespace avacli
