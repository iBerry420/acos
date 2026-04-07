#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "auth/MasterKeyManager.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace avacli {

namespace fs = std::filesystem;

void registerAuthRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Get("/api/auth/me", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json j;
        j["authenticated"] = true;
        j["mode"] = "local";
        std::string user = getRequestUser(req, ctx.masterKeyMgr);
        if (!user.empty()) {
            j["username"] = user;
            j["is_admin"] = MasterKeyManager::isUserAdmin(user);
            j["has_password"] = MasterKeyManager::accountHasPassword(user);
        } else {
            j["username"] = "admin";
            j["is_admin"] = true;
            j["has_password"] = false;
        }
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/auth/status", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json j;
        bool configured = MasterKeyManager::isConfigured();
        bool requiresLogin = configured && MasterKeyManager::hasAnyPasswordSet();
        j["master_key_configured"] = configured;
        j["requires_login"] = requiresLogin;
        bool authed = false;
        if (requiresLogin) {
            std::string authHeader = req.get_header_value("Authorization");
            if (authHeader.size() > 7 && authHeader.substr(0, 7) == "Bearer ") {
                authed = ctx.masterKeyMgr->validateSession(authHeader.substr(7));
            }
        } else {
            authed = true;
        }
        j["authenticated"] = authed;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/auth/login", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!MasterKeyManager::isConfigured() || !MasterKeyManager::hasAnyPasswordSet()) {
            nlohmann::json j;
            j["token"] = "";
            j["username"] = "admin";
            j["is_admin"] = true;
            j["message"] = "No credentials configured — access is open";
            res.set_content(j.dump(), "application/json");
            return;
        }

        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string username = body.value("username", "");
        std::string password = body.value("password", "");
        if (password.empty() && body.contains("master_key"))
            password = body.value("master_key", "");
        if (password.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"password required"})", "application/json");
            return;
        }

        auto authResult = MasterKeyManager::authenticateUser(username, password);
        if (!authResult) {
            res.status = 401;
            res.set_content(R"({"error":"invalid credentials"})", "application/json");
            return;
        }

        std::string token = ctx.masterKeyMgr->createSession(authResult->username);
        nlohmann::json j;
        j["token"] = token;
        j["username"] = authResult->username;
        j["is_admin"] = authResult->is_admin;
        j["message"] = "authenticated";
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/auth/logout", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string authHeader = req.get_header_value("Authorization");
        if (authHeader.size() > 7 && authHeader.substr(0, 7) == "Bearer ") {
            ctx.masterKeyMgr->invalidateSession(authHeader.substr(7));
        }
        res.set_content(R"({"logged_out":true})", "application/json");
    });

    svr.Post("/api/auth/generate-key", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        try {
            std::string key = MasterKeyManager::generateMasterKey();
            nlohmann::json j;
            j["key"] = key;
            j["message"] = "Master key generated and stored";
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("Key generation failed: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── Account management (admin only for create/update/delete) ──

    svr.Get("/api/auth/accounts", [](const httplib::Request&, httplib::Response& res) {
        auto accounts = MasterKeyManager::listAccounts();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& a : accounts) {
            arr.push_back({
                {"username", a.username},
                {"is_admin", a.is_admin},
                {"has_password", a.has_password},
                {"created_at", a.created_at}
            });
        }
        nlohmann::json j;
        j["accounts"] = arr;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/auth/accounts", [ctx](const httplib::Request& req, httplib::Response& res) {
        bool firstAccount = !MasterKeyManager::isConfigured();

        if (!firstAccount && !isAdminRequest(req, ctx.masterKeyMgr)) {
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
        std::string username = body.value("username", "");
        std::string password = body.value("password", "");
        bool isAdmin = firstAccount ? true : body.value("is_admin", false);
        if (username.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"username required"})", "application/json");
            return;
        }
        if (!firstAccount && password.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"password required"})", "application/json");
            return;
        }
        if (!password.empty() && password.size() < 8) {
            res.status = 400;
            res.set_content(R"({"error":"password must be at least 8 characters"})", "application/json");
            return;
        }
        if (!MasterKeyManager::createAccount(username, password, isAdmin)) {
            res.status = 409;
            res.set_content(R"({"error":"account already exists or creation failed"})", "application/json");
            return;
        }

        if (firstAccount) {
            std::string token = ctx.masterKeyMgr->createSession(username);
            nlohmann::json j;
            j["created"] = true;
            j["token"] = token;
            j["username"] = username;
            j["is_admin"] = true;
            j["message"] = "First admin account created, auto-logged in";
            res.set_content(j.dump(), "application/json");
            return;
        }

        res.set_content(R"({"created":true})", "application/json");
    });

    svr.Post("/api/auth/accounts/update", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string username = body.value("username", "");
        if (username.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"username required"})", "application/json");
            return;
        }

        std::string reqUser = getRequestUser(req, ctx.masterKeyMgr);
        bool isSelf = (!reqUser.empty() && reqUser == username);
        bool isAdmin = isAdminRequest(req, ctx.masterKeyMgr);

        if (!isSelf && !isAdmin) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required, or change your own password"})", "application/json");
            return;
        }

        bool updated = false;

        if (body.contains("password") && body["password"].is_string()) {
            std::string pw = body["password"].get<std::string>();
            if (pw.size() < 8) {
                res.status = 400;
                res.set_content(R"({"error":"password must be at least 8 characters"})", "application/json");
                return;
            }
            if (!MasterKeyManager::updateAccountPassword(username, pw)) {
                res.status = 404;
                res.set_content(R"({"error":"account not found"})", "application/json");
                return;
            }
            updated = true;
        }

        if (body.contains("is_admin") && body["is_admin"].is_boolean() && isAdmin) {
            MasterKeyManager::updateAccountRole(username, body["is_admin"].get<bool>());
            updated = true;
        }

        if (body.contains("new_username") && body["new_username"].is_string()) {
            std::string newName = body["new_username"].get<std::string>();
            if (newName.empty() || newName.size() < 2) {
                res.status = 400;
                res.set_content(R"({"error":"new username must be at least 2 characters"})", "application/json");
                return;
            }
            if (!isSelf && !isAdmin) {
                res.status = 403;
                res.set_content(R"({"error":"admin access required to rename other accounts"})", "application/json");
                return;
            }
            if (!MasterKeyManager::renameAccount(username, newName)) {
                res.status = 409;
                res.set_content(R"({"error":"rename failed - target username may already exist"})", "application/json");
                return;
            }
            ctx.masterKeyMgr->updateSessionUsername(username, newName);
            updated = true;
        }

        if (!updated) {
            res.status = 400;
            res.set_content(R"({"error":"nothing to update"})", "application/json");
            return;
        }
        res.set_content(R"({"updated":true})", "application/json");
    });

    svr.Delete(R"(/api/auth/accounts/(.+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        std::string username = req.matches[1];
        MasterKeyManager::deleteAccount(username);
        res.set_content(R"({"removed":true})", "application/json");
    });

    // ── Change own password (any authenticated user) ──

    svr.Post("/api/auth/change-password", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string currentPw = body.value("current_password", "");
        std::string newPw = body.value("new_password", "");
        if (currentPw.empty() || newPw.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"current_password and new_password required"})", "application/json");
            return;
        }
        if (newPw.size() < 8) {
            res.status = 400;
            res.set_content(R"({"error":"new password must be at least 8 characters"})", "application/json");
            return;
        }

        std::string username = getRequestUser(req, ctx.masterKeyMgr);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"not authenticated"})", "application/json");
            return;
        }

        auto auth = MasterKeyManager::authenticateUser(username, currentPw);
        if (!auth) {
            res.status = 401;
            res.set_content(R"({"error":"current password is incorrect"})", "application/json");
            return;
        }

        if (!MasterKeyManager::updateAccountPassword(username, newPw)) {
            res.status = 500;
            res.set_content(R"({"error":"failed to update password"})", "application/json");
            return;
        }
        res.set_content(R"({"changed":true})", "application/json");
    });
}

} // namespace avacli
