#include "session/SessionManager.hpp"
#include "platform/Paths.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>

namespace avacli {

namespace fs = std::filesystem;

std::string SessionManager::defaultSessionsDir() {
    std::string home = userHomeDirectory();
    if (home.empty())
        return (fs::path(".avacli") / "sessions").string();
    return (fs::path(home) / ".avacli" / "sessions").string();
}

SessionManager::SessionManager(const std::string& sessionsDir) : baseDir_(sessionsDir) {
    try {
        fs::create_directories(baseDir_);
    } catch (const fs::filesystem_error& e) {
        spdlog::warn("SessionManager: Could not create sessions dir {}: {}", baseDir_, e.what());
    }
}

std::string SessionManager::sessionPath(const std::string& name) const {
    return (fs::path(baseDir_) / name / "session.json").string();
}

bool SessionManager::exists(const std::string& name) const {
    return fs::exists(sessionPath(name));
}

bool SessionManager::save(const std::string& name, const SessionData& data) {
    try {
        std::string dir = (fs::path(baseDir_) / name).string();
        fs::create_directories(dir);

        nlohmann::json j;
        j["mode"] = modeToString(data.mode);
        j["total_prompt_tokens"] = data.totalPromptTokens;
        j["total_completion_tokens"] = data.totalCompletionTokens;

        auto msgs = nlohmann::json::array();
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
            msgs.push_back(msg);
        }
        j["messages"] = msgs;

        j["project_notes"] = data.projectNotes;
        auto todosArr = nlohmann::json::array();
        for (const auto& t : data.todos) {
            todosArr.push_back({{"id", t.id}, {"title", t.title}, {"status", t.status}, {"description", t.description}});
        }
        j["todos"] = todosArr;

        std::string path = sessionPath(name);
        std::ofstream f(path);
        if (!f) {
            spdlog::error("SessionManager: Cannot open {} for writing", path);
            return false;
        }
        f << j.dump(2);
        spdlog::info("SessionManager: Saved session {}", name);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("SessionManager: Save failed: {}", e.what());
        return false;
    }
}

bool SessionManager::load(const std::string& name, SessionData& data) {
    try {
        std::string path = sessionPath(name);
        if (!fs::exists(path)) {
            spdlog::warn("SessionManager: Session file not found: {}", path);
            return false;
        }

        std::ifstream f(path);
        if (!f) {
            spdlog::error("SessionManager: Cannot open {} for reading", path);
            return false;
        }

        nlohmann::json j = nlohmann::json::parse(f);

        data.mode = j.contains("mode") ? modeFromString(j["mode"].get<std::string>()) : Mode::Question;
        data.totalPromptTokens = j.value("total_prompt_tokens", 0ULL);
        data.totalCompletionTokens = j.value("total_completion_tokens", 0ULL);

        data.messages.clear();
        if (j.contains("messages")) {
            for (const auto& m : j["messages"]) {
                Message msg;
                msg.role = m.value("role", "");
                msg.content = m.value("content", "");
                if (m.contains("tool_call_id") && m["tool_call_id"].is_string())
                    msg.tool_call_id = m["tool_call_id"].get<std::string>();
                if (m.contains("tool_calls") && m["tool_calls"].is_array()) {
                    for (const auto& tc : m["tool_calls"]) {
                        ToolCall tcall;
                        tcall.id = tc.value("id", "");
                        tcall.name = tc.value("name", "");
                        tcall.arguments = tc.value("arguments", "");
                        msg.tool_calls.push_back(tcall);
                    }
                }
                data.messages.push_back(std::move(msg));
            }
        }

        data.projectNotes.clear();
        if (j.contains("project_notes"))
            for (const auto& n : j["project_notes"])
                data.projectNotes.push_back(n.get<std::string>());

        data.todos.clear();
        if (j.contains("todos") && j["todos"].is_array()) {
            for (const auto& t : j["todos"]) {
                TodoItem item;
                item.id = t.value("id", "");
                item.title = t.value("title", "");
                item.status = t.value("status", "pending");
                item.description = t.value("description", "");
                data.todos.push_back(std::move(item));
            }
        }

        spdlog::info("SessionManager: Loaded session {}", name);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("SessionManager: Load failed: {}", e.what());
        return false;
    }
}

} // namespace avacli
