#include "tools/ProjectNotes.hpp"
#include "platform/Paths.hpp"
#include "platform/PortableTime.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <random>

namespace avacli {

namespace fs = std::filesystem;

namespace {

std::string generateId() {
    static std::mt19937 rng(std::random_device{}());
    static const char chars[] = "0123456789abcdef";
    std::string id;
    id.reserve(12);
    for (int i = 0; i < 12; ++i) id += chars[rng() % 16];
    return id;
}

std::string nowTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    utcTmFromTime(t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto toLower = [](const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return r;
    };
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

} // namespace

// ── Session storage ──────────────────────────────────────

std::string sessionStorageDir(const std::string& sessionId) {
    if (sessionId.empty()) return "";
    return (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "sessions" / sessionId).string();
}

void ensureSessionStorage(const std::string& sessionId) {
    if (sessionId.empty()) return;
    fs::create_directories(sessionStorageDir(sessionId));
}

// ── Session todos ────────────────────────────────────────

void loadSessionTodos(const std::string& sessionId, std::vector<TodoItem>& out) {
    out.clear();
    if (sessionId.empty()) return;
    std::string path = sessionStorageDir(sessionId) + "/todos.json";
    if (!fs::exists(path)) return;
    try {
        std::ifstream f(path);
        if (!f) return;
        auto j = nlohmann::json::parse(f);
        if (!j.is_array()) return;
        for (const auto& t : j) {
            TodoItem item;
            item.id = t.value("id", "");
            item.title = t.value("title", "");
            item.status = t.value("status", "pending");
            item.description = t.value("description", "");
            out.push_back(std::move(item));
        }
    } catch (...) {}
}

void saveSessionTodos(const std::string& sessionId, const std::vector<TodoItem>& todos) {
    if (sessionId.empty()) return;
    ensureSessionStorage(sessionId);
    std::string path = sessionStorageDir(sessionId) + "/todos.json";
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : todos)
        arr.push_back({{"id", t.id}, {"title", t.title}, {"status", t.status}, {"description", t.description}});
    std::ofstream f(path);
    if (f) f << arr.dump(2);
}

void pruneDoneTodos(std::vector<TodoItem>& todos, size_t maxActive) {
    std::vector<TodoItem> active;
    for (auto& t : todos) {
        if (t.status != "done") active.push_back(std::move(t));
    }
    if (active.size() > maxActive)
        active.erase(active.begin(), active.begin() + (active.size() - maxActive));
    todos = std::move(active);
}

// ── Memory (JSONL journal) ───────────────────────────────

void addMemoryEntry(const std::string& sessionId, const MemoryEntry& entry) {
    if (sessionId.empty()) return;
    ensureSessionStorage(sessionId);
    std::string path = sessionStorageDir(sessionId) + "/memory.jsonl";
    nlohmann::json j;
    j["id"] = entry.id.empty() ? generateId() : entry.id;
    j["ts"] = entry.timestamp.empty() ? nowTimestamp() : entry.timestamp;
    j["category"] = entry.category;
    j["content"] = entry.content;
    j["tags"] = entry.tags;
    j["pinned"] = entry.pinned;
    std::ofstream f(path, std::ios::app);
    if (f) f << j.dump() << "\n";
}

std::vector<MemoryEntry> loadAllMemory(const std::string& sessionId) {
    std::vector<MemoryEntry> result;
    if (sessionId.empty()) return result;
    std::string path = sessionStorageDir(sessionId) + "/memory.jsonl";
    if (!fs::exists(path)) return result;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            MemoryEntry e;
            e.id = j.value("id", "");
            e.timestamp = j.value("ts", "");
            e.category = j.value("category", "");
            e.content = j.value("content", "");
            if (j.contains("tags") && j["tags"].is_array())
                for (const auto& t : j["tags"])
                    if (t.is_string()) e.tags.push_back(t.get<std::string>());
            e.pinned = j.value("pinned", false);
            result.push_back(std::move(e));
        } catch (...) {}
    }
    return result;
}

std::vector<MemoryEntry> searchMemory(const std::string& sessionId,
                                       const std::string& query,
                                       const std::string& category) {
    auto all = loadAllMemory(sessionId);
    std::vector<MemoryEntry> results;
    for (const auto& e : all) {
        if (!category.empty() && e.category != category) continue;
        if (!query.empty()) {
            bool match = containsCI(e.content, query);
            if (!match)
                for (const auto& tag : e.tags)
                    if (containsCI(tag, query)) { match = true; break; }
            if (!match && containsCI(e.category, query)) match = true;
            if (!match) continue;
        }
        results.push_back(e);
    }
    return results;
}

