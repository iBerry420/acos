#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include "utils/TimeUtils.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <unordered_set>

namespace avacli {

namespace fs = std::filesystem;

void registerFileRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Get("/api/files/tree", [ctx](const httplib::Request&, httplib::Response& res) {
        std::function<nlohmann::json(const fs::path&, int)> buildTree;
        buildTree = [&](const fs::path& dir, int depth) -> nlohmann::json {
            nlohmann::json arr = nlohmann::json::array();
            if (depth > 5) return arr;

            std::vector<fs::directory_entry> entries;
            std::error_code iterEc;
            for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, iterEc), end;
                 !iterEc && it != end; it.increment(iterEc)) {
                try { entries.push_back(*it); } catch (const std::exception&) {}
            }

            std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                std::error_code ea, eb;
                bool da = fs::is_directory(a.path(), ea);
                bool db = fs::is_directory(b.path(), eb);
                if (ea || eb) return a.path().filename() < b.path().filename();
                if (da != db) return da;
                return a.path().filename() < b.path().filename();
            });

            static const std::unordered_set<std::string> SKIP = {
                ".git", "node_modules", "vendor", "build", "__pycache__", ".cache"
            };

            const fs::path wsRoot(ctx.config->workspace);

            for (const auto& e : entries) {
                try {
                    std::string name = e.path().filename().string();
                    if (!name.empty() && name[0] == '.' && name != ".avacli") continue;
                    if (SKIP.count(name)) continue;

                    std::error_code stEc;
                    fs::file_status st = fs::status(e.path(), stEc);
                    if (stEc) continue;

                    nlohmann::json node;
                    node["name"] = name;

                    std::error_code relEc;
                    fs::path rel = fs::relative(e.path(), wsRoot, relEc);
                    node["path"] = relEc ? name : rel.generic_string();

                    if (fs::is_directory(st)) {
                        node["type"] = "dir";
                        try {
                            node["children"] = buildTree(e.path(), depth + 1);
                        } catch (const std::exception&) {
                            node["children"] = nlohmann::json::array();
                            node["unreadable"] = true;
                        }
                    } else if (fs::is_regular_file(st) || fs::is_symlink(st)) {
                        node["type"] = "file";
                        std::error_code fsec;
                        auto sz = fs::file_size(e.path(), fsec);
                        if (!fsec)
                            node["size"] = sz;
                        else {
                            node["size"] = 0;
                            node["unreadable"] = true;
                        }
                    } else {
                        continue;
                    }
                    arr.push_back(node);
                } catch (const std::exception&) {
                    continue;
                }
            }
            return arr;
        };

        try {
            auto wsPath = fs::canonical(ctx.config->workspace);
            nlohmann::json j;
            j["tree"] = buildTree(wsPath, 0);
            j["workspace"] = wsPath.string();
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("failed to read workspace: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Get("/api/files/read", [ctx](const httplib::Request& req, httplib::Response& res) {
        auto it = req.params.find("path");
        if (it == req.params.end() || it->second.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"path parameter required"})", "application/json");
            return;
        }

        try {
            auto workspaceResolved = fs::canonical(ctx.config->workspace);
            auto requested = fs::path(ctx.config->workspace) / it->second;
            if (!fs::exists(requested)) {
                res.status = 404;
                res.set_content(R"({"error":"file not found"})", "application/json");
                return;
            }
            auto resolved = fs::canonical(requested);

            if (resolved.string().find(workspaceResolved.string()) != 0) {
                res.status = 403;
                res.set_content(R"({"error":"path outside workspace"})", "application/json");
                return;
            }

            auto fileSize = fs::file_size(resolved);
            if (fileSize > 1024 * 1024) {
                res.status = 413;
                res.set_content(R"({"error":"file exceeds 1MB limit"})", "application/json");
                return;
            }

            std::ifstream ifs(resolved, std::ios::binary);
            if (!ifs) {
                res.status = 500;
                res.set_content(R"({"error":"failed to open file"})", "application/json");
                return;
            }
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());

            size_t lines = 0;
            for (char c : content) { if (c == '\n') ++lines; }
            if (!content.empty() && content.back() != '\n') ++lines;

            nlohmann::json j;
            j["path"] = fs::relative(resolved, workspaceResolved).string();
            j["content"] = content;
            j["size"] = fileSize;
            j["lines"] = lines;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("read failed: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post("/api/files/write", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string filePath = body.value("path", "");
        std::string content = body.value("content", "");
        if (filePath.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"path required"})", "application/json");
            return;
        }

        try {
            auto workspaceResolved = fs::canonical(ctx.config->workspace);
            auto target = workspaceResolved / filePath;
            auto parentDir = target.parent_path();
            if (!fs::exists(parentDir)) fs::create_directories(parentDir);

            auto resolvedParent = fs::canonical(parentDir);
            if (resolvedParent.string().find(workspaceResolved.string()) != 0) {
                res.status = 403;
                res.set_content(R"({"error":"path outside workspace"})", "application/json");
                return;
            }

            std::ofstream ofs(target, std::ios::binary);
            if (!ofs) {
                res.status = 500;
                res.set_content(R"({"error":"failed to open file for writing"})", "application/json");
                return;
            }
            ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
            ofs.close();

            nlohmann::json j;
            j["success"] = true;
            j["path"] = filePath;
            j["size"] = content.size();
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("write failed: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post("/api/upload", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!req.has_file("file")) {
            res.status = 400;
            res.set_content(R"({"error":"no file uploaded"})", "application/json");
            return;
        }
        const auto& file = req.get_file_value("file");
        if (file.content.size() > 50 * 1024 * 1024) {
            res.status = 413;
            res.set_content(R"json({"error":"file too large (max 50MB)"})json", "application/json");
            return;
        }

        std::string mediaDir = (fs::path(ctx.config->workspace) / "uploads" / "avacli-media").string();
        fs::create_directories(mediaDir);

        std::string origName = file.filename;
        std::string ext = ".bin";
        auto dotPos = origName.rfind('.');
        if (dotPos != std::string::npos) ext = origName.substr(dotPos);

        bool isVideo = (ext == ".mp4" || ext == ".webm" || ext == ".mov");
        bool isImage = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp");
        std::string prefix;
        std::string mediaType;
        if (isVideo) { prefix = "upload_vid_"; mediaType = "video"; }
        else if (isImage) { prefix = "upload_img_"; mediaType = "image"; }
        else { prefix = "upload_doc_"; mediaType = "document"; }
        static const char* hx = "0123456789abcdef";
        std::random_device rd; std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 15);
        std::string rnd; for (int ri = 0; ri < 8; ++ri) rnd += hx[dist(gen)];
        std::string fname = prefix + rnd + ext;
        std::string localPath = (fs::path(mediaDir) / fname).string();

        std::ofstream ofs(localPath, std::ios::binary);
        if (!ofs) {
            res.status = 500;
            res.set_content(R"({"error":"failed to save file"})", "application/json");
            return;
        }
        ofs.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
        ofs.close();

        std::string webUrl = "/uploads/avacli-media/" + fname;
        nlohmann::json j;
        j["url"] = webUrl;
        j["type"] = mediaType;
        j["filename"] = origName;
        j["size"] = file.content.size();
        LogBuffer::instance().info("assets", "File uploaded: " + origName);
        res.set_content(j.dump(), "application/json");
    });

    svr.Get(R"(/uploads/(.+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string subPath = req.matches[1];
        auto filePath = fs::path(ctx.config->workspace) / "uploads" / subPath;

        std::error_code ec;
        auto canonical = fs::canonical(filePath, ec);
        if (ec || !fs::exists(canonical)) {
            res.status = 404;
            res.set_content("Not found", "text/plain");
            return;
        }

        auto wsRoot = fs::canonical(fs::path(ctx.config->workspace) / "uploads");
        if (canonical.string().find(wsRoot.string()) != 0) {
            res.status = 403;
            res.set_content("Forbidden", "text/plain");
            return;
        }

        std::ifstream ifs(canonical, std::ios::binary);
        if (!ifs) {
            res.status = 500;
            return;
        }
        std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        std::string extStr = canonical.extension().string();
        std::string ct = "application/octet-stream";
        if (extStr == ".png") ct = "image/png";
        else if (extStr == ".jpg" || extStr == ".jpeg") ct = "image/jpeg";
        else if (extStr == ".gif") ct = "image/gif";
        else if (extStr == ".webp") ct = "image/webp";
        else if (extStr == ".mp4") ct = "video/mp4";
        else if (extStr == ".webm") ct = "video/webm";
        else if (extStr == ".mov") ct = "video/quicktime";

        res.set_header("Cache-Control", "public, max-age=3600");
        res.set_content(data, ct);
    });

    svr.Get("/api/assets", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        fs::path mediaBase = fs::path(ctx.config->workspace) / "uploads" / "avacli-media";
        std::string thumbDir = (fs::path(ctx.config->workspace) / "uploads" / "thumbs").string();
        std::string metaDir = (fs::path(ctx.config->workspace) / "uploads" / "asset-meta").string();
        fs::create_directories(thumbDir);

        std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
        fs::path scanDir = folder.empty() ? mediaBase : (mediaBase / folder);
        if (folder.find("..") != std::string::npos || !fs::exists(scanDir)) {
            res.set_content("[]", "application/json");
            return;
        }

        std::string filterType = req.has_param("type") ? req.get_param_value("type") : "";
        std::string searchQ = req.has_param("q") ? req.get_param_value("q") : "";
        std::string searchLower = searchQ;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        static const auto classifyExt = [](const std::string& ext) -> std::string {
            if (ext == ".mp4" || ext == ".webm" || ext == ".mov") return "video";
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp") return "image";
            if (ext == ".pdf" || ext == ".doc" || ext == ".docx") return "document";
            if (ext == ".txt" || ext == ".md" || ext == ".html" || ext == ".xml" || ext == ".csv") return "document";
            if (ext == ".json" || ext == ".yaml" || ext == ".yml" || ext == ".toml" || ext == ".ini" || ext == ".env") return "document";
            if (ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".tsx" || ext == ".jsx") return "code";
            if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" || ext == ".rs") return "code";
            if (ext == ".go" || ext == ".java" || ext == ".rb" || ext == ".sh" || ext == ".css") return "code";
            if (ext == ".scss" || ext == ".sql" || ext == ".lua" || ext == ".zig" || ext == ".swift") return "code";
            if (ext == ".kt" || ext == ".cs") return "code";
            return "";
        };

        nlohmann::json assets = nlohmann::json::array();
        nlohmann::json folders = nlohmann::json::array();

        for (const auto& entry : fs::directory_iterator(scanDir, fs::directory_options::skip_permission_denied)) {
            try {
                std::string fname = entry.path().filename().string();
                if (entry.is_directory()) {
                    std::string relPath = folder.empty() ? fname : (folder + "/" + fname);
                    folders.push_back({{"id", fname}, {"filename", fname}, {"type", "folder"}, {"path", relPath}});
                    continue;
                }
                if (!entry.is_regular_file()) continue;

                std::string extStr = entry.path().extension().string();
                std::transform(extStr.begin(), extStr.end(), extStr.begin(), ::tolower);
                std::string type = classifyExt(extStr);
                if (type.empty()) continue;
                if (!filterType.empty() && filterType != type) continue;

                std::string relName = folder.empty() ? fname : (folder + "/" + fname);

                // Load metadata for search
                nlohmann::json meta;
                std::string metaPath = (fs::path(metaDir) / (relName + ".json")).string();
                // Also try flat name
                if (!fs::exists(metaPath)) metaPath = (fs::path(metaDir) / (fname + ".json")).string();
                if (fs::exists(metaPath)) {
                    try { std::ifstream f(metaPath); meta = nlohmann::json::parse(f); } catch (...) {}
                }

                if (!searchLower.empty()) {
                    std::string fnameLower = fname;
                    std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
                    bool match = fnameLower.find(searchLower) != std::string::npos;
                    if (!match && meta.contains("tags") && meta["tags"].is_array()) {
                        for (const auto& tag : meta["tags"]) {
                            std::string t = tag.get<std::string>();
                            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                            if (t.find(searchLower) != std::string::npos) { match = true; break; }
                        }
                    }
                    if (!match && meta.contains("description") && meta["description"].is_string()) {
                        std::string d = meta["description"].get<std::string>();
                        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
                        if (d.find(searchLower) != std::string::npos) match = true;
                    }
                    if (!match) continue;
                }

                std::error_code szEc;
                auto sz = fs::file_size(entry.path(), szEc);
                auto mtime = fs::last_write_time(entry.path());
                long long ts = avacli::fileTimeToUnixSeconds(mtime);

                std::string url = "/uploads/avacli-media/" + relName;
                std::string thumbUrl = url;

                if (type == "image") {
                    std::string thumbFname = "th_" + fname;
                    std::string thumbPath = (fs::path(thumbDir) / thumbFname).string();
                    thumbUrl = "/uploads/thumbs/" + thumbFname;
                    if (!fs::exists(thumbPath)) {
                        try { fs::copy_file(entry.path(), thumbPath, fs::copy_options::skip_existing); } catch (...) {}
                    }
                }

                std::string source = "unknown";
                if (fname.find("img_") == 0) source = "generated";
                else if (fname.find("edit_") == 0 || fname.find("edited_") == 0) source = "edited";
                else if (fname.find("vid_") == 0 || fname.find("ext_") == 0) source = "generated";
                else if (fname.find("upload_") == 0) source = "uploaded";

                nlohmann::json asset;
                asset["id"] = relName;
                asset["filename"] = fname;
                asset["url"] = url;
                asset["thumb_url"] = thumbUrl;
                asset["type"] = type;
                asset["source"] = source;
                asset["size"] = sz;
                asset["created_at"] = ts;
                asset["node_id"] = "";
                if (meta.contains("tags")) asset["tags"] = meta["tags"];
                if (meta.contains("description")) asset["description"] = meta["description"];

                assets.push_back(asset);
            } catch (...) { continue; }
        }

        std::sort(assets.begin(), assets.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
            return a.value("created_at", 0LL) > b.value("created_at", 0LL);
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& f : folders) result.push_back(f);
        for (auto& a : assets) result.push_back(a);

        res.set_content(result.dump(), "application/json");
    });

    // Delete asset
    svr.Delete(R"(/api/assets/(.+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string id = req.matches[1];
        if (id.find("..") != std::string::npos || id.find('/') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid id"})", "application/json");
            return;
        }
        std::string mediaDir = (fs::path(ctx.config->workspace) / "uploads" / "avacli-media").string();
        std::string thumbDir = (fs::path(ctx.config->workspace) / "uploads" / "thumbs").string();
        std::string filePath = (fs::path(mediaDir) / id).string();
        if (!fs::exists(filePath)) {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
            return;
        }
        std::error_code ec;
        fs::remove(filePath, ec);
        fs::remove((fs::path(thumbDir) / ("th_" + id)), ec);
        // Remove metadata
        std::string metaDir = (fs::path(ctx.config->workspace) / "uploads" / "asset-meta").string();
        fs::remove((fs::path(metaDir) / (id + ".json")), ec);
        LogBuffer::instance().info("assets", "Asset deleted", {{"id", id}});
        res.set_content(R"({"ok":true})", "application/json");
    });

    // Rename asset
    svr.Post(R"(/api/assets/(.+)/rename)", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string id = req.matches[1];
        if (id.find("..") != std::string::npos || id.find('/') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid id"})", "application/json");
            return;
        }
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
            return;
        }
        std::string newName = body.value("name", "");
        if (newName.empty() || newName.find("..") != std::string::npos || newName.find('/') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid name"})", "application/json");
            return;
        }
        std::string mediaDir = (fs::path(ctx.config->workspace) / "uploads" / "avacli-media").string();
        std::string oldPath = (fs::path(mediaDir) / id).string();
        std::string newPath = (fs::path(mediaDir) / newName).string();
        if (!fs::exists(oldPath)) {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
            return;
        }
        if (fs::exists(newPath)) {
            res.status = 409;
            res.set_content(R"({"error":"name already exists"})", "application/json");
            return;
        }
        std::error_code ec;
        fs::rename(oldPath, newPath, ec);
        if (ec) {
            res.status = 500;
            res.set_content(R"({"error":"rename failed"})", "application/json");
            return;
        }
        // Rename thumbnail and metadata too
        std::string thumbDir = (fs::path(ctx.config->workspace) / "uploads" / "thumbs").string();
        fs::rename((fs::path(thumbDir) / ("th_" + id)), (fs::path(thumbDir) / ("th_" + newName)), ec);
        std::string metaDir = (fs::path(ctx.config->workspace) / "uploads" / "asset-meta").string();
        fs::rename((fs::path(metaDir) / (id + ".json")), (fs::path(metaDir) / (newName + ".json")), ec);
        nlohmann::json j;
        j["ok"] = true;
        j["id"] = newName;
        j["url"] = "/uploads/avacli-media/" + newName;
        res.set_content(j.dump(), "application/json");
    });

    // Create folder in asset media dir
    svr.Post("/api/assets/folder", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
            return;
        }
        std::string name = body.value("name", "");
        std::string folder = body.value("folder", "");
        if (name.empty() || name.find("..") != std::string::npos || name.find('/') != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid folder name"})", "application/json");
            return;
        }
        fs::path base = fs::path(ctx.config->workspace) / "uploads" / "avacli-media";
        if (!folder.empty()) {
            if (folder.find("..") != std::string::npos) {
                res.status = 400;
                res.set_content(R"({"error":"invalid path"})", "application/json");
                return;
            }
            base = base / folder;
        }
        fs::path target = base / name;
        std::error_code ec;
        fs::create_directories(target, ec);
        if (ec) {
            res.status = 500;
            res.set_content(R"({"error":"failed to create folder"})", "application/json");
            return;
        }
        LogBuffer::instance().info("assets", "Folder created: " + name);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // Move asset
    svr.Post("/api/assets/move", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
            return;
        }
        std::string src = body.value("src", "");
        std::string dst = body.value("dst", "");
        if (src.empty() || dst.empty() || src.find("..") != std::string::npos || dst.find("..") != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid paths"})", "application/json");
            return;
        }
        fs::path mediaDir = fs::path(ctx.config->workspace) / "uploads" / "avacli-media";
        fs::path srcPath = mediaDir / src;
        fs::path dstPath = mediaDir / dst;
        if (!fs::exists(srcPath)) {
            res.status = 404;
            res.set_content(R"({"error":"source not found"})", "application/json");
            return;
        }
        if (fs::is_directory(dstPath))
            dstPath = dstPath / srcPath.filename();
        std::error_code ec;
        fs::create_directories(dstPath.parent_path(), ec);
        fs::rename(srcPath, dstPath, ec);
        if (ec) {
            res.status = 500;
            res.set_content(R"({"error":"move failed"})", "application/json");
            return;
        }
        nlohmann::json j;
        j["ok"] = true;
        j["new_path"] = fs::relative(dstPath, mediaDir).generic_string();
        res.set_content(j.dump(), "application/json");
    });

    // Update asset tags
    svr.Post(R"(/api/assets/(.+)/tags)", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string id = req.matches[1];
        if (id.find("..") != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid id"})", "application/json");
            return;
        }
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
            return;
        }
        std::string metaDir = (fs::path(ctx.config->workspace) / "uploads" / "asset-meta").string();
        fs::create_directories(metaDir);
        std::string metaPath = (fs::path(metaDir) / (id + ".json")).string();
        nlohmann::json meta;
        if (fs::exists(metaPath)) {
            try { std::ifstream f(metaPath); meta = nlohmann::json::parse(f); } catch (...) {}
        }
        if (body.contains("tags")) meta["tags"] = body["tags"];
        if (body.contains("description")) meta["description"] = body["description"];
        std::ofstream f(metaPath);
        f << meta.dump(2);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // Get asset metadata
    svr.Get(R"(/api/assets/(.+)/meta)", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string id = req.matches[1];
        if (id.find("..") != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid id"})", "application/json");
            return;
        }
        std::string metaDir = (fs::path(ctx.config->workspace) / "uploads" / "asset-meta").string();
        std::string metaPath = (fs::path(metaDir) / (id + ".json")).string();
        if (fs::exists(metaPath)) {
            try {
                std::ifstream f(metaPath);
                auto meta = nlohmann::json::parse(f);
                res.set_content(meta.dump(), "application/json");
                return;
            } catch (...) {}
        }
        res.set_content(R"({})", "application/json");
    });

    // Analyze asset with AI
    svr.Post(R"(/api/assets/(.+)/analyze)", [ctx](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string id = req.matches[1];
        if (id.find("..") != std::string::npos) {
            res.status = 400;
            res.set_content(R"({"error":"invalid id"})", "application/json");
            return;
        }
        std::string mediaDir = (fs::path(ctx.config->workspace) / "uploads" / "avacli-media").string();
        std::string filePath = (fs::path(mediaDir) / id).string();
        if (!fs::exists(filePath)) {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
            return;
        }

        std::string apiKey, chatUrl;
        resolveXaiAuth(*ctx.config, apiKey, chatUrl);
        if (apiKey.empty()) {
            res.status = 500;
            res.set_content(R"({"error":"No API key configured"})", "application/json");
            return;
        }

        std::string ext = fs::path(filePath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        nlohmann::json messages = nlohmann::json::array();
        bool isImage = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp");

        if (isImage) {
            std::ifstream imgFile(filePath, std::ios::binary);
            std::string imgData((std::istreambuf_iterator<char>(imgFile)), std::istreambuf_iterator<char>());
            std::string mime = (ext == ".jpg" || ext == ".jpeg") ? "image/jpeg" : "image/png";
            // base64 encode
            static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string encoded;
            encoded.reserve(((imgData.size() + 2) / 3) * 4);
            for (size_t i = 0; i < imgData.size(); i += 3) {
                unsigned int n = ((unsigned char)imgData[i]) << 16;
                if (i + 1 < imgData.size()) n |= ((unsigned char)imgData[i + 1]) << 8;
                if (i + 2 < imgData.size()) n |= ((unsigned char)imgData[i + 2]);
                encoded += b64[(n >> 18) & 63];
                encoded += b64[(n >> 12) & 63];
                encoded += (i + 1 < imgData.size()) ? b64[(n >> 6) & 63] : '=';
                encoded += (i + 2 < imgData.size()) ? b64[n & 63] : '=';
            }
            std::string dataUri = "data:" + mime + ";base64," + encoded;
            messages.push_back({
                {"role", "system"},
                {"content", "Analyze this file. Return a JSON object with: {\"description\": \"1-2 sentence description\", \"tags\": [\"tag1\", \"tag2\", ...]} where tags are semantic keywords. Return ONLY valid JSON, no markdown."}
            });
            messages.push_back({
                {"role", "user"},
                {"content", nlohmann::json::array({
                    {{"type", "text"}, {"text", "Analyze this image and generate a description and semantic tags."}},
                    {{"type", "image_url"}, {"image_url", {{"url", dataUri}, {"detail", "low"}}}}
                })}
            });
        } else {
            std::ifstream textFile(filePath);
            std::string content((std::istreambuf_iterator<char>(textFile)), std::istreambuf_iterator<char>());
            if (content.size() > 8000) content = content.substr(0, 8000) + "\n...(truncated)";
            messages.push_back({
                {"role", "system"},
                {"content", "Analyze this file. Return a JSON object with: {\"description\": \"1-2 sentence description\", \"tags\": [\"tag1\", \"tag2\", ...]} where tags are semantic keywords. Return ONLY valid JSON, no markdown."}
            });
            messages.push_back({
                {"role", "user"},
                {"content", "File: " + id + "\n\n" + content}
            });
        }

        nlohmann::json body;
        std::string analysisModel = ctx.config->extraModel.empty() ? ctx.config->model : ctx.config->extraModel;
        body["model"] = analysisModel;
        body["messages"] = messages;
        body["max_tokens"] = 500;
        body["stream"] = false;

        try {
            httplib::Client cli("https://api.x.ai");
            cli.set_bearer_token_auth(apiKey);
            cli.set_connection_timeout(30);
            cli.set_read_timeout(60);
            auto apiRes = cli.Post("/v1/chat/completions",
                body.dump(), "application/json");
            if (!apiRes || apiRes->status != 200) {
                res.status = 502;
                std::string errBody = apiRes ? apiRes->body.substr(0, 200) : "no response";
                res.set_content("{\"error\":\"API call failed: " + errBody + "\"}", "application/json");
                return;
            }
            auto result = nlohmann::json::parse(apiRes->body);
            std::string assistantContent;
            if (result.contains("choices") && result["choices"].is_array() && !result["choices"].empty())
                assistantContent = result["choices"][0]["message"]["content"].get<std::string>();

            // Try to parse JSON from the response
            nlohmann::json analysis;
            try {
                // Strip markdown code fences if present
                std::string cleaned = assistantContent;
                if (cleaned.find("```json") == 0) cleaned = cleaned.substr(7);
                else if (cleaned.find("```") == 0) cleaned = cleaned.substr(3);
                auto endFence = cleaned.rfind("```");
                if (endFence != std::string::npos) cleaned = cleaned.substr(0, endFence);
                // Trim whitespace
                while (!cleaned.empty() && (cleaned[0] == '\n' || cleaned[0] == ' ')) cleaned.erase(0, 1);
                while (!cleaned.empty() && (cleaned.back() == '\n' || cleaned.back() == ' ')) cleaned.pop_back();
                analysis = nlohmann::json::parse(cleaned);
            } catch (...) {
                analysis = {{"description", assistantContent}, {"tags", nlohmann::json::array()}};
            }

            // Save to metadata
            std::string metaDir = (fs::path(ctx.config->workspace) / "uploads" / "asset-meta").string();
            fs::create_directories(metaDir);
            std::string metaPath = (fs::path(metaDir) / (id + ".json")).string();
            nlohmann::json meta;
            if (fs::exists(metaPath)) {
                try { std::ifstream f(metaPath); meta = nlohmann::json::parse(f); } catch (...) {}
            }
            if (analysis.contains("description")) meta["description"] = analysis["description"];
            if (analysis.contains("tags")) meta["tags"] = analysis["tags"];
            std::ofstream mf(metaPath);
            mf << meta.dump(2);

            LogBuffer::instance().info("assets", "Asset analyzed", {{"id", id}});
            res.set_content(analysis.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
        }
    });

    svr.Get("/api/files/list", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string dir = "";
        auto it = req.params.find("dir");
        if (it != req.params.end()) dir = it->second;

        try {
            auto wsPath = fs::canonical(ctx.config->workspace);
            fs::path target = dir.empty() ? wsPath : (wsPath / dir);
            if (!fs::exists(target) || !fs::is_directory(target)) {
                res.status = 404;
                res.set_content(R"({"error":"directory not found"})", "application/json");
                return;
            }
            auto resolved = fs::canonical(target);
            if (resolved.string().find(wsPath.string()) != 0) {
                res.status = 403;
                res.set_content(R"({"error":"path outside workspace"})", "application/json");
                return;
            }

            static const std::unordered_set<std::string> SKIP = {
                ".git", "node_modules", "vendor", "build", "__pycache__", ".cache",
                ".DS_Store", "dist", ".next", ".nuxt"
            };

            nlohmann::json arr = nlohmann::json::array();
            std::vector<fs::directory_entry> entries;
            std::error_code iterEc;
            for (fs::directory_iterator it2(resolved, fs::directory_options::skip_permission_denied, iterEc), end;
                 !iterEc && it2 != end; it2.increment(iterEc)) {
                try { entries.push_back(*it2); } catch (...) {}
            }

            std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                std::error_code ea, eb;
                bool da = fs::is_directory(a.path(), ea);
                bool db = fs::is_directory(b.path(), eb);
                if (da != db) return da;
                return a.path().filename().string() < b.path().filename().string();
            });

            for (const auto& e : entries) {
                try {
                    std::string name = e.path().filename().string();
                    if (SKIP.count(name)) continue;

                    std::error_code stEc;
                    fs::file_status st = fs::status(e.path(), stEc);
                    if (stEc) continue;

                    nlohmann::json node;
                    node["name"] = name;
                    std::error_code relEc;
                    fs::path rel = fs::relative(e.path(), wsPath, relEc);
                    node["path"] = relEc ? name : rel.generic_string();

                    if (fs::is_directory(st)) {
                        node["type"] = "dir";
                    } else if (fs::is_regular_file(st) || fs::is_symlink(st)) {
                        node["type"] = "file";
                        std::error_code fsec;
                        auto sz = fs::file_size(e.path(), fsec);
                        node["size"] = fsec ? 0 : sz;
                    } else continue;

                    arr.push_back(node);
                } catch (...) { continue; }
            }

            nlohmann::json j;
            j["entries"] = arr;
            j["dir"] = dir;
            j["workspace"] = wsPath.string();
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err;
            err["error"] = std::string("list failed: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });
}

} // namespace avacli
