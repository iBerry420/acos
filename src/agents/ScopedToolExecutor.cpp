#include "agents/ScopedToolExecutor.hpp"
#include "agents/LeaseManager.hpp"
#include "platform/Paths.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <mutex>

namespace avacli {

namespace fs = std::filesystem;

namespace {

std::string jsonErr(const std::string& code, const std::string& msg,
                    const nlohmann::json& extra = {}) {
    nlohmann::json j;
    j["error"] = code;
    j["message"] = msg;
    if (!extra.is_null()) {
        for (auto it = extra.begin(); it != extra.end(); ++it) {
            j[it.key()] = it.value();
        }
    }
    return j.dump();
}

/// Canonicalise a path relative to workspace. We must NOT require the file
/// to exist — write_file creates new files. So we canonicalise the parent
/// directory and append the basename, falling back to `lexically_normal`
/// when even the parent is absent.
std::string canonicalizeForLease(const std::string& workspace,
                                 const std::string& raw) {
    fs::path p(raw);
    if (p.is_relative()) {
        p = fs::path(workspace) / p;
    }
    std::error_code ec;
    fs::path parent = p.parent_path();
    fs::path canonParent;
    if (!parent.empty() && fs::exists(parent, ec)) {
        canonParent = fs::canonical(parent, ec);
    }
    if (!canonParent.empty() && !ec) {
        return (canonParent / p.filename()).string();
    }
    return p.lexically_normal().string();
}

/// Does `abs` fall under at least one of `allowedPrefixes`?
/// Each prefix is canonicalised if it exists; otherwise compared as-is.
bool pathAllowed(const std::string& abs,
                 const std::vector<std::string>& allowedPrefixes) {
    if (allowedPrefixes.empty()) return true;
    std::error_code ec;
    fs::path target(abs);
    fs::path targetNorm = target.lexically_normal();
    for (const auto& raw : allowedPrefixes) {
        fs::path pfx(raw);
        fs::path pfxCanon;
        if (fs::exists(pfx, ec)) {
            pfxCanon = fs::canonical(pfx, ec);
        }
        if (pfxCanon.empty()) pfxCanon = pfx.lexically_normal();

        auto pfxStr = pfxCanon.string();
        auto tgtStr = targetNorm.string();
        // Ensure prefix match is at a directory boundary (no "/foo" matching
        // "/foobar"). Also allow exact-match equality.
        if (tgtStr == pfxStr) return true;
        if (tgtStr.size() > pfxStr.size() &&
            tgtStr.rfind(pfxStr, 0) == 0 &&
            (pfxStr.back() == '/' || tgtStr[pfxStr.size()] == '/')) {
            return true;
        }
    }
    return false;
}

/// Extract the target path from a write-tool's arguments. Returns "" if the
/// args don't parse or the tool doesn't carry a known path field.
std::string extractTargetPath(const std::string& toolName,
                              const std::string& argumentsJson) {
    nlohmann::json j;
    try { j = nlohmann::json::parse(argumentsJson); }
    catch (...) { return ""; }
    // All Phase-4 gated write tools use "path" today. Kept extensible.
    (void)toolName;
    if (j.contains("path") && j["path"].is_string()) {
        return j["path"].get<std::string>();
    }
    return "";
}

} // anonymous namespace

const std::unordered_set<std::string>& ScopedToolExecutor::writeToolNames() {
    static const std::unordered_set<std::string> kWrites = {
        "write_file",
        "edit_file",
        "insert_line",
        "append_file",
        "delete_file",
    };
    return kWrites;
}

ScopedToolExecutor::ScopedToolExecutor(const std::string& workspace,
                                       Mode mode,
                                       Scope scope,
                                       AskUserFn askUser)
    : ToolExecutor(workspace, mode, std::move(askUser)),
      scope_(std::move(scope)) {}

std::string ScopedToolExecutor::execute(const std::string& name,
                                        const std::string& arguments) {
    // ── 1. allowed_tools ────────────────────────────────────────────────
    if (!scope_.allowedTools.empty()) {
        bool ok = false;
        for (const auto& t : scope_.allowedTools) {
            if (t == name) { ok = true; break; }
        }
        if (!ok) {
            nlohmann::json extra;
            extra["name"] = name;
            extra["allowed"] = scope_.allowedTools;
            return jsonErr("tool_not_allowed",
                           "sub-agent is not permitted to call '" + name + "'",
                           extra);
        }
    }

    const auto& writes = writeToolNames();
    const bool isWrite = writes.count(name) > 0;

    // ── 2 + 3. Path gate + lease (writes only) ──────────────────────────
    if (isWrite) {
        std::string target = extractTargetPath(name, arguments);
        if (target.empty()) {
            // Unknown/missing path — refuse rather than silently bypass.
            nlohmann::json extra;
            extra["tool"] = name;
            return jsonErr("path_required",
                           "write tool requires a 'path' field", extra);
        }

        // Canonicalise relative to our workspace (base class holds it, but
        // we only need the accessor; we'll use the raw arg + our own
        // canon helper to keep symmetry with what the base executor does
        // at dispatch time).
        //
        // We intentionally use lexically_normal for the canonical form
        // rather than requiring the file to exist, so write_file of a
        // brand-new path is gateable.
        fs::path raw(target);
        std::string absForPolicy;
        if (raw.is_absolute()) {
            absForPolicy = fs::path(target).lexically_normal().string();
        } else {
            // Best-effort: prefix with workspace. The base executor will do
            // the real resolve.
            absForPolicy = canonicalizeForLease("" /*wkspc unused here*/, target);
            // If we got back a relative path (no workspace / file not yet
            // created), fall back to pretending the raw target is the key
            // — two sub-agents targeting the same relative path against
            // the same workspace will still collide on the lease key.
            if (!fs::path(absForPolicy).is_absolute()) {
                absForPolicy = raw.lexically_normal().string();
            }
        }

        // allowed_paths gate
        if (!pathAllowed(absForPolicy, scope_.allowedPaths)) {
            nlohmann::json extra;
            extra["path"] = absForPolicy;
            extra["allowed_paths"] = scope_.allowedPaths;
            return jsonErr("path_not_allowed",
                           "write is outside sub-agent's allowed_paths",
                           extra);
        }

        // Lease acquire
        const std::string key = "path:" + absForPolicy;
        auto acq = LeaseManager::acquire(scope_.taskId, key,
                                         scope_.writeLeaseTtlSeconds);
        if (!acq.acquired) {
            nlohmann::json extra;
            extra["resource"] = key;
            extra["holder"] = acq.holderTaskId;
            extra["expires_at"] = acq.existingExpiresAt;
            extra["tool"] = name;
            return jsonErr("resource_locked",
                           "another sub-agent holds the lease on this resource",
                           extra);
        }

        // Delegate, then release regardless of outcome.
        std::string result;
        try {
            result = ToolExecutor::execute(name, arguments);
        } catch (...) {
            LeaseManager::release(scope_.taskId, key);
            throw;
        }
        LeaseManager::release(scope_.taskId, key);
        return result;
    }

    return ToolExecutor::execute(name, arguments);
}

} // namespace avacli
