#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "db/Database.hpp"

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

void registerKnowledgeRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Get("/api/articles", [ctx](const httplib::Request& req, httplib::Response& res) {
        auto& db = Database::instance();
        std::string search = req.get_param_value("q");
        std::string tag = req.get_param_value("tag");
        int limit = 50;
        try { auto l = req.get_param_value("limit"); if (!l.empty()) limit = std::stoi(l); } catch (...) {}
        int offset = 0;
        try { auto o = req.get_param_value("offset"); if (!o.empty()) offset = std::stoi(o); } catch (...) {}

        try {
            nlohmann::json rows;
            if (!search.empty()) {
                rows = db.query(
                    "SELECT id, title, summary, tags, created_at, updated_at, created_by "
                    "FROM articles WHERE title LIKE ?1 OR content LIKE ?1 OR summary LIKE ?1 "
                    "ORDER BY updated_at DESC LIMIT ?2 OFFSET ?3",
                    {"%" + search + "%", std::to_string(limit), std::to_string(offset)});
            } else if (!tag.empty()) {
                rows = db.query(
                    "SELECT id, title, summary, tags, created_at, updated_at, created_by "
                    "FROM articles WHERE tags LIKE ?1 "
                    "ORDER BY updated_at DESC LIMIT ?2 OFFSET ?3",
                    {"%" + tag + "%", std::to_string(limit), std::to_string(offset)});
            } else {
                rows = db.query(
                    "SELECT id, title, summary, tags, created_at, updated_at, created_by "
                    "FROM articles ORDER BY updated_at DESC LIMIT ?1 OFFSET ?2",
                    {std::to_string(limit), std::to_string(offset)});
            }

            for (auto& r : rows) {
                if (r.contains("tags") && r["tags"].is_string()) {
                    try { r["tags"] = nlohmann::json::parse(r["tags"].get<std::string>()); } catch (...) {}
                }
            }

            auto countRow = db.queryOne("SELECT COUNT(*) as total FROM articles");
            nlohmann::json out;
            out["articles"] = rows;
            out["total"] = countRow.is_null() ? nlohmann::json(0) : countRow["total"];
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get(R"(/api/articles/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto row = Database::instance().queryOne(
                "SELECT * FROM articles WHERE id = ?1", {id});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"article not found"})", "application/json");
                return;
            }
            if (row.contains("tags") && row["tags"].is_string()) {
                try { row["tags"] = nlohmann::json::parse(row["tags"].get<std::string>()); } catch (...) {}
            }
            res.set_content(row.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post("/api/articles", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string title = body.value("title", "");
        if (title.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"title required"})", "application/json");
            return;
        }

        std::string id = generateId();
        auto now = nowMs();
        std::string content = body.value("content", "");
        std::string summary = body.value("summary", "");
        std::string tags = body.contains("tags") ? body["tags"].dump() : "[]";
        std::string createdBy = getRequestUser(req, ctx.masterKeyMgr);
        std::string sourceSession = body.value("source_session", "");

        try {
            Database::instance().execute(
                "INSERT INTO articles (id, title, content, summary, tags, created_at, updated_at, created_by, source_session) "
                "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
                {id, title, content, summary, tags, std::to_string(now), std::to_string(now), createdBy, sourceSession});

            LogBuffer::instance().info("knowledge", "Article created: " + title, {{"id", id}});
            nlohmann::json out;
            out["id"] = id;
            out["created"] = true;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Put(R"(/api/articles/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        auto existing = Database::instance().queryOne("SELECT id FROM articles WHERE id = ?1", {id});
        if (existing.is_null()) {
            res.status = 404;
            res.set_content(R"({"error":"article not found"})", "application/json");
            return;
        }

        auto now = std::to_string(nowMs());
        try {
            if (body.contains("title"))
                Database::instance().execute("UPDATE articles SET title = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["title"].get<std::string>(), now, id});
            if (body.contains("content"))
                Database::instance().execute("UPDATE articles SET content = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["content"].get<std::string>(), now, id});
            if (body.contains("summary"))
                Database::instance().execute("UPDATE articles SET summary = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["summary"].get<std::string>(), now, id});
            if (body.contains("tags"))
                Database::instance().execute("UPDATE articles SET tags = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["tags"].dump(), now, id});

            LogBuffer::instance().info("knowledge", "Article updated", {{"id", id}});
            res.set_content(R"({"updated":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Delete(R"(/api/articles/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        std::string id = req.matches[1];
        try {
            Database::instance().execute("DELETE FROM articles WHERE id = ?1", {id});
            LogBuffer::instance().info("knowledge", "Article deleted", {{"id", id}});
            res.set_content(R"({"deleted":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });
}

} // namespace avacli
