#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "db/Database.hpp"
#include "services/ServiceManager.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>

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
}

} // namespace avacli
