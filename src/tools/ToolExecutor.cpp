#include "tools/ToolExecutor.hpp"
#include "tools/ToolRegistry.hpp"
#include "tools/ProjectNotes.hpp"
#include "config/ApiKeyStorage.hpp"
#include "config/VaultStorage.hpp"
#include "config/ServeSettings.hpp"
#include "platform/Paths.hpp"
#include "platform/PortableTime.hpp"
#include "platform/ProcessRunner.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <map>
#include <chrono>
#include <random>
#include <thread>

namespace avacli {

namespace fs = std::filesystem;

namespace {

std::string resolvePath(const std::string& workspace, const std::string& path) {
    return resolveWorkspacePath(workspace, path);
}

std::string generateUuid() {
    static std::mt19937 rng(std::random_device{}());
    static const char hex[] = "0123456789abcdef";
    std::string id;
    id.reserve(16);
    for (int i = 0; i < 16; ++i) id += hex[rng() % 16];
    return id;
}

std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    utcTmFromTime(t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

constexpr int MAX_UNDO_ENTRIES = 50;

std::string undoLogPath() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return (fs::path(home) / ".avacli" / "edit_undo_log.json").string();
}

nlohmann::json readUndoLog() {
    std::string path = undoLogPath();
    if (!fs::exists(path)) return nlohmann::json::array();
    try {
        std::ifstream ifs(path);
        auto j = nlohmann::json::parse(ifs);
        if (j.is_array()) return j;
    } catch (...) {}
    return nlohmann::json::array();
}

void writeUndoLog(const nlohmann::json& log) {
    std::string path = undoLogPath();
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream ofs(path, std::ios::trunc);
    ofs << log.dump();
}

void saveUndoEntry(const std::string& filePath, const std::string& content) {
    try {
        auto log = readUndoLog();
        nlohmann::json entry;
        entry["file"] = filePath;
        entry["content"] = content;
        entry["timestamp"] = currentTimestamp();
        log.push_back(entry);
        while (static_cast<int>(log.size()) > MAX_UNDO_ENTRIES)
            log.erase(log.begin());
        writeUndoLog(log);
    } catch (...) {}
}

bool popUndoEntry(const std::string& filePath, std::string& outContent) {
    try {
        auto log = readUndoLog();
        for (int i = static_cast<int>(log.size()) - 1; i >= 0; --i) {
            if (log[i].value("file", "") == filePath) {
                outContent = log[i].value("content", "");
                log.erase(log.begin() + i);
                writeUndoLog(log);
                return true;
            }
        }
    } catch (...) {}
    return false;
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) lines.push_back(line);
    return lines;
}

std::string generateSimpleDiff(const std::string& before, const std::string& after, const std::string& path) {
    auto oldLines = splitLines(before);
    auto newLines = splitLines(after);
    std::string diff = "--- " + path + "\n+++ " + path + "\n";
    size_t maxLines = std::max(oldLines.size(), newLines.size());
    size_t diffChars = diff.size();
    constexpr size_t MAX_DIFF = 3000;
    size_t oi = 0, ni = 0;
    while (oi < oldLines.size() || ni < newLines.size()) {
        if (oi < oldLines.size() && ni < newLines.size() && oldLines[oi] == newLines[ni]) {
            oi++; ni++;
            continue;
        }
        size_t ctxStart = (oi >= 2) ? oi - 2 : 0;
        std::string chunk = "@@ -" + std::to_string(ctxStart + 1) + " @@\n";
        for (size_t c = ctxStart; c < oi && c < oldLines.size(); c++)
            chunk += " " + oldLines[c] + "\n";
        while (oi < oldLines.size() && (ni >= newLines.size() || oldLines[oi] != newLines[ni])) {
            chunk += "-" + oldLines[oi] + "\n";
            oi++;
            if (diffChars + chunk.size() > MAX_DIFF) break;
        }
        while (ni < newLines.size() && (oi >= oldLines.size() || newLines[ni] != oldLines[oi])) {
            chunk += "+" + newLines[ni] + "\n";
            ni++;
            if (diffChars + chunk.size() > MAX_DIFF) break;
        }
        size_t ctxEnd = std::min(oi + 2, oldLines.size());
        for (size_t c = oi; c < ctxEnd && c < oldLines.size(); c++) {
            if (ni < newLines.size() && oldLines[c] == newLines[ni]) {
                chunk += " " + oldLines[c] + "\n";
                oi++; ni++;
            }
        }
        diff += chunk;
        diffChars = diff.size();
        if (diffChars > MAX_DIFF) { diff += "... (truncated)\n"; break; }
    }
    if (diff.size() <= path.size() * 2 + 20) return "";
    return diff;
}

bool globMatch(const std::string& name, const std::string& pattern) {
    size_t p = 0, n = 0;
    size_t starP = std::string::npos, starN = 0;
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == name[n] || pattern[p] == '?')) {
            ++p; ++n;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starP = p++; starN = n;
        } else if (starP != std::string::npos) {
            p = starP + 1; n = ++starN;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

// ── Cross-platform Python finder ─────────────────────────

std::string findPythonCommand() {
    const char* envPy = std::getenv("AVALYNNAI_PYTHON");
    if (envPy && envPy[0] != '\0') return std::string(envPy);

#if defined(_WIN32)
    // Try py launcher first (standard on Windows), then python3, then python
    const char* candidates[] = {"py -3", "python3", "python"};
#elif defined(__APPLE__)
    const char* candidates[] = {"python3", "/usr/local/bin/python3", "python"};
#else
    const char* candidates[] = {"python3", "python"};
#endif
    for (const auto& cmd : candidates) {
        auto pr = runShellCommand("", std::string(cmd) + " --version", 5);
        if (pr.exitCode == 0 && pr.capturedOutput.find("Python") != std::string::npos)
            return std::string(cmd);
    }
    return "python3";
}

static std::string cachedPythonCmd;
std::string getPythonCommand() {
    if (cachedPythonCmd.empty()) cachedPythonCmd = findPythonCommand();
    return cachedPythonCmd;
}

// ── HTTP helpers for media generation ────────────────────

struct HttpResponse {
    std::string body;
    long statusCode = 0;
    bool ok() const { return statusCode >= 200 && statusCode < 300; }
};

size_t curlWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t curlFileCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* f = static_cast<std::ofstream*>(userdata);
    f->write(ptr, size * nmemb);
    return size * nmemb;
}

std::string getXaiApiKey() {
    const char* env = std::getenv("XAI_API_KEY");
    if (env && env[0] != '\0') return std::string(env);
    return "";
}

std::string resolvedXaiApiKey() {
    try {
        auto j = loadServeSettingsJson();
        if (j.contains("xai_api_key") && j["xai_api_key"].is_string()) {
            std::string sk = j["xai_api_key"].get<std::string>();
            if (!sk.empty())
                return sk;
        }
    } catch (...) {}
    std::string k = getXaiApiKey();
    if (!k.empty()) return k;
    return ApiKeyStorage::load();
}

HttpResponse httpPostJsonTimeout(const std::string& url, const std::string& jsonBody, const std::string& apiKey,
                                 long timeoutSeconds) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) return resp;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!apiKey.empty()) {
        std::string auth = "Authorization: Bearer " + apiKey;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.statusCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

HttpResponse httpPostJson(const std::string& url, const std::string& jsonBody, const std::string& apiKey) {
    return httpPostJsonTimeout(url, jsonBody, apiKey, 120L);
}

/// xAI Responses API: extract assistant text from `output` message blocks.
std::string extractResponsesOutputText(const nlohmann::json& root) {
    std::string acc;
    if (root.contains("output") && root["output"].is_array()) {
        for (const auto& item : root["output"]) {
            if (item.value("type", "") != "message")
                continue;
            if (!item.contains("content") || !item["content"].is_array())
                continue;
            for (const auto& c : item["content"]) {
                if (c.value("type", "") == "output_text" && c.contains("text") && c["text"].is_string())
                    acc += c["text"].get<std::string>();
            }
        }
    }
    return acc;
}

/// Calls POST https://api.x.ai/v1/responses with server-side web_search or x_search.
std::string runXaiLiveSearch(const std::string& apiKey, const std::string& model, const std::string& userContent,
                             const char* serverToolType, int numResults) {
    if (apiKey.empty())
        return R"({"error": "No API key: set XAI_API_KEY or run avacli --set-api-key"})";

    std::string useModel = model.empty() ? "grok-4-fast-non-reasoning" : model;

    std::string instructions =
        "Use the provided search tool to gather current information. Answer concisely (short paragraphs or bullets). "
        "When helpful, mention up to " +
        std::to_string(std::max(1, numResults)) + " distinct sources or findings.\n\n";

    nlohmann::json body;
    body["model"] = useModel;
    body["include"] = nlohmann::json::array({"no_inline_citations"});
    body["input"] = nlohmann::json::array({
        {{"role", "user"}, {"content", instructions + userContent}},
    });
    body["tools"] = nlohmann::json::array({{{"type", serverToolType}}});

    auto resp = httpPostJsonTimeout("https://api.x.ai/v1/responses", body.dump(), apiKey, 180L);

    if (!resp.ok()) {
        nlohmann::json err;
        err["error"] = "xAI Responses API HTTP " + std::to_string(resp.statusCode);
        std::string snippet = resp.body.size() > 800 ? resp.body.substr(0, 800) + "..." : resp.body;
        err["detail"] = snippet;
        return err.dump();
    }

    try {
        auto root = nlohmann::json::parse(resp.body);
        std::string text = extractResponsesOutputText(root);
        nlohmann::json out;
        out["summary"] = text;
        if (root.contains("citations"))
            out["citations"] = root["citations"];
        else
            out["citations"] = nlohmann::json::array();

        nlohmann::json results = nlohmann::json::array();
        if (root.contains("citations") && root["citations"].is_array()) {
            for (const auto& c : root["citations"]) {
                if (c.is_string()) {
                    std::string u = c.get<std::string>();
                    results.push_back({{"title", u}, {"url", u}, {"snippet", ""}});
                }
            }
        }
        out["results"] = results;
        out["provider"] = "xai";
        out["tool"] = serverToolType;
        return out.dump();
    } catch (const std::exception& e) {
        nlohmann::json err;
        err["error"] = std::string("Failed to parse Responses JSON: ") + e.what();
        return err.dump();
    }
}

HttpResponse httpGet(const std::string& url, const std::string& apiKey,
                       const char* userAgent = nullptr) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) return resp;

    struct curl_slist* headers = nullptr;
    if (!apiKey.empty()) {
        std::string auth = "Authorization: Bearer " + apiKey;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (userAgent && userAgent[0] != '\0')
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.statusCode);

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

