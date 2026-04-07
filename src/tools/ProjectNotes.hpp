#pragma once

#include "core/Types.hpp"
#include <string>
#include <vector>

namespace avacli {

// ── Session-scoped storage ────────────────────────────────
std::string sessionStorageDir(const std::string& sessionId);
void ensureSessionStorage(const std::string& sessionId);

// ── Todos (session-scoped with auto-pruning) ──────────────
void loadSessionTodos(const std::string& sessionId, std::vector<TodoItem>& out);
void saveSessionTodos(const std::string& sessionId, const std::vector<TodoItem>& todos);
void pruneDoneTodos(std::vector<TodoItem>& todos, size_t maxActive = 20);

// ── Memory (session-scoped JSONL journal) ─────────────────
void addMemoryEntry(const std::string& sessionId, const MemoryEntry& entry);
std::vector<MemoryEntry> loadAllMemory(const std::string& sessionId);
std::vector<MemoryEntry> searchMemory(const std::string& sessionId,
                                       const std::string& query,
                                       const std::string& category = "");
bool forgetMemory(const std::string& sessionId, const std::string& entryId);

// ── Edit history (session-scoped) ─────────────────────────
void addEditRecord(const std::string& sessionId, const EditRecord& record);
std::vector<EditRecord> loadEditHistory(const std::string& sessionId, size_t maxEntries = 30);

// ── Workspace notes (read-only project reference) ─────────
void ensureProjectNotesExist(const std::string& workspace);

// ── Legacy (backward compat for SessionManager) ───────────
void ensureTodosExist(const std::string& workspace);
std::string todosFilePath(const std::string& workspace);
void loadTodosFromFile(const std::string& workspace, std::vector<TodoItem>& out);
void saveTodosToFile(const std::string& workspace, const std::vector<TodoItem>& todos);

} // namespace avacli
