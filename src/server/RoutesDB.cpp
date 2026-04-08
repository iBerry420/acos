#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "db/Database.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace avacli {

void registerDBRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Post("/api/db/query", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string sql = body.value("sql", "");
        if (sql.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"sql required"})", "application/json");
            return;
        }

        std::vector<std::string> params;
        if (body.contains("params") && body["params"].is_array()) {
            for (const auto& p : body["params"]) {
                params.push_back(p.is_string() ? p.get<std::string>() : p.dump());
            }
        }

        try {
            auto& db = Database::instance();
            std::string upper;
            for (char c : sql) upper += static_cast<char>(toupper(c));

            bool isSelect = (upper.find("SELECT") == 0) ||
                            (upper.find(" SELECT") != std::string::npos && upper.find("SELECT") < 10);
            bool isReadOnly = isSelect &&
                              upper.find("INSERT") == std::string::npos &&
                              upper.find("UPDATE") == std::string::npos &&
                              upper.find("DELETE") == std::string::npos &&
                              upper.find("DROP") == std::string::npos &&
                              upper.find("ALTER") == std::string::npos;

            if (isReadOnly) {
                auto rows = db.query(sql, params);
                nlohmann::json out;
                out["rows"] = rows;
                out["count"] = rows.size();
                res.set_content(out.dump(), "application/json");
            } else {
                auto changes = db.execute(sql, params);
                nlohmann::json out;
                out["changes"] = changes;
                res.set_content(out.dump(), "application/json");
            }

            LogBuffer::instance().debug("db", "Query executed", {{"sql", sql.substr(0, 200)}});
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json out;
            out["error"] = e.what();
            res.set_content(out.dump(), "application/json");
        }
    });
}

} // namespace avacli
