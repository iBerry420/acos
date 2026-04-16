#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "db/Database.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace avacli {

namespace fs = std::filesystem;

namespace {

static std::string htmlDecode(const std::string& s) {
    if (s.find('&') == std::string::npos) return s;
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '&') {
            if (s.compare(i, 4, "&lt;") == 0) { out += '<'; i += 4; }
            else if (s.compare(i, 4, "&gt;") == 0) { out += '>'; i += 4; }
            else if (s.compare(i, 5, "&amp;") == 0) { out += '&'; i += 5; }
            else if (s.compare(i, 6, "&quot;") == 0) { out += '"'; i += 6; }
            else if (s.compare(i, 6, "&apos;") == 0) { out += '\''; i += 6; }
            else if (s.compare(i, 5, "&#10;") == 0) { out += '\n'; i += 5; }
            else if (s.compare(i, 5, "&#13;") == 0) { out += '\r'; i += 5; }
            else { out += s[i++]; }
        } else {
            out += s[i++];
        }
    }
    return out;
}
static bool needsDecode(const std::string& s) {
    return s.find("&lt;") != std::string::npos || s.find("&gt;") != std::string::npos;
}

static size_t curlWriteString(void* p, size_t sz, size_t nm, std::string* s) {
    s->append(static_cast<char*>(p), sz * nm);
    return sz * nm;
}
static size_t curlWriteFile(void* p, size_t sz, size_t nm, FILE* f) {
    return fwrite(p, sz, nm, f);
}

struct CurlResponse { int status = 0; std::string body; bool ok() const { return status >= 200 && status < 300; } };

CurlResponse curlPostJson(const std::string& url, const std::string& json, const std::string& bearer) {
    CurlResponse r;
    CURL* c = curl_easy_init();
    if (!c) { r.body = "curl init failed"; return r; }
    struct curl_slist* h = nullptr;
    h = curl_slist_append(h, "Content-Type: application/json");
    h = curl_slist_append(h, ("Authorization: Bearer " + bearer).c_str());
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteString);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &r.body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
    curl_easy_perform(c);
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
    curl_slist_free_all(h);
    curl_easy_cleanup(c);
    return r;
}

bool curlDownload(const std::string& url, const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    CURL* c = curl_easy_init();
    if (!c) { fclose(f); return false; }
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteFile);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    auto rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    fclose(f);
    return rc == CURLE_OK;
}

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
std::string slugify(const std::string& name) {
    std::string s;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (c == ' ' || c == '-' || c == '_') s += '-';
    }
    while (s.find("--") != std::string::npos)
        s.replace(s.find("--"), 2, "-");
    if (!s.empty() && s.back() == '-') s.pop_back();
    if (!s.empty() && s.front() == '-') s.erase(0, 1);
    if (s.empty()) s = "app-" + std::to_string(nowMs());
    return s;
}
std::string mimeFromFilename(const std::string& f) {
    auto dot = f.rfind('.');
    if (dot == std::string::npos) return "text/plain";
    auto ext = f.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".woff") return "font/woff";
    if (ext == ".xml") return "application/xml";
    if (ext == ".txt") return "text/plain";
    if (ext == ".md") return "text/markdown";
    return "text/plain";
}
}

void registerAppRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Get("/api/apps", [ctx](const httplib::Request&, httplib::Response& res) {
        try {
            auto rows = Database::instance().query(
                "SELECT id, name, description, slug, status, icon_url, created_at, updated_at, created_by "
                "FROM apps ORDER BY updated_at DESC");
            nlohmann::json out;
            out["apps"] = rows;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post("/api/apps", [ctx](const httplib::Request& req, httplib::Response& res) {
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
        std::string slug = body.value("slug", slugify(name));
        std::string desc = body.value("description", "");
        std::string user = getRequestUser(req, ctx.masterKeyMgr);

        try {
            auto& db = Database::instance();
            auto existing = db.queryOne("SELECT id FROM apps WHERE slug = ?1", {slug});
            if (!existing.is_null()) {
                slug = slug + "-" + std::to_string(now % 10000);
            }

            db.execute(
                "INSERT INTO apps (id, name, description, slug, status, created_at, updated_at, created_by) "
                "VALUES (?1, ?2, ?3, ?4, 'active', ?5, ?5, ?6)",
                {id, name, desc, slug, std::to_string(now), user});

            std::string defaultHtml = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">\n"
                "<title>" + name + "</title>\n"
                "<style>\n"
                "*{margin:0;padding:0;box-sizing:border-box}\n"
                "body{font-family:system-ui,-apple-system,sans-serif;background:#0a0a14;color:#e2e2e8;"
                "min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:2rem}\n"
                ".container{max-width:720px;width:100%;padding:1.5rem}\n"
                "h1{font-size:1.75rem;margin-bottom:1rem;color:#c4b5fd}\n"
                "p{color:#a0a0b0;line-height:1.6;margin-bottom:1rem}\n"
                "a{color:#818cf8}\n"
                ".card{background:#12121e;border:1px solid #2a2a3a;border-radius:12px;padding:1.5rem;margin:1rem 0}\n"
                "button{background:#7c3aed;color:#fff;border:none;padding:.625rem 1.25rem;border-radius:8px;"
                "cursor:pointer;font-size:.875rem;font-family:inherit}\n"
                "button:hover{background:#6d28d9}\n"
                "input,textarea{background:#12121e;border:1px solid #2a2a3a;border-radius:8px;padding:.5rem .75rem;"
                "color:#e2e2e8;font-size:.875rem;width:100%;font-family:inherit}\n"
                "input:focus,textarea:focus{outline:none;border-color:#7c3aed}\n"
                "</style>\n"
                "</head>\n<body>\n<div class=\"container\">\n"
                "<h1>" + name + "</h1>\n"
                "<div class=\"card\">\n"
                "<p>Your app is live. Edit the files to build something amazing.</p>\n"
                "</div>\n</div>\n"
                "<script>\n// Your JavaScript here\n</script>\n"
                "</body>\n</html>";

            std::string fileId = generateId();
            db.execute(
                "INSERT INTO app_files (id, app_id, filename, content, mime_type, updated_at) "
                "VALUES (?1, ?2, 'index.html', ?3, 'text/html', ?4)",
                {fileId, id, defaultHtml, std::to_string(now)});

            LogBuffer::instance().info("apps", "App created: " + name, {{"id", id}, {"slug", slug}});
            nlohmann::json out;
            out["id"] = id;
            out["slug"] = slug;
            out["created"] = true;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get(R"(/api/apps/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto& db = Database::instance();
            auto app = db.queryOne("SELECT * FROM apps WHERE id = ?1", {id});
            if (app.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"app not found"})", "application/json");
                return;
            }
            auto files = db.query(
                "SELECT id, filename, mime_type, length(content) as size, updated_at "
                "FROM app_files WHERE app_id = ?1 ORDER BY filename", {id});
            app["files"] = files;
            res.set_content(app.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Put(R"(/api/apps/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
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
                db.execute("UPDATE apps SET name = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["name"].get<std::string>(), now, id});
            if (body.contains("description"))
                db.execute("UPDATE apps SET description = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["description"].get<std::string>(), now, id});
            if (body.contains("status"))
                db.execute("UPDATE apps SET status = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["status"].get<std::string>(), now, id});
            if (body.contains("icon_url"))
                db.execute("UPDATE apps SET icon_url = ?1, updated_at = ?2 WHERE id = ?3",
                    {body["icon_url"].get<std::string>(), now, id});
            res.set_content(R"({"updated":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Delete(R"(/api/apps/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            Database::instance().execute("DELETE FROM app_files WHERE app_id = ?1", {id});
            Database::instance().execute("DELETE FROM apps WHERE id = ?1", {id});
            LogBuffer::instance().info("apps", "App deleted", {{"id", id}});
            res.set_content(R"({"deleted":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get(R"(/api/apps/([^/]+)/files)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string appId = req.matches[1];
        try {
            auto files = Database::instance().query(
                "SELECT id, filename, mime_type, length(content) as size, updated_at "
                "FROM app_files WHERE app_id = ?1 ORDER BY filename", {appId});
            nlohmann::json out;
            out["files"] = files;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get(R"(/api/apps/([^/]+)/files/(.+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string appId = req.matches[1];
        std::string filename = req.matches[2];
        try {
            auto row = Database::instance().queryOne(
                "SELECT content, mime_type FROM app_files WHERE app_id = ?1 AND filename = ?2",
                {appId, filename});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"file not found"})", "application/json");
                return;
            }
            nlohmann::json out;
            out["content"] = row["content"];
            out["mime_type"] = row["mime_type"];
            out["filename"] = filename;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Put(R"(/api/apps/([^/]+)/files/(.+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string appId = req.matches[1];
        std::string filename = req.matches[2];
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string content = body.value("content", "");
        if (needsDecode(content)) content = htmlDecode(content);
        std::string mime = body.value("mime_type", mimeFromFilename(filename));
        auto now = std::to_string(nowMs());

        try {
            auto& db = Database::instance();
            auto existing = db.queryOne(
                "SELECT id FROM app_files WHERE app_id = ?1 AND filename = ?2",
                {appId, filename});

            if (existing.is_null()) {
                std::string fileId = generateId();
                db.execute(
                    "INSERT INTO app_files (id, app_id, filename, content, mime_type, updated_at) "
                    "VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
                    {fileId, appId, filename, content, mime, now});
            } else {
                db.execute(
                    "UPDATE app_files SET content = ?1, mime_type = ?2, updated_at = ?3 "
                    "WHERE app_id = ?4 AND filename = ?5",
                    {content, mime, now, appId, filename});
            }

            db.execute("UPDATE apps SET updated_at = ?1 WHERE id = ?2", {now, appId});
            LogBuffer::instance().debug("apps", "File updated: " + filename, {{"app_id", appId}});
            res.set_content(R"({"saved":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Delete(R"(/api/apps/([^/]+)/files/(.+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string appId = req.matches[1];
        std::string filename = req.matches[2];
        try {
            Database::instance().execute(
                "DELETE FROM app_files WHERE app_id = ?1 AND filename = ?2",
                {appId, filename});
            res.set_content(R"({"deleted":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    // Export app files to workspace filesystem for editing in the Files page
    svr.Post(R"(/api/apps/([^/]+)/export)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto& db = Database::instance();
            auto app = db.queryOne("SELECT slug FROM apps WHERE id = ?1", {id});
            if (app.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"app not found"})", "application/json");
                return;
            }
            std::string slug = app["slug"].get<std::string>();
            fs::path appDir = fs::path(ctx.config->workspace) / "apps" / slug;
            fs::create_directories(appDir);

            auto files = db.query(
                "SELECT filename, content FROM app_files WHERE app_id = ?1", {id});
            for (const auto& f : files) {
                auto filePath = appDir / f["filename"].get<std::string>();
                fs::create_directories(filePath.parent_path());
                std::ofstream ofs(filePath, std::ios::binary);
                ofs << f["content"].get<std::string>();
            }

            nlohmann::json out;
            out["exported"] = true;
            out["path"] = "apps/" + slug;
            out["files"] = static_cast<int>(files.size());
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    // Generate an app icon using xAI image generation
    svr.Post(R"(/api/apps/([^/]+)/generate-icon)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string prompt = body.value("prompt", "");
        if (prompt.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"prompt required"})", "application/json");
            return;
        }

        std::string apiKey = ctx.config->apiKey;
        if (apiKey.empty()) {
            const char* envKey = std::getenv("XAI_API_KEY");
            if (envKey) apiKey = envKey;
        }
        if (apiKey.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"no API key configured"})", "application/json");
            return;
        }

        try {
            std::string defaultMedia = ctx.config->mediaModel.empty() ? "grok-imagine-image" : ctx.config->mediaModel;
            std::string model = body.value("model", defaultMedia);
            if (model.empty()) model = defaultMedia;
            // xAI Imagine API uses resolution ("1k"/"2k") + aspect_ratio — not OpenAI-style "1024x1024" size.
            std::string resolution = body.value("resolution", "");
            std::string aspectRatio = body.value("aspect_ratio", "");
            std::string sizeLegacy = body.value("size", "");
            if (resolution != "1k" && resolution != "2k") resolution = "";
            if (!sizeLegacy.empty() && resolution.empty()) {
                resolution = "1k";
                if (aspectRatio.empty()) aspectRatio = "1:1";
            }
            if (resolution.empty()) resolution = "1k";
            if (aspectRatio.empty()) aspectRatio = "1:1";

            nlohmann::json reqBody;
            reqBody["model"] = model;
            reqBody["prompt"] = prompt;
            reqBody["n"] = 1;
            reqBody["resolution"] = resolution;
            reqBody["aspect_ratio"] = aspectRatio;
            reqBody["response_format"] = "url";

            auto resp = curlPostJson("https://api.x.ai/v1/images/generations", reqBody.dump(), apiKey);
            if (!resp.ok()) {
                std::string detail;
                try { auto j = nlohmann::json::parse(resp.body); detail = j.value("error", resp.body); } catch (...) { detail = resp.body; }
                res.status = 502;
                res.set_content(nlohmann::json({{"error", "Image API error (" + std::to_string(resp.status) + "): " + detail}}).dump(), "application/json");
                return;
            }

            auto result = nlohmann::json::parse(resp.body);
            std::string imgUrl;
            if (result.contains("data") && result["data"].is_array() && !result["data"].empty()) {
                imgUrl = result["data"][0].value("url", "");
            }
            if (imgUrl.empty()) {
                res.status = 502;
                res.set_content(R"({"error":"no image returned"})", "application/json");
                return;
            }

            fs::path mediaDir = fs::path(ctx.config->workspace) / "uploads" / "avacli-media";
            fs::create_directories(mediaDir);
            std::string fname = "icon_" + id.substr(0, 13) + ".png";
            std::string localPath = (mediaDir / fname).string();

            if (!curlDownload(imgUrl, localPath)) {
                res.status = 502;
                res.set_content(R"({"error":"failed to download image"})", "application/json");
                return;
            }

            std::string iconUrl = "/uploads/avacli-media/" + fname;
            auto now = std::to_string(nowMs());
            Database::instance().execute(
                "UPDATE apps SET icon_url = ?1, updated_at = ?2 WHERE id = ?3",
                {iconUrl, now, id});

            nlohmann::json out;
            out["icon_url"] = iconUrl;
            out["generated"] = true;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", e.what()}}).dump(), "application/json");
        }
    });
}

} // namespace avacli
