#pragma once

#include "core/Types.hpp"
#include "core/TokenTracker.hpp"
#include <string>
#include <vector>

namespace avacli {

struct SessionData {
    std::vector<Message> messages;
    Mode mode = Mode::Question;
    size_t totalPromptTokens = 0;
    size_t totalCompletionTokens = 0;
    std::vector<std::string> projectNotes;
    std::vector<TodoItem> todos;
};

class SessionManager {
public:
    explicit SessionManager(const std::string& sessionsDir = defaultSessionsDir());

    bool save(const std::string& name, const SessionData& data);
    bool load(const std::string& name, SessionData& data);
    bool exists(const std::string& name) const;
    std::string sessionPath(const std::string& name) const;

    static std::string defaultSessionsDir();

private:
    std::string baseDir_;
};

} // namespace avacli