bool forgetMemory(const std::string& sessionId, const std::string& entryId) {
    if (sessionId.empty() || entryId.empty()) return false;
    auto all = loadAllMemory(sessionId);
    bool found = false;
    std::string path = sessionStorageDir(sessionId) + "/memory.jsonl";
    std::ofstream f(path, std::ios::trunc);
    for (const auto& e : all) {
        if (e.id == entryId) { found = true; continue; }
        nlohmann::json j;
        j["id"] = e.id; j["ts"] = e.timestamp; j["category"] = e.category;
        j["content"] = e.content; j["tags"] = e.tags; j["pinned"] = e.pinned;
        f << j.dump() << "\n";
    }
    return found;
}

// ── Edit history ─────────────────────────────────────────

void addEditRecord(const std::string& sessionId, const EditRecord& record) {
    if (sessionId.empty()) return;
    ensureSessionStorage(sessionId);
    std::string path = sessionStorageDir(sessionId) + "/edits.jsonl";
    nlohmann::json j;
    j["file"] = record.file;
    j["action"] = record.action;
    j["ts"] = record.timestamp.empty() ? nowTimestamp() : record.timestamp;
    j["summary"] = record.summary;
    std::ofstream f(path, std::ios::app);
    if (f) f << j.dump() << "\n";
}

std::vector<EditRecord> loadEditHistory(const std::string& sessionId, size_t maxEntries) {
    std::vector<EditRecord> result;
    if (sessionId.empty()) return result;
    std::string path = sessionStorageDir(sessionId) + "/edits.jsonl";
    if (!fs::exists(path)) return result;
    std::ifstream f(path);
    std::string line;
    std::vector<EditRecord> all;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            EditRecord r;
            r.file = j.value("file", "");
            r.action = j.value("action", "");
            r.timestamp = j.value("ts", "");
            r.summary = j.value("summary", "");
            all.push_back(std::move(r));
        } catch (...) {}
    }
    if (all.size() > maxEntries)
        result.assign(all.end() - maxEntries, all.end());
    else
        result = std::move(all);
    return result;
}

// ── Workspace notes (read-only project reference) ────────

void ensureProjectNotesExist(const std::string& workspace) {
    std::string path = (fs::path(workspace) / "GROK_NOTES.md").string();
    if (fs::exists(path)) return;
    fs::create_directories(fs::path(path).parent_path());
    const char* content = R"(# Project Notes

## Architecture

(Describe the overall architecture.)

## Key Decisions

(Important decisions and rationale.)

## Gotchas

(Common pitfalls, edge cases, things to watch out for.)

## Setup Instructions

(How to build, run, configure.)

## Integration Notes

(External systems, APIs, dependencies.)

## Future Work

(Planned improvements, TODOs.)

)";
    std::ofstream f(path);
    if (f) f << content;
}

// ── Legacy (backward compat for SessionManager) ──────────

std::string todosFilePath(const std::string& workspace) {
    return (fs::path(workspace) / "GROK_TODOS.json").string();
}

void ensureTodosExist(const std::string& workspace) {
    std::string path = todosFilePath(workspace);
    if (fs::exists(path)) return;
    fs::create_directories(fs::path(path).parent_path());
    nlohmann::json j = nlohmann::json::array();
    std::ofstream f(path);
    if (f) f << j.dump(2);
}

void loadTodosFromFile(const std::string& workspace, std::vector<TodoItem>& out) {
    out.clear();
    std::string path = todosFilePath(workspace);
    if (!fs::exists(path)) return;
    try {
        std::ifstream f(path);
        if (!f) return;
        auto j = nlohmann::json::parse(f);
        if (!j.is_array()) return;
        for (const auto& t : j) {
            TodoItem item;
            item.id = t.value("id", "");
            item.title = t.value("title", "");
            item.status = t.value("status", "pending");
            item.description = t.value("description", "");
            out.push_back(std::move(item));
        }
    } catch (...) {}
}

void saveTodosToFile(const std::string& workspace, const std::vector<TodoItem>& todos) {
    std::string path = todosFilePath(workspace);
    fs::create_directories(fs::path(path).parent_path());
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : todos)
        arr.push_back({{"id", t.id}, {"title", t.title}, {"status", t.status}, {"description", t.description}});
    std::ofstream f(path);
    if (f) f << arr.dump(2);
}

} // namespace avacli