bool downloadToFile(const std::string& url, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlFileCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    file.close();
    return res == CURLE_OK;
}

std::string readFileToBase64(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(data.size() * 4 / 3 + 4);
    size_t i = 0;
    while (i < data.size()) {
        uint32_t a = i < data.size() ? (uint8_t)data[i++] : 0;
        uint32_t b = i < data.size() ? (uint8_t)data[i++] : 0;
        uint32_t c = i < data.size() ? (uint8_t)data[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        out += b64[(triple >> 18) & 0x3f];
        out += b64[(triple >> 12) & 0x3f];
        out += (i > data.size() + 1) ? '=' : b64[(triple >> 6) & 0x3f];
        out += (i > data.size()) ? '=' : b64[triple & 0x3f];
    }
    return out;
}

static const std::vector<std::string> SKIP_DIRS = {
    ".git", "node_modules", "build", "dist", ".cache",
    "__pycache__", "target", ".next", "vendor"
};

} // namespace

ToolExecutor::ToolExecutor(const std::string& workspace, Mode mode, AskUserFn askUser)
    : workspace_(workspace), mode_(mode), askUser_(std::move(askUser)) {}

std::string ToolExecutor::execute(const std::string& name, const std::string& arguments) {
    if (!ToolRegistry::hasTool(name, mode_)) {
        return R"({"error": "Unknown tool: )" + name + "\"}";
    }

    nlohmann::json args;
    try {
        args = arguments.empty() ? nlohmann::json::object() : nlohmann::json::parse(arguments);
    } catch (const nlohmann::json::exception& e) {
        return R"({"error": "Invalid JSON arguments: )" + std::string(e.what()) + "\"}";
    }

    auto get = [&args](const std::string& k, const std::string& def = "") -> std::string {
        return args.contains(k) && args[k].is_string() ? args[k].get<std::string>() : def;
    };
    auto getInt = [&args](const std::string& k, int def = 0) -> int {
        return args.contains(k) && args[k].is_number() ? args[k].get<int>() : def;
    };
    auto getBool = [&args](const std::string& k, bool def = false) -> bool {
        return args.contains(k) && args[k].is_boolean() ? args[k].get<bool>() : def;
    };

    try {
        // ── read_file ────────────────────────────────────
        if (name == "read_file") {
            std::string path = resolvePath(workspace_, get("path"));
            int startLine = getInt("start_line", 1);
            int endLine = getInt("end_line", INT_MAX);

            std::ifstream f(path);
            if (!f) return R"({"error": "Cannot open file: )" + path + "\"}";

            std::string line;
            std::stringstream out;
            int ln = 0;
            while (std::getline(f, line)) {
                ++ln;
                if (ln >= startLine && ln <= endLine)
                    out << line << "\n";
                if (ln > endLine) break;
            }
            nlohmann::json j;
            j["content"] = out.str();
            j["path"] = path;
            return j.dump();
        }

        // ── search_files ─────────────────────────────────
        if (name == "search_files") {
            std::string query = get("query");
            std::string dir = resolvePath(workspace_, get("directory", "."));
            bool recursive = getBool("recursive", true);

            if (query.empty()) return R"({"error": "query is required"})";

            std::vector<std::string> matches;
            bool usedGrep = false;

#if !defined(_WIN32)
            if (fs::exists(dir) && fs::is_directory(dir) && recursive) {
                std::string qesc;
                for (char c : query) {
                    if (c == '\'') qesc += "'\\''";
                    else qesc += c;
                }
                std::string cmd = "grep -rl"
                    " --exclude-dir=.git --exclude-dir=node_modules"
                    " --exclude-dir=build --exclude-dir=dist"
                    " --exclude-dir=.cache --exclude-dir=__pycache__"
                    " --exclude-dir=target --exclude-dir=.next"
                    " --exclude-dir=vendor"
                    " --exclude='*.o' --exclude='*.a' --exclude='*.so'"
                    " -e '" + qesc + "' " + dir + " 2>/dev/null";
                auto pr = runShellCommand(".", cmd, 120);
                // 127: command not found — fall back to directory walk
                if (!pr.timedOut && pr.exitCode >= 0 && pr.exitCode != 127) {
                    usedGrep = true;
                    std::istringstream iss(pr.capturedOutput);
                    std::string line;
                    while (std::getline(iss, line)) {
                        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                            line.pop_back();
                        if (!line.empty())
                            matches.push_back(line);
                    }
                }
            }
#endif

            if (!usedGrep) {
                auto searchDir = [&](const fs::path& p, auto&& self) {
                    if (!fs::exists(p) || !fs::is_directory(p)) return;
                    try {
                        for (const auto& e : fs::directory_iterator(p)) {
                            std::string fname = e.path().filename().string();
                            if (fname.starts_with('.')) continue;
                            if (fs::is_directory(e.status())) {
                                if (!recursive) continue;
                                bool skip = false;
                                for (const auto& sd : SKIP_DIRS)
                                    if (fname == sd) { skip = true; break; }
                                if (skip) continue;
                                self(e.path(), self);
                            } else if (fs::is_regular_file(e.status())) {
                                std::ifstream f(e.path());
                                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                                if (content.find(query) != std::string::npos)
                                    matches.push_back(e.path().string());
                            }
                        }
                    } catch (const fs::filesystem_error&) {}
                };
                searchDir(dir, searchDir);
            }

            nlohmann::json j;
            j["matches"] = matches;
            return j.dump();
        }

        // ── list_directory ───────────────────────────────
        if (name == "list_directory") {
            std::string path = resolvePath(workspace_, get("path", "."));
            std::vector<std::string> entries;

            if (!fs::exists(path) || !fs::is_directory(path))
                return R"({"error": "Not a directory: )" + path + "\"}";

            for (const auto& e : fs::directory_iterator(path))
                entries.push_back(e.path().filename().string());

            nlohmann::json j;
            j["entries"] = entries;
            j["path"] = path;
            return j.dump();
        }

        // ── glob_files ───────────────────────────────────
        if (name == "glob_files") {
            std::string pattern = get("pattern");
            std::string dir = resolvePath(workspace_, get("directory", "."));
            if (pattern.empty()) return R"({"error": "pattern required"})";

            std::string simplePattern = pattern;
            if (simplePattern.starts_with("**/")) simplePattern = simplePattern.substr(3);

            std::vector<std::string> matches;
            std::function<void(const fs::path&)> scan = [&](const fs::path& p) {
                if (!fs::exists(p) || !fs::is_directory(p)) return;
                if (matches.size() >= 200) return;
                try {
                    for (const auto& e : fs::directory_iterator(p)) {
                        std::string fname = e.path().filename().string();
                        if (fname.starts_with('.') && fname != ".") continue;
                        if (e.is_directory()) {
                            bool skip = false;
                            for (const auto& sd : SKIP_DIRS)
                                if (fname == sd) { skip = true; break; }
                            if (!skip) scan(e.path());
                        } else if (e.is_regular_file()) {
                            std::string relPath = fs::relative(e.path(), dir).string();
                            if (globMatch(fname, simplePattern) || globMatch(relPath, pattern)) {
                                matches.push_back(e.path().string());
                                if (matches.size() >= 200) return;
                            }
                        }
                    }
                } catch (const fs::filesystem_error&) {}
            };
            scan(dir);

            nlohmann::json j;
            j["matches"] = matches;
            j["count"] = matches.size();
            return j.dump();
        }

        // ── read_url ─────────────────────────────────────
        if (name == "read_url") {
            std::string url = get("url");
            int maxBytes = getInt("max_bytes", 8000);
            if (url.empty()) return R"({"error": "url required"})";

            auto resp = httpGet(url, "");
            if (!resp.ok())
                return R"({"error": "HTTP )" + std::to_string(resp.statusCode) + ": failed to fetch URL\"}";

            // Strip HTML tags and normalize whitespace
            std::string stripped;
            bool inTag = false, inScript = false;
            for (size_t i = 0; i < resp.body.size(); ++i) {
                if (resp.body[i] == '<') {
                    std::string ahead;
                    for (size_t k = i; k < std::min(i + 10, resp.body.size()); ++k)
                        ahead += std::tolower(resp.body[k]);
                    if (ahead.starts_with("<script") || ahead.starts_with("<style")) inScript = true;
                    if (ahead.starts_with("</script") || ahead.starts_with("</style")) inScript = false;
                    inTag = true;
                    continue;
                }
                if (resp.body[i] == '>') { inTag = false; continue; }
                if (!inTag && !inScript) stripped += resp.body[i];
            }
            // Collapse whitespace
            std::string result;
            bool lastSpace = false;
            for (char c : stripped) {
                if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                if (c == ' ' && lastSpace) continue;
                result += c;
                lastSpace = (c == ' ');
            }
            if ((int)result.size() > maxBytes) result = result.substr(0, maxBytes) + "... (truncated)";

            nlohmann::json j;
            j["content"] = result;
            j["url"] = url;
            j["bytes"] = result.size();
            return j.dump();
        }

        // ── read_project_notes ───────────────────────────
        if (name == "read_project_notes") {
            nlohmann::json j;
            // Workspace GROK_NOTES.md (project-level reference)
            std::string notesPath = resolvePath(workspace_, "GROK_NOTES.md");
            std::ifstream nf(notesPath);
            if (nf) {
                std::string content((std::istreambuf_iterator<char>(nf)), std::istreambuf_iterator<char>());
                if (content.size() > 4096) content = content.substr(0, 4096) + "\n... (truncated)";
                j["project_notes"] = content;
            } else {
                j["project_notes"] = "(GROK_NOTES.md not found)";
            }
            // Session memory
            if (!sessionId_.empty()) {
                auto memories = loadAllMemory(sessionId_);
                auto arr = nlohmann::json::array();
                for (const auto& m : memories) {
                    arr.push_back({{"id", m.id}, {"category", m.category}, {"content", m.content},
                                   {"tags", m.tags}, {"pinned", m.pinned}});
                }
                j["session_memory"] = arr;
                j["memory_count"] = memories.size();
            }
            return j.dump();
        }

        // ── update_project_notes (backward compat -> add_memory) ──
        if (name == "update_project_notes") {
            std::string section = get("section");
            std::string content = get("content");
            if (section.empty() || content.empty())
                return R"({"error": "section and content required"})";
            if (!sessionId_.empty()) {
                MemoryEntry entry;
                entry.category = section;
                entry.content = content;
                addMemoryEntry(sessionId_, entry);
                nlohmann::json j;
                j["status"] = "ok";
                j["note"] = "Saved to session memory (category: " + section + ")";
                return j.dump();
            }
            // Fallback: write to workspace GROK_NOTES.md
            std::string path = resolvePath(workspace_, "GROK_NOTES.md");
            std::ofstream out(path, std::ios::app);
            out << "\n## " << section << "\n" << content << "\n";
            nlohmann::json j;
            j["status"] = "ok";
            j["path"] = path;
            return j.dump();
        }

        // ── add_memory ───────────────────────────────────
        if (name == "add_memory") {
            std::string category = get("category");
            std::string content = get("content");
            bool pinned = getBool("pinned", false);
            if (content.empty()) return R"({"error": "content required"})";
            if (sessionId_.empty()) return R"({"error": "No session - memory requires --session flag"})";

            MemoryEntry entry;
            entry.category = category.empty() ? "custom" : category;
            entry.content = content;
            entry.pinned = pinned;
            if (args.contains("tags") && args["tags"].is_array())
                for (const auto& t : args["tags"])
                    if (t.is_string()) entry.tags.push_back(t.get<std::string>());

            addMemoryEntry(sessionId_, entry);
            nlohmann::json j;
            j["status"] = "ok";
            j["id"] = entry.id.empty() ? "(auto)" : entry.id;
            j["category"] = entry.category;
            return j.dump();
        }

        // ── search_memory ────────────────────────────────
        if (name == "search_memory") {
            if (sessionId_.empty()) return R"({"error": "No session - memory requires --session flag"})";
            std::string query = get("query");
            std::string category = get("category");
            auto results = searchMemory(sessionId_, query, category);
            auto arr = nlohmann::json::array();
            for (const auto& m : results) {
                arr.push_back({{"id", m.id}, {"ts", m.timestamp}, {"category", m.category},
                               {"content", m.content}, {"tags", m.tags}, {"pinned", m.pinned}});
            }
            nlohmann::json j;
            j["results"] = arr;
            j["count"] = results.size();
            return j.dump();
        }

        // ── forget_memory ────────────────────────────────
        if (name == "forget_memory") {
            if (sessionId_.empty()) return R"({"error": "No session - memory requires --session flag"})";
            std::string id = get("id");
            if (id.empty()) return R"({"error": "id required"})";
            bool ok = forgetMemory(sessionId_, id);
            nlohmann::json j;
            j["status"] = ok ? "ok" : "not_found";
            return j.dump();
        }

        // ── web_search (xAI live web search via Responses API) ──
        if (name == "web_search") {
            std::string query = get("query");
            int numResults = getInt("num_results", 8);
            if (query.empty()) return R"({"error": "query required"})";
            return runXaiLiveSearch(resolvedXaiApiKey(), searchModel_, query, "web_search", numResults);
        }

        // ── x_search (xAI live X/Twitter search via Responses API) ──
        if (name == "x_search") {
            std::string query = get("query");
            int numResults = getInt("num_results", 8);
            if (query.empty()) return R"({"error": "query required"})";
            return runXaiLiveSearch(resolvedXaiApiKey(), searchModel_, query, "x_search", numResults);
        }

        // ── read_todos ───────────────────────────────────
        if (name == "read_todos") {
            std::vector<TodoItem> todos;
            if (!sessionId_.empty()) {
                loadSessionTodos(sessionId_, todos);
            } else {
                loadTodosFromFile(workspace_, todos);
            }
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : todos) {
                arr.push_back({{"id", t.id}, {"title", t.title}, {"status", t.status}, {"description", t.description}});
            }
            nlohmann::json j;
            j["todos"] = arr;
            return j.dump();
        }

        // ── create_update_todo ───────────────────────────
        if (name == "create_update_todo") {
            if (!args.contains("todos") || !args["todos"].is_array())
                return R"({"error": "todos array required"})";
            std::vector<TodoItem> todos;
            if (!sessionId_.empty()) {
                loadSessionTodos(sessionId_, todos);
            } else {
                loadTodosFromFile(workspace_, todos);
            }
            std::map<std::string, size_t> idToIdx;
            for (size_t i = 0; i < todos.size(); ++i)
                idToIdx[todos[i].id] = i;
            for (const auto& t : args["todos"]) {
                if (!t.contains("id") || !t.contains("title") || !t.contains("status")) continue;
                TodoItem item;
                item.id = t["id"].get<std::string>();
                item.title = t["title"].get<std::string>();
                item.status = t["status"].get<std::string>();
                item.description = t.value("description", "");
                if (idToIdx.count(item.id)) {
                    todos[idToIdx[item.id]] = item;
                } else {
                    idToIdx[item.id] = todos.size();
                    todos.push_back(item);
                }
            }
            pruneDoneTodos(todos);
            if (!sessionId_.empty()) {
                saveSessionTodos(sessionId_, todos);
            } else {
                saveTodosToFile(workspace_, todos);
            }
            nlohmann::json j;
            j["status"] = "ok";
            j["count"] = todos.size();
            return j.dump();
        }

        // ── ask_user ─────────────────────────────────────
        if (name == "ask_user") {
            std::string question = get("question");
            if (question.empty()) return R"({"error": "question required"})";
            if (!askUser_)
                return R"({"error": "ask_user not configured - no UI callback"})";
            std::string answer = askUser_(question);
            nlohmann::json j;
            j["answer"] = answer;
            return j.dump();
        }

        // ── edit_file ────────────────────────────────────
        if (name == "edit_file") {
            std::string path = resolvePath(workspace_, get("path"));
            if (!args.contains("edits") || !args["edits"].is_array())
                return R"({"error": "edits array required"})";

            std::ifstream in(path);
            if (!in) return R"({"error": "Cannot open file: )" + path + "\"}";

            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();

            std::string originalContent = content;

            int editCount = 0;
            for (const auto& edit : args["edits"]) {
                if (!edit.contains("search") || !edit.contains("replace")) continue;
                std::string s = edit["search"].get<std::string>();
                std::string r = edit["replace"].get<std::string>();
                size_t pos = 0;
                while ((pos = content.find(s, pos)) != std::string::npos) {
                    content.replace(pos, s.size(), r);
                    pos += r.size();
                    editCount++;
                }
            }

            saveUndoEntry(path, originalContent);

            std::ofstream out(path);
            out << content;

            // Track edit
            if (!sessionId_.empty()) {
                EditRecord rec;
                rec.file = path;
                rec.action = "edit";
                rec.summary = std::to_string(editCount) + " replacement(s)";
                addEditRecord(sessionId_, rec);
                editHistory_.push_back(rec);
            }

            nlohmann::json j;
            j["status"] = "ok";
            j["path"] = path;
            j["edits_applied"] = editCount;
            std::string diff = generateSimpleDiff(originalContent, content, path);
            if (!diff.empty()) j["diff"] = diff;
            return j.dump();
        }

        // ── write_file ───────────────────────────────────
        if (name == "write_file") {
            std::string path = resolvePath(workspace_, get("path"));
            std::string content = get("content");

            if (path.empty()) return R"({"error": "path required"})";

            try {
                fs::path dest(path);
                // lexically_normal() can drop a leading ".", leaving a bare filename with an empty
                // parent_path(); create_directories("") throws invalid_argument on libstdc++.
                if (dest.has_parent_path())
                    fs::create_directories(dest.parent_path());
            } catch (const fs::filesystem_error& e) {
                return R"({"error": "Failed to create directories: )" + std::string(e.what()) + "\"}";
            }

            bool existed = fs::exists(path);

            std::ofstream out(path, std::ios::binary);
            if (!out) return R"({"error": "Cannot open file for writing: )" + path + "\"}";
            out << content;

            // Track write
            if (!sessionId_.empty()) {
                EditRecord rec;
                rec.file = path;
                rec.action = existed ? "overwrite" : "create";
                rec.summary = std::to_string(content.size()) + " bytes";
                addEditRecord(sessionId_, rec);
                editHistory_.push_back(rec);
            }

            nlohmann::json j;
            j["status"] = "ok";
            j["path"] = path;
            auto lines = splitLines(content);
            size_t previewLines = std::min(lines.size(), size_t(30));
            std::string preview;
            for (size_t i = 0; i < previewLines; i++) {
                preview += "+" + lines[i] + "\n";
            }
            if (lines.size() > previewLines) preview += "... (" + std::to_string(lines.size()) + " lines total)\n";
            if (!preview.empty()) j["diff"] = "--- /dev/null\n+++ " + path + "\n@@ new file @@\n" + preview;
            return j.dump();
        }

        // ── undo_edit ────────────────────────────────────
        if (name == "undo_edit") {
            std::string path = resolvePath(workspace_, get("path"));

            std::string restored;
            if (!popUndoEntry(path, restored))
                return R"({"error": "No undo history found for: )" + path + "\"}";

            try {
                std::ofstream out(path, std::ios::binary | std::ios::trunc);
                if (!out) return R"({"error": "Cannot write file: )" + path + "\"}";
                out << restored;
            } catch (const std::exception& e) {
                return R"({"error": "Failed to restore: )" + std::string(e.what()) + "\"}";
            }

            if (!sessionId_.empty()) {
                EditRecord rec;
                rec.file = path;
                rec.action = "undo";
                rec.summary = "Restored from undo log";
                addEditRecord(sessionId_, rec);
                editHistory_.push_back(rec);
            }

            nlohmann::json j;
            j["status"] = "ok";
            j["path"] = path;
            j["message"] = "Restored previous version from undo log";
            return j.dump();
        }

        // ── run_shell ────────────────────────────────────
        if (name == "run_shell") {
            std::string command = get("command");
            std::string wd = resolvePath(workspace_, get("working_dir", "."));
            int timeout = getInt("timeout", 60);
            if (command.empty()) return R"({"error": "command required"})";

            auto isDangerous = [](const std::string& c) -> bool {
                std::string lower;
                for (unsigned char ch : c) lower += static_cast<char>(std::tolower(ch));
                if (lower.find("rm ") != std::string::npos && lower.find("-rf") != std::string::npos) return true;
                if (lower.find("rm -rf") != std::string::npos) return true;
                if (lower.find("sudo ") != std::string::npos || lower.find(" sudo") != std::string::npos) return true;
                if (lower.find("> /dev/") != std::string::npos || lower.find(">/dev/") != std::string::npos) return true;
                if (lower.find("dd if=") != std::string::npos) return true;
                if (lower.find("mkfs") != std::string::npos) return true;
                if (lower.find(":(){") != std::string::npos) return true;
                if (lower.find("chmod 777") != std::string::npos) return true;
                return false;
            };

            if (isDangerous(command) && askUser_) {
                std::string answer = askUser_("Dangerous command detected: \"" + command + "\". Run anyway? (yes/no)");
                if (answer.find("yes") == std::string::npos)
                    return R"({"error": "User declined to run dangerous command"})";
            }

            auto pr = runShellCommand(wd, command, timeout);
            nlohmann::json j;
            j["stdout"] = pr.capturedOutput;
            j["exit_code"] = pr.exitCode;
            if (pr.timedOut)
                j["timed_out"] = true;
            return j.dump();
        }

        // ── run_tests ────────────────────────────────────
        if (name == "run_tests") {
            std::string wd = resolvePath(workspace_, get("working_dir", "."));
            if (!fs::exists(wd) || !fs::is_directory(wd)) {
                nlohmann::json j;
                j["error"] = "Working directory does not exist";
                return j.dump();
            }
            std::string command;
            fs::path p(wd);
            if (fs::exists(p / "Makefile")) {
                std::ifstream mf(p / "Makefile");
                std::string content((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
                mf.close();
                if (content.find("test:") != std::string::npos)
                    command = "make test";
                else if (content.find("check:") != std::string::npos)
                    command = "make check";
                else
                    command = "make";
            } else if (fs::exists(p / "Cargo.toml")) {
                command = "cargo test";
            } else if (fs::exists(p / "package.json")) {
                command = "npm test";
            } else if (fs::exists(p / "go.mod")) {
                command = "go test ./...";
            } else if (fs::exists(p / "CMakeLists.txt")) {
                std::string buildDir = (p / "build").string();
#if defined(_WIN32)
                if (fs::exists(buildDir))
                    command = "cmake --build build & ctest --test-dir build";
                else
                    command = "cmake -B build -S . & cmake --build build";
#else
                if (fs::exists(buildDir))
                    command = "cmake --build build 2>/dev/null && (ctest --test-dir build 2>/dev/null || ctest 2>/dev/null || true)";
                else
                    command = "cmake -B build -S . 2>/dev/null && cmake --build build 2>/dev/null";
#endif
            } else if (fs::exists(p / "pyproject.toml") || fs::exists(p / "pytest.ini") || fs::exists(p / "setup.py")) {
#if defined(_WIN32)
                command = "python -m pytest -v";
#else
                command = "pytest -v 2>/dev/null || python -m pytest -v 2>/dev/null";
#endif
            } else {
                nlohmann::json j;
                j["error"] = "No supported test framework detected";
                j["hint"] = "Use run_shell with a specific command instead";
                return j.dump();
            }
            auto pr = runShellCommand(wd, command, 120);
            nlohmann::json j;
            j["stdout"] = pr.capturedOutput;
            j["command"] = command;
            j["exit_code"] = pr.exitCode;
            if (pr.timedOut)
                j["timed_out"] = true;
            return j.dump();
        }

        // ── custom Python tools ───────────────────────────
        if (ToolRegistry::isCustomTool(name)) {
            nlohmann::json ja;
            try { ja = nlohmann::json::parse(arguments); } catch (...) {
                return R"({"error":"invalid JSON arguments for custom tool"})";
            }

            std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
            std::string scriptPath = (fs::path(toolsDir) / (name + ".py")).string();
            if (!fs::exists(scriptPath)) {
                nlohmann::json ej;
                ej["error"] = "Custom tool script not found: " + name + ".py";
                return ej.dump();
            }

            std::string inputJson = ja.dump();
            std::string inputPath = (fs::path(toolsDir) / (name + "_" + generateUuid() + ".json")).string();
            {
                std::ofstream ifs(inputPath);
                ifs << inputJson;
            }

            std::string pythonCmd = getPythonCommand();
            std::string cmd = pythonCmd + " \"" + scriptPath + "\" \"" + inputPath + "\"";
            auto pr = runShellCommand(toolsDir, cmd, 120);

            // Clean up input file
            try { fs::remove(inputPath); } catch (...) {}

            nlohmann::json result;
            result["exit_code"] = pr.exitCode;
            if (pr.timedOut) {
                result["error"] = "Tool execution timed out (120s)";
                result["timed_out"] = true;
            } else if (pr.exitCode != 0) {
                result["error"] = "Tool exited with code " + std::to_string(pr.exitCode);
                result["output"] = pr.capturedOutput;
            } else {
                // Try to parse output as JSON
                try {
                    result["output"] = nlohmann::json::parse(pr.capturedOutput);
                } catch (...) {
                    result["output"] = pr.capturedOutput;
                }
            }
            return result.dump();
        }

        // ── generate_image ───────────────────────────────
        if (name == "generate_image") {
            std::string prompt = get("prompt");
            std::string model = get("model", "grok-imagine-image");
            int n = getInt("n", 1);
            std::string aspectRatio = get("aspect_ratio", "");
            std::string resolution = get("resolution", "");
            std::string quality = get("quality", "");
            if (resolution != "1k" && resolution != "2k") resolution = "";
            if (prompt.empty()) return R"({"error": "prompt required"})";

            std::string apiKey = resolvedXaiApiKey();
            if (apiKey.empty()) return R"({"error": "XAI_API_KEY not set"})";

            nlohmann::json body;
            body["model"] = model;
            body["prompt"] = prompt;
            body["n"] = n;
            body["response_format"] = "url";
            if (!aspectRatio.empty()) body["aspect_ratio"] = aspectRatio;
            if (!resolution.empty()) body["resolution"] = resolution;
            if (!quality.empty()) body["quality"] = quality;

            auto resp = httpPostJson("https://api.x.ai/v1/images/generations", body.dump(), apiKey);
            if (!resp.ok()) {
                nlohmann::json j;
                j["error"] = "API error (" + std::to_string(resp.statusCode) + "): " + resp.body.substr(0, 500);
                return j.dump();
            }

            try {
                auto result = nlohmann::json::parse(resp.body);
                nlohmann::json output;
                auto arr = nlohmann::json::array();
                if (result.contains("data") && result["data"].is_array()) {
                    std::string mediaDir = (fs::path(workspace_) / "uploads" / "avacli-media").string();
                    fs::create_directories(mediaDir);
                    for (const auto& item : result["data"]) {
                        std::string imgUrl = item.value("url", "");
                        if (imgUrl.empty()) continue;
                        std::string fname = "img_" + generateUuid() + ".png";
                        std::string localPath = (fs::path(mediaDir) / fname).string();
                        if (downloadToFile(imgUrl, localPath)) {
                            arr.push_back({{"path", localPath}, {"url", "/uploads/avacli-media/" + fname}});
                        }
                    }
                }
                output["status"] = arr.empty() ? "no_images" : "ok";
                output["images"] = arr;
                output["model"] = model;
                output["prompt"] = prompt;
                return output.dump();
            } catch (const std::exception& e) {
                return R"({"error": "Failed to parse response: )" + std::string(e.what()) + "\"}";
            }
        }

        // ── edit_image ───────────────────────────────────
        if (name == "edit_image") {
            std::string prompt = get("prompt");
            std::string imagePath = resolvePath(workspace_, get("image_path"));
            std::string model = get("model", "grok-imagine-image");
            if (prompt.empty()) return R"({"error": "prompt required"})";
            if (!fs::exists(imagePath)) return R"({"error": "Image not found: )" + imagePath + "\"}";

            std::string apiKey = resolvedXaiApiKey();
            if (apiKey.empty()) return R"({"error": "XAI_API_KEY not set"})";

            std::string b64 = readFileToBase64(imagePath);
            if (b64.empty()) return R"({"error": "Failed to read image"})";

            std::string ext = fs::path(imagePath).extension().string();
            std::string mime = (ext == ".jpg" || ext == ".jpeg") ? "image/jpeg" : "image/png";

            nlohmann::json body;
            body["model"] = model;
            body["prompt"] = prompt;
            body["image"]["url"] = "data:" + mime + ";base64," + b64;
            body["image"]["type"] = "image_url";
            body["n"] = 1;
            body["response_format"] = "url";

            auto resp = httpPostJson("https://api.x.ai/v1/images/edits", body.dump(), apiKey);
            if (!resp.ok()) {
                nlohmann::json j;
                j["error"] = "API error (" + std::to_string(resp.statusCode) + "): " + resp.body.substr(0, 500);
                return j.dump();
            }

            try {
                auto result = nlohmann::json::parse(resp.body);
                if (result.contains("data") && result["data"].is_array() && !result["data"].empty()) {
                    std::string imgUrl = result["data"][0].value("url", "");
                    if (!imgUrl.empty()) {
                        std::string mediaDir = (fs::path(workspace_) / "uploads" / "avacli-media").string();
                        fs::create_directories(mediaDir);
                        std::string fname = "edit_" + generateUuid() + ".png";
                        std::string localPath = (fs::path(mediaDir) / fname).string();
                        if (downloadToFile(imgUrl, localPath)) {
                            nlohmann::json j;
                            j["status"] = "ok";
                            j["path"] = localPath;
                            j["url"] = "/uploads/avacli-media/" + fname;
                            return j.dump();
                        }
                    }
                }
                return R"({"error": "No image in response"})";
            } catch (const std::exception& e) {
                return R"({"error": "Failed to parse response: )" + std::string(e.what()) + "\"}";
            }
        }

        // ── generate_video ───────────────────────────────
        if (name == "generate_video") {
            std::string prompt = get("prompt");
            std::string model = get("model", "grok-imagine-video");
            int duration = getInt("duration", 5);
            std::string aspectRatio = get("aspect_ratio", "16:9");
            std::string resolution = get("resolution", "720p");
            std::string imageSrc = get("image", "");
            if (resolution != "480p" && resolution != "720p") resolution = "720p";
            if (prompt.empty()) return R"({"error": "prompt required"})";

            std::string apiKey = resolvedXaiApiKey();
            if (apiKey.empty()) return R"({"error": "XAI_API_KEY not set"})";

            nlohmann::json body;
            body["model"] = model;
            body["prompt"] = prompt;
            body["duration"] = duration;
            if (!aspectRatio.empty()) body["aspect_ratio"] = aspectRatio;
            body["resolution"] = resolution;

            if (!imageSrc.empty()) {
                if (imageSrc.find("http") == 0 || imageSrc.find("data:") == 0) {
                    body["image"] = {{"url", imageSrc}};
                } else {
                    std::string resolved = resolvePath(workspace_, imageSrc);
                    if (fs::exists(resolved)) {
                        std::string b64 = readFileToBase64(resolved);
                        std::string ext = fs::path(resolved).extension().string();
                        std::string mime = (ext == ".jpg" || ext == ".jpeg") ? "image/jpeg" : "image/png";
                        body["image"] = {{"url", "data:" + mime + ";base64," + b64}};
                    }
                }
            }

            if (args.contains("reference_images") && args["reference_images"].is_array()) {
                auto refs = nlohmann::json::array();
                for (const auto& ref : args["reference_images"]) {
                    std::string url = ref.is_string() ? ref.get<std::string>() : ref.value("url", "");
                    if (!url.empty()) refs.push_back({{"url", url}});
                }
                if (!refs.empty()) body["reference_images"] = refs;
            }

            {
                auto logBody = body;
                if (logBody.contains("image") && logBody["image"].contains("url")) {
                    std::string u = logBody["image"]["url"].get<std::string>();
                    if (u.size() > 100) logBody["image"]["url"] = u.substr(0, 60) + "...[" + std::to_string(u.size()) + " chars]";
                }
                spdlog::info("generate_video request: {}", logBody.dump());
            }

            auto submitResp = httpPostJson("https://api.x.ai/v1/videos/generations", body.dump(), apiKey);
            if (!submitResp.ok()) {
                nlohmann::json j;
                j["error"] = "API error (" + std::to_string(submitResp.statusCode) + "): " + submitResp.body.substr(0, 500);
                return j.dump();
            }

            try {
                auto submitResult = nlohmann::json::parse(submitResp.body);
                std::string requestId = submitResult.value("request_id", "");
                if (requestId.empty()) {
                    std::string videoUrl;
                    if (submitResult.contains("video") && submitResult["video"].contains("url"))
                        videoUrl = submitResult["video"]["url"].get<std::string>();
                    if (!videoUrl.empty()) {
                        std::string mediaDir = (fs::path(workspace_) / "uploads" / "avacli-media").string();
                        fs::create_directories(mediaDir);
                        std::string fname = "vid_" + generateUuid() + ".mp4";
                        std::string localPath = (fs::path(mediaDir) / fname).string();
                        if (downloadToFile(videoUrl, localPath)) {
                            nlohmann::json j;
                            j["status"] = "ok";
                            j["path"] = localPath;
                            j["url"] = "/uploads/avacli-media/" + fname;
                            return j.dump();
                        }
                    }
                    return R"({"error": "No request_id or video_url in response"})";
                }

                // Poll for completion
                std::string pollUrl = "https://api.x.ai/v1/videos/" + requestId;
                for (int attempt = 0; attempt < 60; ++attempt) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    auto pollResp = httpGet(pollUrl, apiKey);
                    if (!pollResp.ok()) continue;

                    auto pollResult = nlohmann::json::parse(pollResp.body);
                    std::string status = pollResult.value("status", "");
                    if (status == "done") {
                        std::string videoUrl;
                        if (pollResult.contains("video") && pollResult["video"].contains("url"))
                            videoUrl = pollResult["video"]["url"].get<std::string>();
                        if (!videoUrl.empty()) {
                            std::string mediaDir = (fs::path(workspace_) / "uploads" / "avacli-media").string();
                            fs::create_directories(mediaDir);
                            std::string fname = "vid_" + generateUuid() + ".mp4";
                            std::string localPath = (fs::path(mediaDir) / fname).string();
                            if (downloadToFile(videoUrl, localPath)) {
                                nlohmann::json j;
                                j["status"] = "ok";
                                j["path"] = localPath;
                                j["url"] = "/uploads/avacli-media/" + fname;
                                j["request_id"] = requestId;
                                return j.dump();
                            }
                        }
                        return R"({"error": "Video completed but no download URL found"})";
                    } else if (status == "failed" || status == "expired") {
                        nlohmann::json j;
                        j["error"] = "Video generation failed: " + pollResult.value("error", "unknown");
                        j["request_id"] = requestId;
                        return j.dump();
                    }
                    // Still pending, continue polling
                }
                return R"({"error": "Video generation timed out after 5 minutes"})";
            } catch (const std::exception& e) {
                return R"({"error": "Failed to parse response: )" + std::string(e.what()) + "\"}";
            }
        }

        // ── edit_video ────────────────────────────────────
        if (name == "edit_video") {
            std::string prompt = get("prompt");
            std::string videoSrc = get("video", "");
            std::string model = get("model", "grok-imagine-video");
            if (prompt.empty()) return R"({"error": "prompt required"})";
            if (videoSrc.empty()) return R"({"error": "video URL or path required"})";

            std::string apiKey = resolvedXaiApiKey();
            if (apiKey.empty()) return R"({"error": "XAI_API_KEY not set"})";

            nlohmann::json body;
            body["model"] = model;
            body["prompt"] = prompt;
            if (videoSrc.find("http") == 0) {
                body["video"] = {{"url", videoSrc}};
            } else {
                std::string resolved = resolvePath(workspace_, videoSrc);
                if (!fs::exists(resolved)) return R"({"error": "Video not found: )" + resolved + "\"}";
                std::string b64 = readFileToBase64(resolved);
                if (b64.empty()) return R"({"error": "Failed to read video file"})";
                body["video"] = {{"url", "data:video/mp4;base64," + b64}};
            }

            auto submitResp = httpPostJson("https://api.x.ai/v1/videos/edits", body.dump(), apiKey);
            if (!submitResp.ok()) {
                nlohmann::json j;
                j["error"] = "API error (" + std::to_string(submitResp.statusCode) + "): " + submitResp.body.substr(0, 500);
                return j.dump();
            }

            try {
                auto submitResult = nlohmann::json::parse(submitResp.body);
                std::string requestId = submitResult.value("request_id", "");
                if (requestId.empty()) return R"({"error": "No request_id in response"})";

                std::string pollUrl = "https://api.x.ai/v1/videos/" + requestId;
                for (int attempt = 0; attempt < 60; ++attempt) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    auto pollResp = httpGet(pollUrl, apiKey);
                    if (!pollResp.ok()) continue;
                    auto pollResult = nlohmann::json::parse(pollResp.body);
                    std::string status = pollResult.value("status", "");
                    if (status == "done") {
                        std::string videoUrl;
                        if (pollResult.contains("video") && pollResult["video"].contains("url"))
                            videoUrl = pollResult["video"]["url"].get<std::string>();
                        if (!videoUrl.empty()) {
                            std::string mediaDir = (fs::path(workspace_) / "uploads" / "avacli-media").string();
                            fs::create_directories(mediaDir);
                            std::string fname = "edited_" + generateUuid() + ".mp4";
                            std::string localPath = (fs::path(mediaDir) / fname).string();
                            if (downloadToFile(videoUrl, localPath))
                                return nlohmann::json({{"status","ok"},{"path",localPath},{"url","/uploads/avacli-media/"+fname},{"request_id",requestId}}).dump();
                        }
                        return R"({"error": "Video edit completed but no download URL"})";
                    } else if (status == "failed" || status == "expired") {
                        return nlohmann::json({{"error","Video edit failed: "+pollResult.value("error","unknown")},{"request_id",requestId}}).dump();
                    }
                }
                return R"({"error": "Video edit timed out after 5 minutes"})";
            } catch (const std::exception& e) {
                return R"({"error": "Failed to parse response: )" + std::string(e.what()) + "\"}";
            }
        }

        // ── extend_video ────────────────────────────────────
        if (name == "extend_video") {
            std::string prompt = get("prompt");
            std::string videoSrc = get("video", "");
            std::string model = get("model", "grok-imagine-video");
            int duration = getInt("duration", 5);
            if (prompt.empty()) return R"({"error": "prompt required"})";
            if (videoSrc.empty()) return R"({"error": "video URL or path required"})";

            std::string apiKey = resolvedXaiApiKey();
            if (apiKey.empty()) return R"({"error": "XAI_API_KEY not set"})";

            nlohmann::json body;
            body["model"] = model;
            body["prompt"] = prompt;
            body["duration"] = duration;
            if (videoSrc.find("http") == 0) {
                body["video"]["url"] = videoSrc;
            } else {
                std::string resolved = resolvePath(workspace_, videoSrc);
                if (!fs::exists(resolved)) return R"({"error": "Video not found: )" + resolved + "\"}";
                std::string b64 = readFileToBase64(resolved);
                if (b64.empty()) return R"({"error": "Failed to read video file"})";
                body["video"]["url"] = "data:video/mp4;base64," + b64;
            }

            auto submitResp = httpPostJson("https://api.x.ai/v1/videos/extensions", body.dump(), apiKey);
            if (!submitResp.ok()) {
                nlohmann::json j;
                j["error"] = "API error (" + std::to_string(submitResp.statusCode) + "): " + submitResp.body.substr(0, 500);
                return j.dump();
            }

            try {
                auto submitResult = nlohmann::json::parse(submitResp.body);
                std::string requestId = submitResult.value("request_id", "");
                if (requestId.empty()) return R"({"error": "No request_id in response"})";

                std::string pollUrl = "https://api.x.ai/v1/videos/" + requestId;
                for (int attempt = 0; attempt < 60; ++attempt) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    auto pollResp = httpGet(pollUrl, apiKey);
                    if (!pollResp.ok()) continue;
                    auto pollResult = nlohmann::json::parse(pollResp.body);
                    std::string status = pollResult.value("status", "");
                    if (status == "done") {
                        std::string videoUrl;
                        if (pollResult.contains("video") && pollResult["video"].contains("url"))
                            videoUrl = pollResult["video"]["url"].get<std::string>();
                        if (!videoUrl.empty()) {
                            std::string mediaDir = (fs::path(workspace_) / "uploads" / "avacli-media").string();
                            fs::create_directories(mediaDir);
                            std::string fname = "ext_" + generateUuid() + ".mp4";
                            std::string localPath = (fs::path(mediaDir) / fname).string();
                            if (downloadToFile(videoUrl, localPath))
                                return nlohmann::json({{"status","ok"},{"path",localPath},{"url","/uploads/avacli-media/"+fname},{"request_id",requestId}}).dump();
                        }
                        return R"({"error": "Video extension completed but no download URL"})";
                    } else if (status == "failed" || status == "expired") {
                        return nlohmann::json({{"error","Video extension failed: "+pollResult.value("error","unknown")},{"request_id",requestId}}).dump();
                    }
                }
                return R"({"error": "Video extension timed out after 5 minutes"})";
            } catch (const std::exception& e) {
                return R"({"error": "Failed to parse response: )" + std::string(e.what()) + "\"}";
            }
        }

        // ── search_assets ────────────────────────────────────
        if (name == "search_assets") {
            std::string query = get("query");
            std::string filterType = get("type", "");
            if (query.empty()) return R"({"error": "query required"})";

            std::string queryLower = query;
            std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);

            fs::path mediaDir = fs::path(workspace_) / "uploads" / "avacli-media";
            fs::path metaDir = fs::path(workspace_) / "uploads" / "asset-meta";

            auto classifyExt = [](const std::string& ext) -> std::string {
                if (ext == ".mp4" || ext == ".webm" || ext == ".mov") return "video";
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp") return "image";
                if (ext == ".pdf" || ext == ".doc" || ext == ".docx" || ext == ".txt" || ext == ".md" ||
                    ext == ".html" || ext == ".xml" || ext == ".csv" || ext == ".json" ||
                    ext == ".yaml" || ext == ".yml" || ext == ".toml" || ext == ".ini") return "document";
                if (ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".tsx" || ext == ".jsx" ||
                    ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" || ext == ".rs" ||
                    ext == ".go" || ext == ".java" || ext == ".rb" || ext == ".sh" || ext == ".css" ||
                    ext == ".scss" || ext == ".sql" || ext == ".lua" || ext == ".zig" ||
                    ext == ".swift" || ext == ".kt" || ext == ".cs") return "code";
                return "";
            };

            nlohmann::json results = nlohmann::json::array();
            if (!fs::exists(mediaDir)) return nlohmann::json({{"results", results}}).dump();

            std::function<void(const fs::path&, const std::string&)> scanDir;
            scanDir = [&](const fs::path& dir, const std::string& prefix) {
                for (const auto& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
                    try {
                        if (entry.is_directory()) {
                            std::string sub = prefix.empty() ? entry.path().filename().string() : prefix + "/" + entry.path().filename().string();
                            scanDir(entry.path(), sub);
                            continue;
                        }
                        if (!entry.is_regular_file()) continue;
                        std::string fname = entry.path().filename().string();
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        std::string type = classifyExt(ext);
                        if (type.empty()) continue;
                        if (!filterType.empty() && filterType != type) continue;

                        std::string relName = prefix.empty() ? fname : prefix + "/" + fname;

                        // Load metadata
                        nlohmann::json meta;
                        std::string metaPath = (metaDir / (relName + ".json")).string();
                        if (!fs::exists(metaPath)) metaPath = (metaDir / (fname + ".json")).string();
                        if (fs::exists(metaPath)) {
                            try { std::ifstream f(metaPath); meta = nlohmann::json::parse(f); } catch (...) {}
                        }

                        std::string fnameLower = fname;
                        std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
                        bool match = fnameLower.find(queryLower) != std::string::npos;
                        if (!match && meta.contains("tags") && meta["tags"].is_array()) {
                            for (const auto& tag : meta["tags"]) {
                                std::string t = tag.get<std::string>();
                                std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                                if (t.find(queryLower) != std::string::npos) { match = true; break; }
                            }
                        }
                        if (!match && meta.contains("description") && meta["description"].is_string()) {
                            std::string d = meta["description"].get<std::string>();
                            std::transform(d.begin(), d.end(), d.begin(), ::tolower);
                            if (d.find(queryLower) != std::string::npos) match = true;
                        }
                        if (!match) continue;

                        nlohmann::json r;
                        r["filename"] = fname;
                        r["url"] = "/uploads/avacli-media/" + relName;
                        r["type"] = type;
                        r["path"] = entry.path().string();
                        if (meta.contains("description")) r["description"] = meta["description"];
                        if (meta.contains("tags")) r["tags"] = meta["tags"];
                        results.push_back(r);
                    } catch (...) {}
                }
            };
            scanDir(mediaDir, "");

            nlohmann::json out;
            out["results"] = results;
            out["count"] = results.size();
            out["query"] = query;
            return out.dump();
        }

        // ── summarize_and_new_chat ────────────────────────────
        if (name == "summarize_and_new_chat") {
            std::string summary = get("summary");
            std::string continuationPrompt = get("continuation_prompt");
            if (summary.empty()) return R"({"error": "summary required"})";
            if (continuationPrompt.empty()) return R"({"error": "continuation_prompt required"})";

            std::string sessionName = "continued_" + generateUuid();
            fs::path sessDir = fs::path(workspace_) / ".avacli" / "sessions";
            fs::create_directories(sessDir);

            nlohmann::json sessData;
            sessData["messages"] = nlohmann::json::array({
                {{"role", "user"}, {"content", "## Conversation Summary (continued from previous session)\n\n" + summary + "\n\n---\n\n" + continuationPrompt}}
            });

            std::ofstream f((sessDir / (sessionName + ".json")).string());
            f << sessData.dump(2);

            nlohmann::json result;
            result["status"] = "ok";
            result["session"] = sessionName;
            result["message"] = "New session created with conversation summary. The user should switch to this session to continue.";
            return result.dump();
        }

        // ── create_tool ────────────────────────────────────
        if (name == "create_tool") {
            std::string toolName = get("name");
            std::string description = get("description");
            std::string script = get("script");
            if (toolName.empty() || script.empty())
                return R"({"error": "name and script required"})";
            if (toolName.find("custom_") != 0) toolName = "custom_" + toolName;

            // Build parameter schema from the parameters array
            nlohmann::json paramSchema;
            paramSchema["type"] = "object";
            paramSchema["properties"] = nlohmann::json::object();
            paramSchema["required"] = nlohmann::json::array();
            if (args.contains("parameters") && args["parameters"].is_array()) {
                for (const auto& p : args["parameters"]) {
                    std::string pname = p.value("name", "");
                    if (pname.empty()) continue;
                    paramSchema["properties"][pname] = {
                        {"type", p.value("type", "string")},
                        {"description", p.value("description", "")}
                    };
                    if (p.value("required", false))
                        paramSchema["required"].push_back(pname);
                }
            }

            std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
            fs::create_directories(toolsDir);

            std::string scriptPath = (fs::path(toolsDir) / (toolName + ".py")).string();
            { std::ofstream ofs(scriptPath); ofs << script; }

            nlohmann::json meta;
            meta["name"] = toolName;
            meta["description"] = description;
            meta["parameters"] = paramSchema;
            meta["script_path"] = scriptPath;
            meta["created_by"] = "ai";
            meta["created_at"] = currentTimestamp();
            std::string metaPath = (fs::path(toolsDir) / (toolName + ".json")).string();
            { std::ofstream ofs(metaPath); ofs << meta.dump(2); }

            nlohmann::json toolDef;
            toolDef["type"] = "function";
            toolDef["function"]["name"] = toolName;
            toolDef["function"]["description"] = description;
            toolDef["function"]["parameters"] = paramSchema;
            ToolRegistry::registerCustomTool(toolDef);

            nlohmann::json j;
            j["status"] = "ok";
            j["name"] = toolName;
            j["description"] = description;
            j["script_path"] = scriptPath;
            j["note"] = "Tool is now available for use in this and future sessions.";
            return j.dump();
        }

        // ── modify_tool ────────────────────────────────────
        if (name == "modify_tool") {
            std::string toolName = get("name");
            if (toolName.empty()) return R"({"error": "name required"})";
            if (!ToolRegistry::isCustomTool(toolName))
                return R"({"error": "Can only modify custom tools, not built-in"})";

            std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
            std::string metaPath = (fs::path(toolsDir) / (toolName + ".json")).string();
            std::string scriptPath = (fs::path(toolsDir) / (toolName + ".py")).string();

            nlohmann::json meta;
            if (fs::exists(metaPath)) {
                std::ifstream ifs(metaPath);
                try { meta = nlohmann::json::parse(ifs); } catch (...) {}
            }

            std::string newDesc = get("description");
            std::string newScript = get("script");
            if (!newDesc.empty()) meta["description"] = newDesc;
            if (!newScript.empty()) {
                if (fs::exists(scriptPath)) {
                    std::ifstream oldFile(scriptPath);
                    std::string oldContent((std::istreambuf_iterator<char>(oldFile)), std::istreambuf_iterator<char>());
                    saveUndoEntry(scriptPath, oldContent);
                }
                std::ofstream ofs(scriptPath);
                ofs << newScript;
            }
            meta["updated_at"] = currentTimestamp();
            { std::ofstream ofs(metaPath); ofs << meta.dump(2); }

            // Re-register
            nlohmann::json toolDef;
            toolDef["type"] = "function";
            toolDef["function"]["name"] = toolName;
            toolDef["function"]["description"] = meta.value("description", "");
            toolDef["function"]["parameters"] = meta.value("parameters", nlohmann::json::object());
            ToolRegistry::registerCustomTool(toolDef);

            nlohmann::json j;
            j["status"] = "ok";
            j["name"] = toolName;
            j["updated"] = true;
            return j.dump();
        }

        // ── delete_tool ────────────────────────────────────
        if (name == "delete_tool") {
            std::string toolName = get("name");
            if (toolName.empty()) return R"({"error": "name required"})";
            if (!ToolRegistry::isCustomTool(toolName))
                return R"({"error": "Can only delete custom tools, not built-in"})";

            ToolRegistry::removeCustomTool(toolName);
            std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
            try { fs::remove(fs::path(toolsDir) / (toolName + ".py")); } catch (...) {}
            try { fs::remove(fs::path(toolsDir) / (toolName + ".json")); } catch (...) {}

            nlohmann::json j;
            j["status"] = "ok";
            j["name"] = toolName;
            j["deleted"] = true;
            return j.dump();
        }

        // ── list_custom_tools ──────────────────────────────
        if (name == "list_custom_tools") {
            auto allTools = ToolRegistry::getAllTools();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : allTools) {
                std::string tname = t["function"]["name"];
                if (!ToolRegistry::isCustomTool(tname)) continue;
                nlohmann::json entry;
                entry["name"] = tname;
                entry["description"] = t["function"].value("description", "");
                entry["parameters"] = t["function"].value("parameters", nlohmann::json::object());
                arr.push_back(entry);
            }
            nlohmann::json j;
            j["tools"] = arr;
            j["count"] = arr.size();
            return j.dump();
        }

        // ── research_api ──────────────────────────────────
        if (name == "research_api") {
            std::string query = get("query");
            std::string focus = get("focus", "full");
            if (query.empty()) return R"({"error": "query required"})";

            std::string searchQuery = "API documentation " + query;
            if (focus == "auth") searchQuery += " authentication setup API key";
            else if (focus == "endpoints") searchQuery += " REST endpoints available";
            else if (focus == "quickstart") searchQuery += " quickstart getting started";

            std::string instructions =
                "Research the following API and return a structured JSON response. "
                "Include: base_url, auth_type (bearer/api_key_header/api_key_query/basic/custom), "
                "auth_config (header_name, token_prefix if applicable), "
                "endpoints (array of {method, path, description, params}), "
                "rate_limits, documentation_url, and a brief summary. "
                "Respond with ONLY a JSON object, no markdown fences.\n\nAPI to research: " + query;

            return runXaiLiveSearch(resolvedXaiApiKey(), searchModel_, instructions, "web_search", 10);
        }

        // ── setup_api ─────────────────────────────────────
        if (name == "setup_api") {
            std::string slug = get("slug");
            std::string apiName = get("name");
            std::string baseUrl = get("base_url");
            std::string authType = get("auth_type");
            if (slug.empty() || baseUrl.empty())
                return R"({"error": "slug and base_url required"})";

            ApiRegistryEntry entry;
            entry.slug = slug;
            entry.name = apiName;
            entry.base_url = baseUrl;
            entry.auth_type = authType;
            entry.auth_config = args.value("auth_config", nlohmann::json::object());
            entry.vault_key_name = get("vault_key_name");
            entry.endpoints = args.value("endpoints", nlohmann::json::array());
            entry.documentation_url = get("documentation_url");
            entry.research_notes = get("research_notes");
            entry.status = "configured";

            VaultStorage::saveApiRegistry(slug, entry);

            nlohmann::json j;
            j["status"] = "ok";
            j["slug"] = slug;
            j["name"] = apiName;
            j["base_url"] = baseUrl;
            j["endpoints_count"] = entry.endpoints.size();
            j["note"] = "API registered. Use call_api with slug '" + slug + "' to make requests.";
            if (entry.vault_key_name.empty())
                j["warning"] = "No vault_key_name set. Use vault_store to add the API key, then update with setup_api.";
            return j.dump();
        }

        // ── call_api ──────────────────────────────────────
        if (name == "call_api") {
            std::string slug = get("slug");
            std::string path = get("path");
            std::string method = get("method", "GET");
            if (slug.empty() || path.empty())
                return R"({"error": "slug and path required"})";

            auto regEntry = VaultStorage::getApiRegistry(slug);
            if (!regEntry)
                return R"({"error": "API not found in registry: )" + slug + "\"}";

            // Resolve the API key from vault
            std::string apiKey;
            if (!regEntry->vault_key_name.empty()) {
                std::string vp = vaultPassword_;
                if (vp.empty()) vp = "avacli_default";
                auto key = VaultStorage::retrieve(vp, regEntry->vault_key_name);
                if (key) apiKey = *key;
                else return R"({"error": "Could not decrypt vault key: )" + regEntry->vault_key_name + "\"}";
            }

            std::string fullUrl = regEntry->base_url;
            if (!fullUrl.empty() && fullUrl.back() == '/' && !path.empty() && path.front() == '/')
                fullUrl.pop_back();
            fullUrl += path;

            // Add query params
            if (args.contains("query_params") && args["query_params"].is_object()) {
                std::string qs;
                for (auto& [k, v] : args["query_params"].items()) {
                    if (!qs.empty()) qs += "&";
                    qs += k + "=" + (v.is_string() ? v.get<std::string>() : v.dump());
                }
                if (!qs.empty()) fullUrl += "?" + qs;
            }

            CURL* curl = curl_easy_init();
            if (!curl) return R"({"error": "Failed to initialize HTTP client"})";

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");

            // Apply auth
            if (!apiKey.empty()) {
                std::string authType = regEntry->auth_type;
                if (authType == "bearer") {
                    std::string prefix = regEntry->auth_config.value("token_prefix", "Bearer");
                    std::string authH = "Authorization: " + prefix + " " + apiKey;
                    headers = curl_slist_append(headers, authH.c_str());
                } else if (authType == "api_key_header") {
                    std::string headerName = regEntry->auth_config.value("header_name", "X-API-Key");
                    std::string authH = headerName + ": " + apiKey;
                    headers = curl_slist_append(headers, authH.c_str());
                } else if (authType == "api_key_query") {
                    std::string paramName = regEntry->auth_config.value("query_param", "api_key");
                    std::string sep = (fullUrl.find('?') == std::string::npos) ? "?" : "&";
                    fullUrl += sep + paramName + "=" + apiKey;
                }
            }

            // Additional headers
            if (args.contains("headers") && args["headers"].is_object()) {
                for (auto& [k, v] : args["headers"].items()) {
                    std::string h = k + ": " + (v.is_string() ? v.get<std::string>() : v.dump());
                    headers = curl_slist_append(headers, h.c_str());
                }
            }

            std::string responseBody;
            curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            // Set method
            std::string upperMethod;
            for (char c : method) upperMethod += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (upperMethod == "POST") {
                std::string bodyStr = args.contains("body") ? args["body"].dump() : "";
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
            } else if (upperMethod == "PUT") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                std::string bodyStr = args.contains("body") ? args["body"].dump() : "";
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
            } else if (upperMethod == "PATCH") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
                std::string bodyStr = args.contains("body") ? args["body"].dump() : "";
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
            } else if (upperMethod == "DELETE") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            }

            CURLcode curlRes = curl_easy_perform(curl);
            long httpCode = 0;
            if (curlRes == CURLE_OK)
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            if (curlRes != CURLE_OK) {
                nlohmann::json j;
                j["error"] = "HTTP request failed: " + std::string(curl_easy_strerror(curlRes));
                return j.dump();
            }

            nlohmann::json j;
            j["status_code"] = httpCode;
            j["url"] = fullUrl;
            j["method"] = upperMethod;
            // Try to parse response as JSON
            try {
                j["response"] = nlohmann::json::parse(responseBody);
            } catch (...) {
                if (responseBody.size() > 4000)
                    responseBody = responseBody.substr(0, 4000) + "... (truncated)";
                j["response"] = responseBody;
            }
            return j.dump();
        }

        // ── list_api_registry ──────────────────────────────
        if (name == "list_api_registry") {
            auto entries = VaultStorage::listApiRegistry();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& e : entries) {
                nlohmann::json entry;
                entry["slug"] = e.slug;
                entry["name"] = e.name;
                entry["base_url"] = e.base_url;
                entry["auth_type"] = e.auth_type;
                entry["vault_key"] = e.vault_key_name;
                entry["status"] = e.status;
                entry["endpoints_count"] = e.endpoints.size();
                arr.push_back(entry);
            }
            nlohmann::json j;
            j["apis"] = arr;
            j["count"] = arr.size();
            return j.dump();
        }

        // ── vault_store ────────────────────────────────────
        if (name == "vault_store") {
            std::string vname = get("name");
            std::string service = get("service", "");
            std::string value = get("value");
            if (vname.empty() || value.empty())
                return R"({"error": "name and value required"})";

            std::string vp = vaultPassword_;
            if (vp.empty()) vp = "avacli_default";
            VaultStorage::initialize(vp);
            bool ok = VaultStorage::store(vp, vname, service, value);

            nlohmann::json j;
            j["status"] = ok ? "ok" : "error";
            j["name"] = vname;
            if (!ok) j["error"] = "Failed to encrypt and store";
            return j.dump();
        }

        // ── vault_list ─────────────────────────────────────
        if (name == "vault_list") {
            auto entries = VaultStorage::listEntries();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& e : entries) {
                arr.push_back({
                    {"name", e.name},
                    {"service", e.service},
                    {"created_at", e.created_at},
                    {"updated_at", e.updated_at}
                });
            }
            nlohmann::json j;
            j["entries"] = arr;
            j["count"] = arr.size();
            return j.dump();
        }

        // ── vault_retrieve ─────────────────────────────────
        if (name == "vault_retrieve") {
            std::string vname = get("name");
            if (vname.empty()) return R"({"error": "name required"})";

            std::string vp = vaultPassword_;
            if (vp.empty()) vp = "avacli_default";
            auto val = VaultStorage::retrieve(vp, vname);

            nlohmann::json j;
            if (val) {
                j["status"] = "ok";
                j["name"] = vname;
                j["value"] = *val;
            } else {
                j["error"] = "Key not found or decryption failed";
            }
            return j.dump();
        }

        // ── vault_remove ───────────────────────────────────
        if (name == "vault_remove") {
            std::string vname = get("name");
            if (vname.empty()) return R"({"error": "name required"})";

            std::string vp = vaultPassword_;
            if (vp.empty()) vp = "avacli_default";
            bool ok = VaultStorage::remove(vp, vname);

            nlohmann::json j;
            j["status"] = ok ? "ok" : "not_found";
            j["name"] = vname;
            return j.dump();
        }

    } catch (const std::exception& e) {
        return R"({"error": ")" + std::string(e.what()) + "\"}";
    }

    return R"({"error": "Tool not implemented: )" + name + "\"}";
}

} // namespace avacli
