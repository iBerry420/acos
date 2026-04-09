#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "config/VaultStorage.hpp"
#include "session/SessionManager.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "utils/TimeUtils.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace avacli {

namespace fs = std::filesystem;

void registerDataRoutes(httplib::Server& svr, ServerContext ctx) {

    // ── Vault ────────────────────────────────────────────────────────

    svr.Get("/api/vault", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        LogBuffer::instance().info("api", "Vault operation: list");
        auto entries = VaultStorage::listEntries();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : entries) {
            arr.push_back({{"name", e.name}, {"service", e.service},
                           {"created_at", e.created_at}, {"updated_at", e.updated_at}});
        }
        nlohmann::json result;
        result["entries"] = arr;
        result["count"] = arr.size();
        result["vault_exists"] = VaultStorage::exists();
        res.set_content(result.dump(), "application/json");
    });

    svr.Post("/api/vault", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string action = body.value("action", "store");
        std::string password = body.value("password", "avacli_default");

        if (action == "initialize") {
            bool ok = VaultStorage::initialize(password);
            if (ok) LogBuffer::instance().info("api", "Vault operation: initialize");
            res.set_content(ok ? R"({"status":"ok"})" : R"({"error":"init failed"})", "application/json");
            return;
        }

        if (action == "store") {
            std::string name = body.value("name", "");
            std::string service = body.value("service", "");
            std::string value = body.value("value", "");
            if (name.empty() || value.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"name and value required"})", "application/json");
                return;
            }
            VaultStorage::initialize(password);
            bool ok = VaultStorage::store(password, name, service, value);
            if (ok) LogBuffer::instance().info("api", "Vault operation: store");
            res.set_content(ok ? R"({"status":"ok"})" : R"({"error":"store failed"})", "application/json");
            return;
        }

        if (action == "retrieve") {
            std::string name = body.value("name", "");
            if (name.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"name required"})", "application/json");
                return;
            }
            auto val = VaultStorage::retrieve(password, name);
            if (val) {
                LogBuffer::instance().info("api", "Vault operation: retrieve");
                nlohmann::json j;
                j["status"] = "ok";
                j["value"] = *val;
                res.set_content(j.dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content(R"({"error":"not found or wrong password"})", "application/json");
            }
            return;
        }

        if (action == "remove") {
            std::string name = body.value("name", "");
            bool ok = VaultStorage::remove(password, name);
            if (ok) LogBuffer::instance().info("api", "Vault operation: remove");
            res.set_content(ok ? R"({"status":"ok"})" : R"({"error":"not found"})", "application/json");
            return;
        }

        res.status = 400;
        res.set_content(R"({"error":"unknown action"})", "application/json");
    });

    // ── API Registry ─────────────────────────────────────────────────

    svr.Get("/api/api-registry", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        auto entries = VaultStorage::listApiRegistry();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : entries) {
            arr.push_back({
                {"slug", e.slug}, {"name", e.name}, {"base_url", e.base_url},
                {"auth_type", e.auth_type}, {"vault_key_name", e.vault_key_name},
                {"endpoints", e.endpoints}, {"status", e.status},
                {"documentation_url", e.documentation_url},
                {"created_at", e.created_at}, {"updated_at", e.updated_at}
            });
        }
        nlohmann::json result;
        result["apis"] = arr;
        result["count"] = arr.size();
        res.set_content(result.dump(), "application/json");
    });

    svr.Post("/api/api-registry", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string action = body.value("action", "save");

        if (action == "remove") {
            std::string slug = body.value("slug", "");
            bool ok = VaultStorage::removeApiRegistry(slug);
            if (ok) LogBuffer::instance().info("api", "Vault operation: api_registry_remove");
            res.set_content(ok ? R"({"status":"ok"})" : R"({"error":"not found"})", "application/json");
            return;
        }

        std::string slug = body.value("slug", "");
        if (slug.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"slug required"})", "application/json");
            return;
        }

        ApiRegistryEntry entry;
        entry.slug = slug;
        entry.name = body.value("name", "");
        entry.base_url = body.value("base_url", "");
        entry.auth_type = body.value("auth_type", "bearer");
        entry.auth_config = body.value("auth_config", nlohmann::json::object());
        entry.vault_key_name = body.value("vault_key_name", "");
        entry.endpoints = body.value("endpoints", nlohmann::json::array());
        entry.documentation_url = body.value("documentation_url", "");
        entry.research_notes = body.value("research_notes", "");
        entry.status = body.value("status", "configured");

        bool ok = VaultStorage::saveApiRegistry(slug, entry);
        if (ok) LogBuffer::instance().info("api", "Vault operation: api_registry_save");
        res.set_content(ok ? R"({"status":"ok"})" : R"({"error":"save failed"})", "application/json");
    });

    svr.Delete(R"(/api/api-registry/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string slug = req.matches[1];
        bool ok = VaultStorage::removeApiRegistry(slug);
        if (ok) LogBuffer::instance().info("api", "Vault operation: api_registry_delete");
        res.set_content(ok ? R"({"ok":true})" : R"({"error":"not found"})", "application/json");
    });

    // ── Sessions ─────────────────────────────────────────────────────

    svr.Get("/api/sessions", [](const httplib::Request&, httplib::Response& res) {
        std::string sessDir = SessionManager::defaultSessionsDir();
        nlohmann::json arr = nlohmann::json::array();

        if (fs::exists(sessDir) && fs::is_directory(sessDir)) {
            try {
                for (const auto& entry :
                     fs::directory_iterator(sessDir, fs::directory_options::skip_permission_denied)) {
                    try {
                        std::error_code dec;
                        if (!fs::is_directory(entry.path(), dec) || dec) continue;

                        auto sessionFile = entry.path() / "session.json";
                        if (!fs::exists(sessionFile)) continue;

                        nlohmann::json item;
                        item["name"] = entry.path().filename().string();
                        std::error_code szEc;
                        auto sz = fs::file_size(sessionFile, szEc);
                        item["size"] = szEc ? 0 : sz;

                        std::error_code timeEc;
                        auto ftime = fs::last_write_time(sessionFile, timeEc);
                        if (!timeEc) {
                            item["modified"] = avacli::fileTimeToUnixSeconds(ftime);
                        } else {
                            item["modified"] = 0;
                        }

                        try {
                            std::ifstream f(sessionFile);
                            auto j = nlohmann::json::parse(f);
                            if (j.contains("messages") && j["messages"].is_array())
                                item["message_count"] = j["messages"].size();
                            else
                                item["message_count"] = 0;
                        } catch (...) {
                            item["message_count"] = 0;
                        }

                        arr.push_back(item);
                    } catch (const std::exception&) {
                        continue;
                    }
                }
            } catch (const std::exception&) {}
        }

        res.set_content(arr.dump(), "application/json");
    });

    svr.Get(R"(/api/sessions/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string name = req.matches[1];

        SessionManager mgr;
        if (!mgr.exists(name)) {
            res.status = 404;
            res.set_content(R"({"error":"session not found"})", "application/json");
            return;
        }

        SessionData data;
        if (!mgr.load(name, data)) {
            res.status = 500;
            res.set_content(R"({"error":"failed to load session"})", "application/json");
            return;
        }

        LogBuffer::instance().debug("chat", "Session loaded: " + name);

        nlohmann::json messages = nlohmann::json::array();
        for (const auto& m : data.messages) {
            nlohmann::json msg{{"role", m.role}, {"content", m.content}};
            if (m.tool_call_id) msg["tool_call_id"] = *m.tool_call_id;
            if (!m.tool_calls.empty()) {
                auto arr = nlohmann::json::array();
                for (const auto& tc : m.tool_calls) {
                    arr.push_back({{"id", tc.id}, {"name", tc.name}, {"arguments", tc.arguments}});
                }
                msg["tool_calls"] = arr;
            }
            messages.push_back(msg);
        }

        nlohmann::json j;
        j["name"] = name;
        j["messages"] = messages;
        j["mode"] = modeToStdString(data.mode);
        j["prompt_tokens"] = data.totalPromptTokens;
        j["completion_tokens"] = data.totalCompletionTokens;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/sessions/append", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400; res.set_content(R"({"error":"invalid JSON"})", "application/json"); return;
        }
        std::string session = body.value("session", "");
        if(session.empty()){ res.status = 400; res.set_content(R"({"error":"session required"})", "application/json"); return; }
        SessionManager mgr;
        SessionData data;
        mgr.load(session, data);
        LogBuffer::instance().debug("chat", "Session loaded: " + session);
        std::string role = body.value("role", "user");
        std::string content = body.value("content", "");
        data.messages.push_back({role, content});
        mgr.save(session, data);
        res.set_content(R"({"saved":true})", "application/json");
    });

    svr.Post("/api/sessions/rename", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string from = body.value("from", "");
        std::string to = body.value("to", "");
        if (from.empty() || to.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"from and to required"})", "application/json");
            return;
        }

        if (from.find("..") != std::string::npos || to.find("..") != std::string::npos ||
            from.find('/') != std::string::npos || to.find('/') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid session name"})", "application/json");
            return;
        }

        std::string sessDir = SessionManager::defaultSessionsDir();
        auto srcDir = fs::path(sessDir) / from;
        auto dstDir = fs::path(sessDir) / to;

        if (!fs::exists(srcDir)) {
            res.status = 404;
            res.set_content(R"({"error":"session not found"})", "application/json");
            return;
        }
        if (fs::exists(dstDir)) {
            res.status = 409;
            res.set_content(R"({"error":"a session with that name already exists"})", "application/json");
            return;
        }

        try {
            fs::rename(srcDir, dstDir);
            res.set_content(R"({"renamed":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("rename failed: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post("/api/sessions/delete-message", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string session = body.value("session", "");
        int visibleIndex = body.value("visible_index", -1);
        if (session.empty() || visibleIndex < 0) {
            res.status = 400;
            res.set_content(R"({"error":"session and visible_index required"})", "application/json");
            return;
        }

        SessionManager mgr;
        SessionData data;
        if (!mgr.load(session, data)) {
            res.status = 404;
            res.set_content(R"({"error":"session not found"})", "application/json");
            return;
        }

        LogBuffer::instance().debug("chat", "Session loaded: " + session);

        int visCount = 0;
        int targetRealIndex = -1;
        for (size_t i = 0; i < data.messages.size(); i++) {
            const auto& role = data.messages[i].role;
            if (role != "system" && role != "tool") {
                if (visCount == visibleIndex) {
                    targetRealIndex = static_cast<int>(i);
                    break;
                }
                visCount++;
            }
        }

        if (targetRealIndex < 0) {
            res.status = 404;
            res.set_content(R"({"error":"message not found"})", "application/json");
            return;
        }

        data.messages.erase(data.messages.begin() + targetRealIndex);
        mgr.save(session, data);
        nlohmann::json result;
        result["deleted"] = true;
        result["message_count"] = 0;
        for (const auto& m : data.messages) {
            if (m.role != "system" && m.role != "tool") result["message_count"] = result["message_count"].get<int>() + 1;
        }
        res.set_content(result.dump(), "application/json");
    });

    svr.Delete(R"(/api/sessions/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string name = req.matches[1];
        std::string sessDir = SessionManager::defaultSessionsDir();
        auto sessionDir = fs::path(sessDir) / name;

        if (!fs::exists(sessionDir)) {
            res.status = 404;
            res.set_content(R"({"error":"session not found"})", "application/json");
            return;
        }

        try {
            fs::remove_all(sessionDir);
            LogBuffer::instance().info("chat", "Session deleted: " + name);
            res.set_content(R"({"deleted":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("delete failed: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── Tokens / Usage ───────────────────────────────────────────────

    svr.Get("/api/tokens/balance", [ctx](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["mode"] = "local";
        j["balance"] = -1;
        j["lifetime_usage"] = -1;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/tokens/usage", [ctx](const httplib::Request&, httplib::Response& res) {
        size_t prompt = ctx.sessionPromptTokens->load();
        size_t completion = ctx.sessionCompletionTokens->load();
        nlohmann::json j;
        j["session_prompt_tokens"] = prompt;
        j["session_completion_tokens"] = completion;
        j["session_total"] = prompt + completion;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/usage/history", [](const httplib::Request& req, httplib::Response& res) {
        int days = 30, hours = 0;
        auto itH = req.params.find("hours");
        auto itD = req.params.find("days");
        if (itH != req.params.end()) {
            try { hours = std::stoi(itH->second); } catch (...) {}
        } else if (itD != req.params.end()) {
            try { days = std::stoi(itD->second); } catch (...) {}
        }
        auto history = loadUsageHistory(days, hours);
        res.set_content(history.dump(), "application/json");
    });
}

} // namespace avacli
