#pragma once

#include "core/Types.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace avacli {

/// Registers tools with OpenAI-format JSON schemas.
/// Filters available tools by Mode (Question, Plan, Agent).
class ToolRegistry {
public:
    /// Register all built-in tools. Call once at startup.
    static void registerAll();

    /// Return tool definitions for the API, filtered by mode.
    static std::vector<nlohmann::json> getToolsForMode(Mode mode);

    /// Check if a tool name is registered for the given mode.
    static bool hasTool(const std::string& name, Mode mode);

    /// Return list of all registered tool definitions (built-in + custom).
    static std::vector<nlohmann::json> getAllTools();

    /// Register a custom Python tool from its JSON definition.
    static void registerCustomTool(const nlohmann::json& toolDef);

    /// Remove a custom tool by name.
    static void removeCustomTool(const std::string& name);

    /// Check if a tool is custom (not built-in).
    static bool isCustomTool(const std::string& name);

    /// Convert tools from chat completions format to Responses API format.
    /// Flattens {"type":"function","function":{...}} to {"type":"function","name":...,"parameters":...}
    /// and prepends server-side tools (web_search, x_search) while removing their function equivalents.
    static std::vector<nlohmann::json> toResponsesApiFormat(const std::vector<nlohmann::json>& chatTools);
};

} // namespace avacli
